#include "PluginProcessor.h"
#include "DSP/Test/TestProcessor.h"

#include <array>

VintageMakerAudioProcessor::VintageMakerAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this)
{
    // Modules 1-3: Texture, Color and Movement are real. Space holds the
    // transparent TestProcessor until it is implemented.
    using Slot = vm::AudioEngine::Slot;
    auto& engine = master.getEngine();

    auto textureModule = std::make_unique<vm::TextureProcessor>();
    texture = textureModule.get();
    engine.setModule (Slot::texture, std::move (textureModule));

    auto colorModule = std::make_unique<vm::ColorProcessor>();
    color = colorModule.get();
    engine.setModule (Slot::color, std::move (colorModule));

    auto movementProcessor = std::make_unique<vm::MovementProcessor>();
    movementModule = movementProcessor.get();
    engine.setModule (Slot::movement, std::move (movementProcessor));

    engine.setModule (Slot::space, std::make_unique<vm::TestProcessor>());
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

    master.prepare (spec);

    // Start from the current parameter values, without ramping from defaults.
    master.setParameters (parameters.readMasterParameters());
    applyModuleParameters();
    master.reset();

    setLatencySamples (master.getLatencySamples());
}

void VintageMakerAudioProcessor::releaseResources()
{
}

void VintageMakerAudioProcessor::reset()
{
    master.reset();
}

void VintageMakerAudioProcessor::applyModuleParameters() noexcept
{
    const auto slot = parameters.readTextureSlot();
    auto& textureSlot = master.getEngine().getSlot (vm::AudioEngine::Slot::texture);
    textureSlot.setEnabled (slot.enabled);
    textureSlot.setMix (slot.mix);

    texture->setParameters (parameters.readTextureParameters());

    const auto colorState = parameters.readColorSlot();
    auto& colorSlot = master.getEngine().getSlot (vm::AudioEngine::Slot::color);
    colorSlot.setEnabled (colorState.enabled);
    colorSlot.setMix (colorState.mix);

    color->setParameters (parameters.readColorParameters());

    const auto movementState = parameters.readMovementSlot();
    auto& movementSlot = master.getEngine().getSlot (vm::AudioEngine::Slot::movement);
    movementSlot.setEnabled (movementState.enabled);
    movementSlot.setMix (movementState.mix);

    movementModule->setParameters (parameters.readMovementParameters());
}

void VintageMakerAudioProcessor::processWithCore (juce::AudioBuffer<float>& buffer,
                                                  const vm::MasterParameters& p) noexcept
{
    const int numInputs = getTotalNumInputChannels();
    const int numOutputs = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (int ch = numInputs; ch < numOutputs; ++ch)
        buffer.clear (ch, 0, numSamples);

    master.setParameters (p);
    applyModuleParameters();

    const int numChannels = juce::jmin (buffer.getNumChannels(), vm::maxChannels);
    master.process (vm::AudioBufferView (buffer.getArrayOfWritePointers(), numChannels, numSamples));
}

void VintageMakerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    processWithCore (buffer, parameters.readMasterParameters());
}

// Host-side bypass (the DAW's own bypass button) goes through the same
// click-free crossfade as the plugin's Bypass parameter.
void VintageMakerAudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto p = parameters.readMasterParameters();
    p.bypass = true;
    processWithCore (buffer, p);
}

juce::AudioProcessorParameter* VintageMakerAudioProcessor::getBypassParameter() const
{
    return parameters.getBypassParameter();
}

juce::AudioProcessorEditor* VintageMakerAudioProcessor::createEditor()
{
    // Provisional: the definitive UI is Module 7.
    return new juce::GenericAudioProcessorEditor (*this);
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
