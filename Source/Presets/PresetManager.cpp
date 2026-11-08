#include "PresetManager.h"

#include "Core/ParameterIds.h"
#include "FactoryPresets.h"

namespace vm
{
    namespace
    {
        const juce::Identifier presetNameProperty { "presetName" };
        const juce::Identifier presetFactoryProperty { "presetIsFactory" };
        constexpr int presetFileVersion = 1;

        // The bypass switch belongs to the host/session, not to the sound of a preset.
        bool isExcluded (const juce::String& id) { return id == param::masterBypass; }

        juce::RangedAudioParameter* asRanged (juce::AudioProcessorParameter* p)
        {
            return dynamic_cast<juce::RangedAudioParameter*> (p);
        }
    }

    PresetManager::PresetManager (juce::AudioProcessor& owner, juce::AudioProcessorValueTreeState& state)
        : processor (owner), apvts (state)
    {
        // A fresh instance is the default state; a restored project keeps the name it was saved with.
        if (! apvts.state.hasProperty (presetNameProperty))
            setCurrent (presets::factoryPresets[0].name, true);

        refresh();
        rememberBaseline();
    }

    juce::File PresetManager::getUserPresetDirectory()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Vintage Maker")
                   .getChildFile ("Presets");
    }

    juce::File PresetManager::fileForName (const juce::String& name)
    {
        return getUserPresetDirectory().getChildFile (juce::File::createLegalFileName (name.trim()) + fileExtension);
    }

    void PresetManager::refresh()
    {
        entries.clear();

        for (int i = 0; i < (int) presets::factoryPresets.size(); ++i)
        {
            Entry e;
            e.name = presets::factoryPresets[(size_t) i].name;
            e.factory = true;
            e.factoryIndex = i;
            entries.push_back (e);
        }

        auto directory = getUserPresetDirectory();
        if (! directory.isDirectory())
            return;

        auto files = directory.findChildFiles (juce::File::findFiles, false, juce::String ("*") + fileExtension);
        files.sort();

        for (const auto& file : files)
        {
            Entry e;
            e.name = file.getFileNameWithoutExtension();
            e.factory = false;
            e.file = file;
            entries.push_back (e);
        }
    }

    juce::String PresetManager::getCurrentName() const
    {
        return apvts.state.getProperty (presetNameProperty, presets::factoryPresets[0].name).toString();
    }

    bool PresetManager::isCurrentFactory() const
    {
        return (bool) apvts.state.getProperty (presetFactoryProperty, true);
    }

    int PresetManager::getCurrentIndex() const
    {
        const auto name = getCurrentName();
        const bool factory = isCurrentFactory();

        for (int i = 0; i < (int) entries.size(); ++i)
            if (entries[(size_t) i].factory == factory && entries[(size_t) i].name == name)
                return i;

        return -1;
    }

    void PresetManager::setCurrent (const juce::String& name, bool factory)
    {
        apvts.state.setProperty (presetNameProperty, name, nullptr);
        apvts.state.setProperty (presetFactoryProperty, factory, nullptr);
    }

    void PresetManager::rememberBaseline()
    {
        baseline.clear();

        for (auto* p : processor.getParameters())
            if (auto* r = asRanged (p); r != nullptr && ! isExcluded (r->paramID))
                baseline.push_back (r->getValue());
    }

    bool PresetManager::isModified() const
    {
        size_t i = 0;

        for (auto* p : processor.getParameters())
        {
            auto* r = asRanged (p);
            if (r == nullptr || isExcluded (r->paramID))
                continue;

            if (i >= baseline.size() || std::abs (baseline[i] - r->getValue()) > 1.0e-4f)
                return true;

            ++i;
        }

        return false;
    }

    void PresetManager::applyValues (const Values& values)
    {
        for (auto* p : processor.getParameters())
        {
            auto* r = asRanged (p);
            if (r == nullptr || isExcluded (r->paramID))
                continue;

            // Anything a preset does not mention goes back to its default.
            float target = r->convertFrom0to1 (r->getDefaultValue());
            for (const auto& [id, value] : values)
                if (id == r->paramID)
                {
                    target = value;
                    break;
                }

            const auto& range = r->getNormalisableRange();
            const float legal = range.snapToLegalValue (juce::jlimit (range.start, range.end, target));

            r->beginChangeGesture();
            r->setValueNotifyingHost (r->convertTo0to1 (legal));
            r->endChangeGesture();
        }
    }

    PresetManager::Values PresetManager::captureValues() const
    {
        Values values;

        for (auto* p : processor.getParameters())
            if (auto* r = asRanged (p); r != nullptr && ! isExcluded (r->paramID))
                values.emplace_back (r->paramID, r->convertFrom0to1 (r->getValue()));

        return values;
    }

    void PresetManager::loadEntry (int index)
    {
        if (index < 0 || index >= (int) entries.size())
            return;

        const auto entry = entries[(size_t) index]; // copy: refresh() may run meanwhile

        if (entry.factory)
        {
            const auto& preset = presets::factoryPresets[(size_t) entry.factoryIndex];

            Values values;
            for (size_t i = 0; i < preset.count; ++i)
                values.emplace_back (preset.values[i].id, preset.values[i].value);

            applyValues (values);
            setCurrent (entry.name, true);
            rememberBaseline();
        }
        else
        {
            loadFile (entry.file);
        }
    }

    void PresetManager::loadNext()
    {
        refresh();
        if (entries.empty())
            return;

        const int current = getCurrentIndex();
        loadEntry (current < 0 ? 0 : (current + 1) % (int) entries.size());
    }

    void PresetManager::loadPrevious()
    {
        refresh();
        if (entries.empty())
            return;

        const int current = getCurrentIndex();
        const int count = (int) entries.size();
        loadEntry (current < 0 ? count - 1 : (current + count - 1) % count);
    }

    juce::Result PresetManager::loadFile (const juce::File& file)
    {
        const auto xml = juce::parseXML (file);

        if (xml == nullptr || ! xml->hasTagName ("VintageMakerPreset"))
            return juce::Result::fail ("This is not a Vintage Maker preset.");

        if (xml->getIntAttribute ("version", 1) > presetFileVersion)
            return juce::Result::fail ("This preset was saved by a newer version of Vintage Maker.");

        Values values;
        for (auto* child : xml->getChildWithTagNameIterator ("PARAM"))
        {
            const auto id = child->getStringAttribute ("id");
            const double value = child->getDoubleAttribute ("value", std::nan (""));

            if (id.isNotEmpty() && std::isfinite (value))
                values.emplace_back (id, (float) value);
        }

        applyValues (values);

        const auto name = xml->getStringAttribute ("name", file.getFileNameWithoutExtension());
        setCurrent (name.isNotEmpty() ? name : file.getFileNameWithoutExtension(), false);
        refresh();
        rememberBaseline();
        return juce::Result::ok();
    }

    bool PresetManager::isFactoryName (const juce::String& name) const
    {
        for (const auto& preset : presets::factoryPresets)
            if (name.trim().equalsIgnoreCase (preset.name))
                return true;

        return false;
    }

    bool PresetManager::userPresetExists (const juce::String& name) const
    {
        return fileForName (name).existsAsFile();
    }

    juce::Result PresetManager::saveAs (const juce::String& rawName, bool overwrite)
    {
        const auto name = rawName.trim();

        if (name.isEmpty() || juce::File::createLegalFileName (name).isEmpty())
            return juce::Result::fail ("Type a name for the preset.");

        if (isFactoryName (name))
            return juce::Result::fail ("That name belongs to a factory preset. Choose another name.");

        const auto file = fileForName (name);

        if (file.existsAsFile() && ! overwrite)
            return juce::Result::fail ("A preset with that name already exists.");

        if (! file.getParentDirectory().createDirectory())
            return juce::Result::fail ("Could not create the presets folder.");

        juce::XmlElement xml ("VintageMakerPreset");
        xml.setAttribute ("version", presetFileVersion);
        xml.setAttribute ("name", name);

        for (const auto& [id, value] : captureValues())
        {
            auto* child = xml.createNewChildElement ("PARAM");
            child->setAttribute ("id", id);
            child->setAttribute ("value", (double) value);
        }

        // Write to a temporary file first so a failed write never destroys an existing preset.
        const auto temporary = file.getSiblingFile (file.getFileName() + ".tmp");
        if (! xml.writeTo (temporary) || ! temporary.moveFileTo (file))
        {
            temporary.deleteFile();
            return juce::Result::fail ("Could not write the preset file.");
        }

        setCurrent (name, false);
        refresh();
        rememberBaseline();
        return juce::Result::ok();
    }

    void PresetManager::resetAllToDefaults()
    {
        applyValues ({});
        setCurrent (presets::factoryPresets[0].name, true);
        rememberBaseline();
    }
}
