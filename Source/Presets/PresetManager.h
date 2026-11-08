#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <vector>

namespace vm
{
    // Presets of the plugin: factory presets (compiled in, see FactoryPresets.h) and user
    // presets (".vmpreset" files in the user's folder, which updates and reinstalls never touch).
    //
    // Loading a preset only ever goes through the host parameters (each change is a proper
    // begin/end gesture), so the host sees it as automation-capable parameter changes.
    // The current preset name is stored in the plugin state, so it survives project save/load.
    //
    // Message thread only.
    class PresetManager
    {
    public:
        struct Entry
        {
            juce::String name;
            bool factory = true;
            int factoryIndex = -1;   // index in vm::presets::factoryPresets
            juce::File file;         // user presets only
        };

        PresetManager (juce::AudioProcessor& owner, juce::AudioProcessorValueTreeState& state);

        // %APPDATA%\Vintage Maker\Presets on Windows.
        static juce::File getUserPresetDirectory();
        static constexpr const char* fileExtension = ".vmpreset";

        // Rescans the user folder. Factory presets come first, then user presets by name.
        void refresh();
        const std::vector<Entry>& getEntries() const noexcept { return entries; }

        juce::String getCurrentName() const;
        bool isCurrentFactory() const;
        int getCurrentIndex() const;            // -1 if the current name is not in the list
        bool isModified() const;                // parameters differ from the last load/save

        void loadEntry (int index);
        void loadNext();
        void loadPrevious();
        juce::Result loadFile (const juce::File& file);

        // Saves the current parameters as a user preset called `name` in the user folder.
        // Fails for an existing file unless overwrite is true, and for a factory name.
        juce::Result saveAs (const juce::String& name, bool overwrite);
        bool userPresetExists (const juce::String& name) const;
        bool isFactoryName (const juce::String& name) const;
        static juce::File fileForName (const juce::String& name);

        void resetAllToDefaults();

    private:
        using Values = std::vector<std::pair<juce::String, float>>;

        void applyValues (const Values& values);
        Values captureValues() const;
        void setCurrent (const juce::String& name, bool factory);
        void rememberBaseline();

        juce::AudioProcessor& processor;
        juce::AudioProcessorValueTreeState& apvts;
        std::vector<Entry> entries;
        std::vector<float> baseline;
    };
}
