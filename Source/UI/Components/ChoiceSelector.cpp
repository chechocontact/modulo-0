#include "ChoiceSelector.h"

#include "../Theme/Palette.h"

namespace vm::ui
{
    ChoiceSelector::ChoiceSelector (juce::AudioProcessorValueTreeState& state, const char* parameterId, juce::String captionText)
        : parameter (state.getParameter (parameterId)), caption (std::move (captionText))
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (parameter))
            options = choice->choices;

        attachment = std::make_unique<juce::ParameterAttachment> (*parameter, [this] (float value)
        {
            selected = juce::jlimit (0, juce::jmax (0, options.size() - 1), (int) std::lround (value));
            repaint();
        });
        attachment->sendInitialUpdate();
    }

    juce::Rectangle<int> ChoiceSelector::getOptionsArea() const
    {
        const int height = juce::jmax (1, getHeight() - textHeight);
        return juce::Rectangle<int> (getWidth() - 8, juce::jmin (height, 22 * juce::jmax (1, options.size())))
                   .withCentre ({ getWidth() / 2, height / 2 });
    }

    void ChoiceSelector::paint (juce::Graphics& g)
    {
        const auto area = getOptionsArea().toFloat();
        const int count = juce::jmax (1, options.size());
        const float rowHeight = area.getHeight() / (float) count;

        g.setColour (colour::recess);
        g.fillRoundedRectangle (area, 4.0f);
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawRoundedRectangle (area, 4.0f, 1.0f);

        for (int i = 0; i < options.size(); ++i)
        {
            auto row = juce::Rectangle<float> (area.getX() + 2.0f, area.getY() + rowHeight * (float) i + 1.0f,
                                               area.getWidth() - 4.0f, rowHeight - 2.0f);
            const bool on = i == selected;

            if (on)
            {
                g.setColour (colour::amber.withAlpha (0.18f));
                g.fillRoundedRectangle (row, 3.0f);
                g.setColour (colour::amber);
                g.fillEllipse (row.getX() + 5.0f, row.getCentreY() - 2.5f, 5.0f, 5.0f);
            }

            g.setColour (on ? colour::cream : colour::creamDim);
            g.setFont (makeFont (11.0f, on ? "Bold" : "Regular"));
            g.drawText (options[i], row.withTrimmedLeft (12.0f), juce::Justification::centred, false);
        }

        g.setColour (colour::cream);
        g.setFont (makeFont (11.5f, "Bold"));
        g.drawText (caption.toUpperCase(), getLocalBounds().withTop (getHeight() - textHeight).removeFromTop (16),
                    juce::Justification::centred, false);
    }

    void ChoiceSelector::mouseDown (const juce::MouseEvent& e)
    {
        if (e.mods.isPopupMenu())
        {
            showHostContextMenu (*this, parameter);
            return;
        }

        const auto area = getOptionsArea();
        if (options.isEmpty() || ! area.contains (e.getPosition()))
            return;

        const int index = juce::jlimit (0, options.size() - 1, (e.y - area.getY()) * options.size() / juce::jmax (1, area.getHeight()));
        attachment->setValueAsCompleteGesture ((float) index);
    }
}
