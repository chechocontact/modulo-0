#include "ParamKnob.h"

#include "../Theme/Palette.h"

namespace vm::ui
{
    ParamKnob::ParamKnob (juce::AudioProcessorValueTreeState& state, const char* parameterId, juce::String captionText, bool compactLayout)
        : slider (state.getParameter (parameterId)), caption (std::move (captionText)), compact (compactLayout)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        addAndMakeVisible (slider);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, parameterId, slider);
        slider.onValueChange = [this] { repaint(); };
    }

    void ParamKnob::resized()
    {
        if (compact)
        {
            slider.setBounds (getLocalBounds().removeFromRight (getHeight()));
            return;
        }

        const int size = juce::jmin (getWidth(), getHeight() - textHeight);
        slider.setBounds (juce::Rectangle<int> (size, size).withCentre ({ getWidth() / 2, size / 2 }));
    }

    void ParamKnob::paint (juce::Graphics& g)
    {
        if (compact)
        {
            auto area = getLocalBounds().withTrimmedRight (getHeight() + 2);
            g.setColour (colour::cream);
            g.setFont (makeFont (11.5f, "Bold"));
            g.drawText (caption.toUpperCase(), area.removeFromTop (area.getHeight() / 2), juce::Justification::centredRight, false);
            g.setColour (colour::amber.withAlpha (0.85f));
            g.setFont (makeFont (11.0f));
            g.drawText (slider.getTextFromValue (slider.getValue()), area, juce::Justification::centredRight, false);
            return;
        }

        auto text = getLocalBounds().withTop (getHeight() - textHeight);

        g.setColour (colour::cream);
        g.setFont (makeFont (11.5f, "Bold"));
        g.drawText (caption.toUpperCase(), text.removeFromTop (16), juce::Justification::centred, false);

        g.setColour (colour::amber.withAlpha (0.85f));
        g.setFont (makeFont (11.0f));
        g.drawText (slider.getTextFromValue (slider.getValue()), text, juce::Justification::centred, false);
    }
}
