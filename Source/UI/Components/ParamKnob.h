#pragma once

#include "HostContextMenu.h"

#include <memory>

namespace vm::ui
{
    // A rotary knob bound to one parameter, with its caption and value readout.
    class ParamKnob : public juce::Component
    {
    public:
        // compact: small knob on the right with the caption and value on its left (used for the
        // Dry/Wet knob in the header of each module).
        ParamKnob (juce::AudioProcessorValueTreeState& state, const char* parameterId, juce::String captionText,
                   bool compactLayout = false);

        void resized() override;
        void paint (juce::Graphics&) override;

    private:
        static constexpr int textHeight = 30;

        ParameterSlider slider;
        juce::String caption;
        bool compact = false;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
}
