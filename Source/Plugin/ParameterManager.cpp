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
