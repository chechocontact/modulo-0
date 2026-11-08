#pragma once

#include "HostContextMenu.h"

#include <memory>

namespace vm::ui
{
    // Round lamp that switches a module on and off (the circle of the design guide).
    class LedToggle : public juce::Button
    {
    public:
        LedToggle (juce::AudioProcessorValueTreeState& state, const char* parameterId);

        void paintButton (juce::Graphics&, bool highlighted, bool down) override;
        void mouseDown (const juce::MouseEvent&) override;

    private:
        juce::RangedAudioParameter* parameter;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    };

    // Push switch with a lamp and a caption (Sync, Bypass).
    class PushToggle : public juce::Button
    {
    public:
        PushToggle (juce::AudioProcessorValueTreeState& state, const char* parameterId, juce::String captionText);

        void paintButton (juce::Graphics&, bool highlighted, bool down) override;
        void mouseDown (const juce::MouseEvent&) override;

    private:
        static constexpr int textHeight = 30;

        juce::RangedAudioParameter* parameter;
        juce::String caption;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    };
}
