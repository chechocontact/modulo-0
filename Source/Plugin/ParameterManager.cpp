#include "ParameterManager.h"
#include "Core/ParameterIds.h"

namespace vm
{
    namespace
    {
        const juce::Identifier stateType { "VintageMaker" };
        const juce::Identifier versionAttribute { "stateVersion" };

        juce::ParameterID makeId (const char* id) { return { id, param::idVersion }; }

        juce::String formatDb (float value, int)
        {
            return (value > 0.0f ? "+" : "") + juce::String (value, 1) + " dB";
        }

        juce::String formatPercent (float value, int)
        {
            return juce::String (juce::roundToInt (value)) + " %";
        }

        juce::String formatHz (float value, int)
        {
            return juce::String (value, value < 10.0f ? 2 : 1) + " Hz";
        }

        std::atomic<float>* requireParameter (juce::AudioProcessorValueTreeState& s, const char* id)
        {
            auto* p = s.getRawParameterValue (id);
            jassert (p != nullptr); // every ID in ParameterIds.h must be in createLayout()
            return p;
        }
    }

    namespace
    {
        void addPercent (juce::AudioProcessorValueTreeState::ParameterLayout& layout,
                         const char* id, const char* name, float defaultPercent)
        {
            layout.add (std::make_unique<juce::AudioParameterFloat> (
                makeId (id), name, juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), defaultPercent,
                juce::AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (formatPercent)));
        }

        void addHz (juce::AudioProcessorValueTreeState::ParameterLayout& layout,
                    const char* id, const char* name, float low, float high, float defaultHz)
        {
            layout.add (std::make_unique<juce::AudioParameterFloat> (
                makeId (id), name, juce::NormalisableRange<float> (low, high, 0.001f, 0.6f), defaultHz,
                juce::AudioParameterFloatAttributes().withLabel ("Hz").withStringFromValueFunction (formatHz)));
        }

        void addTextureParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout)
        {
            const TextureParameters d; // defaults live in one place: TextureParameters

            layout.add (std::make_unique<juce::AudioParameterBool> (makeId (param::texEnable), "Texture On", true));
            addPercent (layout, param::texMix, "Texture Dry/Wet", 100.0f);

            addPercent (layout, param::texWowDepth, "Wow Depth", d.wowDepth * 100.0f);
            addHz (layout, param::texWowRate, "Wow Rate", texture::wowRateMinHz, texture::wowRateMaxHz, d.wowRate);
            addPercent (layout, param::texWowShape, "Wow Shape", d.wowShape * 100.0f);

            addPercent (layout, param::texFlutterDepth, "Flutter Depth", d.flutterDepth * 100.0f);
            addHz (layout, param::texFlutterRate, "Flutter Rate", texture::flutterRateMinHz, texture::flutterRateMaxHz, d.flutterRate);
            addPercent (layout, param::texFlutterVariation, "Flutter Variation", d.flutterVariation * 100.0f);

            addPercent (layout, param::texVinylAmount, "Vinyl Amount", d.vinylAmount * 100.0f);
            layout.add (std::make_unique<juce::AudioParameterChoice> (
                makeId (param::texCrackleType), "Crackle Type",
                juce::StringArray { "Normal", "Dusty", "Heavy" }, (int) d.crackleType));
            addPercent (layout, param::texVinylDrop, "Vinyl Drop", d.vinylDrop * 100.0f);

            addPercent (layout, param::texDegradeAmount, "Degrade Amount", d.degradeAmount * 100.0f);
            addPercent (layout, param::texDegradeBandwidth, "Degrade Bandwidth", d.degradeBandwidth * 100.0f);
            addPercent (layout, param::texDegradeSmoothing, "Degrade Smoothing", d.degradeSmoothing * 100.0f);
        }
    }

    namespace
    {
        void addColorParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout)
        {
            const ColorParameters d; // defaults live in one place: ColorParameters

            layout.add (std::make_unique<juce::AudioParameterBool> (makeId (param::colEnable), "Color On", true));
            addPercent (layout, param::colMix, "Color Dry/Wet", 100.0f);

            addPercent (layout, param::colDrive, "Drive", d.drive * 100.0f);
            layout.add (std::make_unique<juce::AudioParameterChoice> (
                makeId (param::colType), "Color Type",
                juce::StringArray { "Tube", "Tape", "Console" }, (int) d.type));

            layout.add (std::make_unique<juce::AudioParameterFloat> (
                makeId (param::colTone), "Tone", juce::NormalisableRange<float> (-100.0f, 100.0f, 0.01f),
                d.tone * 100.0f,
                juce::AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (
                    [] (float v, int) { return (v > 0.0f ? "+" : "") + juce::String (juce::roundToInt (v)) + " %"; })));

            addHz (layout, param::colFocusLow, "Focus Low", color::focusLowMinHz, color::focusLowMaxHz, d.focusLow);
            addHz (layout, param::colFocusHigh, "Focus High", color::focusHighMinHz, color::focusHighMaxHz, d.focusHigh);
            addPercent (layout, param::colFocusAmount, "Focus", d.focusAmount * 100.0f);

            addPercent (layout, param::colChorusAmount, "Chorus Amount", d.chorusAmount * 100.0f);
            addHz (layout, param::colChorusRate, "Chorus Rate", color::chorusRateMinHz, color::chorusRateMaxHz, d.chorusRate);
            addPercent (layout, param::colChorusDepth, "Chorus Depth", d.chorusDepth * 100.0f);
            addPercent (layout, param::colChorusWidth, "Chorus Width", d.chorusWidth * 100.0f);
            addPercent (layout, param::colChorusMix, "Chorus Mix", d.chorusMix * 100.0f);
            layout.add (std::make_unique<juce::AudioParameterChoice> (
                makeId (param::colChorusMode), "Chorus Mode",
                juce::StringArray { "Classic", "Dimension", "Ensemble" }, (int) d.chorusMode));
        }
    }

    juce::AudioProcessorValueTreeState::ParameterLayout ParameterManager::createLayout()
    {
        using Attributes = juce::AudioParameterFloatAttributes;

        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        const juce::NormalisableRange<float> gainRange { param::gainMinDb, param::gainMaxDb, 0.01f };
        const juce::NormalisableRange<float> mixRange { param::mixMinPercent, param::mixMaxPercent, 0.01f };

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            makeId (param::inputGain), "Input Gain", gainRange, param::gainDefaultDb,
            Attributes().withLabel ("dB").withStringFromValueFunction (formatDb)));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            makeId (param::outputGain), "Output Gain", gainRange, param::gainDefaultDb,
            Attributes().withLabel ("dB").withStringFromValueFunction (formatDb)));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            makeId (param::masterMix), "Master Dry/Wet", mixRange, param::mixDefaultPercent,
            Attributes().withLabel ("%").withStringFromValueFunction (formatPercent)));

        layout.add (std::make_unique<juce::AudioParameterBool> (
            makeId (param::masterBypass), "Bypass", false));

        addTextureParameters (layout);
        addColorParameters (layout);

        return layout;
    }

    ParameterManager::ParameterManager (juce::AudioProcessor& owner)
        : state (owner, nullptr, stateType, createLayout())
    {
        inputGain = requireParameter (state, param::inputGain);
        outputGain = requireParameter (state, param::outputGain);
        masterMix = requireParameter (state, param::masterMix);
        masterBypass = requireParameter (state, param::masterBypass);
        bypassParameter = state.getParameter (param::masterBypass);

        tex.enable = requireParameter (state, param::texEnable);
        tex.mix = requireParameter (state, param::texMix);
        tex.wowDepth = requireParameter (state, param::texWowDepth);
        tex.wowRate = requireParameter (state, param::texWowRate);
        tex.wowShape = requireParameter (state, param::texWowShape);
        tex.flutterDepth = requireParameter (state, param::texFlutterDepth);
        tex.flutterRate = requireParameter (state, param::texFlutterRate);
        tex.flutterVariation = requireParameter (state, param::texFlutterVariation);
        tex.vinylAmount = requireParameter (state, param::texVinylAmount);
        tex.crackleType = requireParameter (state, param::texCrackleType);
        tex.vinylDrop = requireParameter (state, param::texVinylDrop);
        tex.degradeAmount = requireParameter (state, param::texDegradeAmount);
        tex.degradeBandwidth = requireParameter (state, param::texDegradeBandwidth);
        tex.degradeSmoothing = requireParameter (state, param::texDegradeSmoothing);

        col.enable = requireParameter (state, param::colEnable);
        col.mix = requireParameter (state, param::colMix);
        col.drive = requireParameter (state, param::colDrive);
        col.type = requireParameter (state, param::colType);
        col.tone = requireParameter (state, param::colTone);
        col.focusLow = requireParameter (state, param::colFocusLow);
        col.focusHigh = requireParameter (state, param::colFocusHigh);
        col.focusAmount = requireParameter (state, param::colFocusAmount);
        col.chorusAmount = requireParameter (state, param::colChorusAmount);
        col.chorusRate = requireParameter (state, param::colChorusRate);
        col.chorusDepth = requireParameter (state, param::colChorusDepth);
        col.chorusWidth = requireParameter (state, param::colChorusWidth);
        col.chorusMix = requireParameter (state, param::colChorusMix);
        col.chorusMode = requireParameter (state, param::colChorusMode);
    }

    ParameterManager::SlotState ParameterManager::readColorSlot() const noexcept
    {
        return { col.enable->load (std::memory_order_relaxed) >= 0.5f,
                 col.mix->load (std::memory_order_relaxed) / 100.0f };
    }

    ColorParameters ParameterManager::readColorParameters() const noexcept
    {
        const auto pct = [] (const std::atomic<float>* a) { return a->load (std::memory_order_relaxed) / 100.0f; };
        const auto raw = [] (const std::atomic<float>* a) { return a->load (std::memory_order_relaxed); };

        ColorParameters p;
        p.drive = pct (col.drive);
        p.type = (ColorType) juce::jlimit (0, 2, juce::roundToInt (raw (col.type)));
        p.tone = pct (col.tone);
        p.focusLow = raw (col.focusLow);
        p.focusHigh = raw (col.focusHigh);
        p.focusAmount = pct (col.focusAmount);
        p.chorusAmount = pct (col.chorusAmount);
        p.chorusRate = raw (col.chorusRate);
        p.chorusDepth = pct (col.chorusDepth);
        p.chorusWidth = pct (col.chorusWidth);
        p.chorusMix = pct (col.chorusMix);
        p.chorusMode = (ChorusMode) juce::jlimit (0, 2, juce::roundToInt (raw (col.chorusMode)));
        return p;
    }

    ParameterManager::SlotState ParameterManager::readTextureSlot() const noexcept
    {
        return { tex.enable->load (std::memory_order_relaxed) >= 0.5f,
                 tex.mix->load (std::memory_order_relaxed) / 100.0f };
    }

    TextureParameters ParameterManager::readTextureParameters() const noexcept
    {
        const auto pct = [] (const std::atomic<float>* a) { return a->load (std::memory_order_relaxed) / 100.0f; };
        const auto raw = [] (const std::atomic<float>* a) { return a->load (std::memory_order_relaxed); };

        TextureParameters p;
        p.wowDepth = pct (tex.wowDepth);
        p.wowRate = raw (tex.wowRate);
        p.wowShape = pct (tex.wowShape);
        p.flutterDepth = pct (tex.flutterDepth);
        p.flutterRate = raw (tex.flutterRate);
        p.flutterVariation = pct (tex.flutterVariation);
        p.vinylAmount = pct (tex.vinylAmount);
        p.crackleType = (CrackleType) juce::jlimit (0, 2, juce::roundToInt (raw (tex.crackleType)));
        p.vinylDrop = pct (tex.vinylDrop);
        p.degradeAmount = pct (tex.degradeAmount);
        p.degradeBandwidth = pct (tex.degradeBandwidth);
        p.degradeSmoothing = pct (tex.degradeSmoothing);
        return p;
    }

    MasterParameters ParameterManager::readMasterParameters() const noexcept
    {
        MasterParameters p;
        p.inputGainDb = inputGain->load (std::memory_order_relaxed);
        p.outputGainDb = outputGain->load (std::memory_order_relaxed);
        p.mix = masterMix->load (std::memory_order_relaxed) / param::mixMaxPercent;
        p.bypass = masterBypass->load (std::memory_order_relaxed) >= 0.5f;
        return p;
    }

    void ParameterManager::saveState (juce::MemoryBlock& destination)
    {
        auto tree = state.copyState();
        tree.setProperty (versionAttribute, param::stateVersion, nullptr);

        if (auto xml = tree.createXml())
            juce::AudioProcessor::copyXmlToBinary (*xml, destination);
    }

    void ParameterManager::loadState (const void* data, int sizeInBytes)
    {
        const auto xml = juce::AudioProcessor::getXmlFromBinary (data, sizeInBytes);

        if (xml == nullptr || ! xml->hasTagName (stateType))
            return; // not ours or corrupt: keep the current state

        const int version = xml->getIntAttribute (versionAttribute, 1);

        if (version > param::stateVersion)
            return; // saved by a newer Vintage Maker: refuse rather than misread

        // Missing parameters (older states) keep their defaults; replaceState
        // applies every parameter found in the tree.
        state.replaceState (juce::ValueTree::fromXml (*xml));
    }
}
