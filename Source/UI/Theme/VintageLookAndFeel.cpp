#include "VintageLookAndFeel.h"

#include "Palette.h"

#include <cmath>

namespace vm::ui
{
    VintageLookAndFeel::VintageLookAndFeel()
    {
        setColour (juce::PopupMenu::backgroundColourId, colour::faceBottom);
        setColour (juce::PopupMenu::textColourId, colour::cream);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, colour::amberDim);
        setColour (juce::PopupMenu::highlightedTextColourId, colour::cream);
        setColour (juce::PopupMenu::headerTextColourId, colour::creamDim);
        setColour (juce::TextButton::textColourOffId, colour::cream);
        setColour (juce::TextButton::textColourOnId, colour::cream);
    }

    void VintageLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                               float startAngle, float endAngle, juce::Slider& slider)
    {
        const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (2.0f);
        const float diameter = juce::jmin (bounds.getWidth(), bounds.getHeight());
        const auto centre = bounds.getCentre();
        const float radius = diameter * 0.5f;
        const float angle = startAngle + sliderPos * (endAngle - startAngle);
        const bool enabled = slider.isEnabled();

        // Scale arc: dim track plus the amber value arc (from the centre for bipolar controls).
        const float arcRadius = radius - 2.5f;
        juce::Path track;
        track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
        g.setColour (colour::recess.brighter (0.12f));
        g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
        const float from = bipolar ? startAngle + 0.5f * (endAngle - startAngle) : startAngle;

        if (std::abs (angle - from) > 0.002f)
        {
            juce::Path value;
            value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                                 juce::jmin (from, angle), juce::jmax (from, angle), true);

            g.setColour (colour::amber.withAlpha (0.20f));
            g.strokePath (value, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (colour::amber.withAlpha (enabled ? 1.0f : 0.4f));
            g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Knob body.
        const float bodyRadius = radius * 0.70f;
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillEllipse (centre.x - bodyRadius - 1.0f, centre.y - bodyRadius + 2.5f, 2.0f * bodyRadius + 2.0f, 2.0f * bodyRadius + 2.0f);

        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6d7074), centre.x - bodyRadius, centre.y - bodyRadius,
                                                 juce::Colour (0xff141517), centre.x + bodyRadius, centre.y + bodyRadius, false));
        g.fillEllipse (centre.x - bodyRadius, centre.y - bodyRadius, 2.0f * bodyRadius, 2.0f * bodyRadius);

        // Knurled edge.
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        for (int i = 0; i < 40; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / 40.0f;
            const float s = std::sin (a), c = -std::cos (a);
            g.drawLine (centre.x + s * bodyRadius * 0.90f, centre.y + c * bodyRadius * 0.90f,
                        centre.x + s * bodyRadius, centre.y + c * bodyRadius, 1.0f);
        }

        g.setColour (juce::Colour (0xffb7b9bb).withAlpha (0.55f));
        g.drawEllipse (centre.x - bodyRadius, centre.y - bodyRadius, 2.0f * bodyRadius, 2.0f * bodyRadius, 1.2f);

        // Cap.
        const float capRadius = bodyRadius * 0.78f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4d51), centre.x, centre.y - capRadius,
                                                 juce::Colour (0xff1c1d20), centre.x, centre.y + capRadius, false));
        g.fillEllipse (centre.x - capRadius, centre.y - capRadius, 2.0f * capRadius, 2.0f * capRadius);

        // Pointer.
        const float s = std::sin (angle), c = -std::cos (angle);
        g.setColour (colour::cream.withAlpha (enabled ? 1.0f : 0.4f));
        g.drawLine (centre.x + s * capRadius * 0.25f, centre.y + c * capRadius * 0.25f,
                    centre.x + s * bodyRadius * 0.96f, centre.y + c * bodyRadius * 0.96f, 2.6f);
    }

    void VintageLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                               float, float, juce::Slider::SliderStyle, juce::Slider& slider)
    {
        const float centreY = (float) y + (float) height * 0.5f;
        const float left = (float) x + 4.0f;
        const float right = (float) (x + width) - 4.0f;

        // Groove.
        const auto groove = juce::Rectangle<float> (left, centreY - 3.0f, right - left, 6.0f);
        g.setColour (colour::recess);
        g.fillRoundedRectangle (groove, 3.0f);
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawRoundedRectangle (groove, 3.0f, 1.0f);

        // Ticks (-24, -12, 0, +12, +24 for the gain range).
        const double lo = slider.getMinimum(), hi = slider.getMaximum();
        g.setColour (colour::creamDim.withAlpha (0.7f));
        for (int i = 0; i <= 4; ++i)
        {
            const float tx = left + (right - left) * (float) i / 4.0f;
            const float h = (i == 2) ? 7.0f : 4.0f;
            g.drawLine (tx, centreY + 8.0f, tx, centreY + 8.0f + h, 1.0f);
        }
        juce::ignoreUnused (lo, hi);

        // Thumb.
        const auto thumb = juce::Rectangle<float> (14.0f, 24.0f).withCentre ({ sliderPos, centreY });
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (thumb.translated (0.0f, 2.0f), 3.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff8a8d90), thumb.getX(), thumb.getY(),
                                                 juce::Colour (0xff2a2b2e), thumb.getRight(), thumb.getBottom(), false));
        g.fillRoundedRectangle (thumb, 3.0f);
        g.setColour (juce::Colour (0xffb7b9bb).withAlpha (0.6f));
        g.drawRoundedRectangle (thumb, 3.0f, 1.0f);
        g.setColour (colour::cream);
        g.drawLine (thumb.getCentreX(), thumb.getY() + 4.0f, thumb.getCentreX(), thumb.getBottom() - 4.0f, 2.0f);
    }

    void VintageLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                                   bool highlighted, bool down)
    {
        auto area = button.getLocalBounds().toFloat().reduced (0.5f);
        const float alpha = button.isEnabled() ? 1.0f : 0.40f;

        g.setGradientFill (juce::ColourGradient (down ? colour::recess : juce::Colour (0xff2f3236), 0.0f, area.getY(),
                                                 down ? juce::Colour (0xff222427) : colour::recess, 0.0f, area.getBottom(), false));
        g.fillRoundedRectangle (area, 4.0f);
        g.setColour ((highlighted && button.isEnabled() ? colour::cream : colour::creamDim).withAlpha (alpha));
        g.drawRoundedRectangle (area, 4.0f, 1.2f);
    }

    void VintageLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool)
    {
        g.setFont (getTextButtonFont (button, button.getHeight()));
        g.setColour (colour::cream.withAlpha (button.isEnabled() ? 1.0f : 0.4f));
        g.drawText (button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, false);
    }

    juce::Font VintageLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
    {
        return makeFont ((float) juce::jmin (16, buttonHeight * 5 / 10), "Bold");
    }
}
