#pragma once

#include "HostContextMenu.h"

#include "Core/LevelMeter.h"

#include <memory>

namespace vm::ui
{
    // The IN / OUT control of the design guide: a horizontal gain slider, a small stereo
    // meter and a readout of the current peak level. The slider is the only control that
    // stays a slider (every other control is a knob).
    class LevelSlider : public juce::Component
    {
    public:
        LevelSlider (juce::AudioProcessorValueTreeState& state, const char* parameterId,
                     juce::String labelText, const vm::LevelMeter& meterToShow);

        // Message thread, called by the editor's timer.
        void refresh();

        void resized() override;
        void paint (juce::Graphics&) override;

    private:
        const vm::LevelMeter& meter;
        ParameterSlider slider;
        juce::String label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

        juce::Rectangle<int> meterArea, readoutArea;
        float peakDb = -100.0f;
        float barDb[2] { -100.0f, -100.0f };
    };
}
