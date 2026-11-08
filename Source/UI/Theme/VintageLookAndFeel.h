#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace vm::ui
{
    // Draws the rotary knobs, the IN/OUT sliders and the buttons of the editor.
    class VintageLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        VintageLookAndFeel();

        void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                               float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;

        void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                               float minSliderPos, float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;

        int getSliderThumbRadius (juce::Slider&) override { return 10; }

        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
        void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;

        juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    };
}
