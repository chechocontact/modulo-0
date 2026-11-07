#pragma once

#include "Core/MasterProcessor.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>

namespace vm
{
    // Owns the host-facing parameter set (APVTS) and translates it into the
    // JUCE-free Core parameter structs. The UI must change parameters only
    // through the APVTS, never by touching processor state (spec §9).
    class ParameterManager
    {
    public:
        explicit ParameterManager (juce::AudioProcessor& owner);

        juce::AudioProcessorValueTreeState& getState() noexcept { return state; }
        juce::AudioProcessorParameter* getBypassParameter() const noexcept { return bypassParameter; }

        // Audio thread. Lock-free: reads cached std::atomic<float> pointers.
        MasterParameters readMasterParameters() const noexcept;

        // Message thread (or any thread the host chooses; uses the APVTS lock).
        void saveState (juce::MemoryBlock& destination);
        void loadState (const void* data, int sizeInBytes);

        static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    private:
        juce::AudioProcessorValueTreeState state;

        std::atomic<float>* inputGain = nullptr;
        std::atomic<float>* outputGain = nullptr;
        std::atomic<float>* masterMix = nullptr;
        std::atomic<float>* masterBypass = nullptr;
        juce::AudioProcessorParameter* bypassParameter = nullptr;
    };
}
