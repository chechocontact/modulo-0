#pragma once

#include "Chain/VintageChain.h"
#include "Plugin/ParameterManager.h"

#include <juce_audio_processors/juce_audio_processors.h>

// Thin host adapter: buses, parameters and state on the JUCE side, all audio
// work delegated to the JUCE-free vm::VintageChain.
class VintageMakerAudioProcessor final : public juce::AudioProcessor
{
public:
    VintageMakerAudioProcessor();
    ~VintageMakerAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    using AudioProcessor::processBlock;          // double-precision overloads stay unused
    using AudioProcessor::processBlockBypassed;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Maps the DAW's own bypass button to our click-free masterBypass parameter.
    juce::AudioProcessorParameter* getBypassParameter() const override;

    vm::ParameterManager& getParameterManager() noexcept { return parameters; }

    // Level meters for the interface (lock-free reads).
    const vm::LevelMeter& getInputMeter() const noexcept { return chain.getInputMeter(); }
    const vm::LevelMeter& getOutputMeter() const noexcept { return chain.getOutputMeter(); }

private:
    void processWithCore (juce::AudioBuffer<float>& buffer, bool forceBypass) noexcept;
    vm::ChainParameters readParameters (bool forceBypass) noexcept;

    vm::ParameterManager parameters;
    vm::VintageChain chain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VintageMakerAudioProcessor)
};
