#include "ToggleControls.h"

#include "../Theme/Palette.h"

namespace vm::ui
{
    LedToggle::LedToggle (juce::AudioProcessorValueTreeState& state, const char* parameterId)
        : juce::Button (parameterId), parameter (state.getParameter (parameterId))
    {
        setClickingTogglesState (true);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, parameterId, *this);
    }

    void LedToggle::paintButton (juce::Graphics& g, bool highlighted, bool)
    {
        const auto area = getLocalBounds().toFloat().reduced (3.0f);
        const bool on = getToggleState();
        const auto centre = area.getCentre();
        const float radius = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f;

        // Bezel.
        g.setColour (colour::recess);
        g.fillEllipse (area.expanded (2.0f));
        g.setColour (highlighted ? colour::cream : colour::creamDim);
        g.drawEllipse (area.expanded (2.0f), 1.4f);

        if (on)
        {
            g.setColour (colour::amber.withAlpha (0.28f));
            g.fillEllipse (centre.x - radius * 1.8f, centre.y - radius * 1.8f, radius * 3.6f, radius * 3.6f);
        }

        g.setGradientFill (on ? juce::ColourGradient (juce::Colour (0xffffd08a), centre.x - radius * 0.3f, centre.y - radius * 0.4f,
                                                      colour::amber.darker (0.4f), centre.x + radius, centre.y + radius, true)
                              : juce::ColourGradient (colour::amberDim, centre.x, centre.y - radius,
                                                      juce::Colour (0xff231508), centre.x, centre.y + radius, false));
        g.fillEllipse (area);
    }

    void LedToggle::mouseDown (const juce::MouseEvent& e)
    {
        if (e.mods.isPopupMenu())
        {
            showHostContextMenu (*this, parameter);
            return;
        }

        juce::Button::mouseDown (e);
    }

    PushToggle::PushToggle (juce::AudioProcessorValueTreeState& state, const char* parameterId, juce::String captionText)
        : juce::Button (parameterId), parameter (state.getParameter (parameterId)), caption (std::move (captionText))
    {
        setClickingTogglesState (true);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, parameterId, *this);
    }

    void PushToggle::paintButton (juce::Graphics& g, bool highlighted, bool down)
    {
        const int areaHeight = juce::jmax (1, getHeight() - textHeight);
        auto body = juce::Rectangle<float> ((float) getWidth() - 14.0f, juce::jmin (34.0f, (float) areaHeight * 0.6f))
                        .withCentre ({ (float) getWidth() * 0.5f, (float) areaHeight * 0.5f });
        if (down)
            body = body.translated (0.0f, 1.0f);

        const bool on = getToggleState();

        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3d41), 0.0f, body.getY(),
                                                 colour::recess, 0.0f, body.getBottom(), false));
        g.fillRoundedRectangle (body, 4.0f);
        g.setColour (highlighted ? colour::cream : colour::creamDim);
        g.drawRoundedRectangle (body, 4.0f, 1.2f);

        // Lamp.
        const auto lamp = juce::Rectangle<float> (10.0f, 10.0f).withCentre (body.getCentre());
        if (on)
        {
            g.setColour (colour::amber.withAlpha (0.30f));
            g.fillEllipse (lamp.expanded (5.0f));
        }
        g.setColour (on ? colour::amber : colour::amberDim);
        g.fillEllipse (lamp);

        g.setColour (colour::cream);
        g.setFont (makeFont (11.5f, "Bold"));
        g.drawText (caption.toUpperCase(), getLocalBounds().withTop (getHeight() - textHeight).removeFromTop (16),
                    juce::Justification::centred, false);
    }

    void PushToggle::mouseDown (const juce::MouseEvent& e)
    {
        if (e.mods.isPopupMenu())
        {
            showHostContextMenu (*this, parameter);
            return;
        }

        juce::Button::mouseDown (e);
    }
}
