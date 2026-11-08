#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <array>

VintageMakerAudioProcessor::VintageMakerAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this)
{
}

bool VintageMakerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    const auto& in = layouts.getMainInputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return in == out;
}

void VintageMakerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    vm::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maxBlockSize = juce::jmax (1, samplesPerBlock);
    spec.numChannels = juce::jlimit (1, vm::maxChannels, getTotalNumOutputChannels());

    chain.prepare (spec, readParameters (false));
    setLatencySamples (chain.getLatencySamples());
}

void VintageMakerAudioProcessor::releaseResources()
{
}

void VintageMakerAudioProcessor::reset()
{
    chain.reset();
}

vm::ChainParameters VintageMakerAudioProcessor::readParameters (bool forceBypass) noexcept
{
    auto p = parameters.readChainParameters();
    p.master.bypass = p.master.bypass || forceBypass;

    // Host tempo for the delay's Sync (the default tempo is used when the host gives none).
    if (auto* hostPlayHead = getPlayHead())
        if (auto position = hostPlayHead->getPosition())
            if (auto bpm = position->getBpm())
                p.tempoBpm = *bpm;

    return p;
}

void VintageMakerAudioProcessor::processWithCore (juce::AudioBuffer<float>& buffer, bool forceBypass) noexcept
{
    const int numInputs = getTotalNumInputChannels();
    const int numOutputs = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (int ch = numInputs; ch < numOutputs; ++ch)
        buffer.clear (ch, 0, numSamples);

    const int numChannels = juce::jmin (buffer.getNumChannels(), vm::maxChannels);
    chain.process (vm::AudioBufferView (buffer.getArrayOfWritePointers(), numChannels, numSamples),
                   readParameters (forceBypass));
}

void VintageMakerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    processWithCore (buffer, false);
}

// Host-side bypass (the DAW's own bypass button) goes through the same
// click-free crossfade as the plugin's Bypass parameter.
void VintageMakerAudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    processWithCore (buffer, true);
}

juce::AudioProcessorParameter* VintageMakerAudioProcessor::getBypassParameter() const
{
    return parameters.getBypassParameter();
}

juce::AudioProcessorEditor* VintageMakerAudioProcessor::createEditor()
{
    return new VintageMakerEditor (*this);
}

void VintageMakerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    parameters.saveState (destData);
}

void VintageMakerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    parameters.loadState (data, sizeInBytes);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VintageMakerAudioProcessor();
}
