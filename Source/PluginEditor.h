#pragma once

#include "PluginProcessor.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

// The Vintage Maker interface (Module 7). The editor only talks to the host
// parameters through the APVTS attachments of its controls; it reads the level
// meters of the processor for display and holds no audio state.
class VintageMakerEditor final : public juce::AudioProcessorEditor,
                                 private juce::Timer
{
public:
    explicit VintageMakerEditor (VintageMakerAudioProcessor&);
    ~VintageMakerEditor() override;

    void resized() override;

private:
    class Content;

    void timerCallback() override;

    std::unique_ptr<Content> content;
};
