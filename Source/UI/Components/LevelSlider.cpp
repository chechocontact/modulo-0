#include "LevelSlider.h"

#include "../Theme/Palette.h"

#include <algorithm>

namespace vm::ui
{
    namespace
    {
        constexpr float meterFloorDb = -60.0f;
    }

    LevelSlider::LevelSlider (juce::AudioProcessorValueTreeState& state, const char* parameterId,
                              juce::String labelText, const vm::LevelMeter& meterToShow)
        : meter (meterToShow), slider (state.getParameter (parameterId)), label (std::move (labelText))
    {
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        addAndMakeVisible (slider);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, parameterId, slider);
        slider.onValueChange = [this] { repaint(); };
    }

    void LevelSlider::refresh()
    {
        const float l = vm::LevelMeter::toDecibels (meter.getPeak (0));
        const float r = vm::LevelMeter::toDecibels (meter.getPeak (1));
        const float newPeak = std::max (l, r);

        if (std::abs (newPeak - peakDb) > 0.04f || std::abs (l - barDb[0]) > 0.2f || std::abs (r - barDb[1]) > 0.2f)
        {
            peakDb = newPeak;
            barDb[0] = l;
            barDb[1] = r;
            repaint();
        }
    }

    void LevelSlider::resized()
    {
        auto area = getLocalBounds();
        area.removeFromLeft (34);                       // label
        readoutArea = area.removeFromRight (74);        // "-12.1 dB"
        meterArea = area.removeFromRight (62).reduced (4, 0).withSizeKeepingCentre (54, 14);
        area.removeFromRight (6);
        slider.setBounds (area.withTrimmedTop (14));
    }

    void LevelSlider::paint (juce::Graphics& g)
    {
        // Label.
        g.setColour (colour::cream);
        g.setFont (makeFont (17.0f, "Bold"));
        g.drawText (label, getLocalBounds().removeFromLeft (34).withTrimmedTop (14), juce::Justification::centredLeft, false);

        // Gain value above the thumb.
        const auto thumbX = slider.getX() + slider.getPositionOfValue (slider.getValue());
        g.setColour (colour::amber.withAlpha (0.9f));
        g.setFont (makeFont (11.0f));
        g.drawText (slider.getTextFromValue (slider.getValue()),
                    juce::Rectangle<int> (70, 14).withCentre ({ (int) thumbX, 8 }), juce::Justification::centred, false);

        // Stereo meter.
        g.setColour (colour::recess);
        g.fillRoundedRectangle (meterArea.toFloat().expanded (1.5f), 2.0f);

        for (int ch = 0; ch < 2; ++ch)
        {
            auto bar = meterArea.toFloat().withHeight (meterArea.getHeight() * 0.5f - 1.0f).translated (0.0f, ch * (meterArea.getHeight() * 0.5f + 1.0f));
            const float fraction = juce::jlimit (0.0f, 1.0f, (barDb[ch] - meterFloorDb) / -meterFloorDb);
            auto fill = bar.withWidth (bar.getWidth() * fraction);

            g.setGradientFill (juce::ColourGradient (colour::creamDim, bar.getX(), 0.0f,
                                                     juce::Colour (0xffe0503a), bar.getRight(), 0.0f, false));
            g.fillRect (fill);
        }

        // Peak readout.
        const bool hot = peakDb > -0.5f;
        g.setColour (hot ? juce::Colour (0xffe0503a) : colour::cream);
        g.setFont (makeFont (13.0f));
        g.drawText (peakDb <= -99.0f ? juce::String ("-inf dB") : juce::String (peakDb, 1) + " dB",
                    readoutArea, juce::Justification::centredRight, false);
    }
}
