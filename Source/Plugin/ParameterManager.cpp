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

        std::atomic<float>* requireParameter (juce::AudioProcessorValueTreeState& s, const char* id)
        {
            auto* p = s.getRawParameterValue (id);
            jassert (p != nullptr); // every ID in ParameterIds.h must be in createLayout()
            return p;
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
