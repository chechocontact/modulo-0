#include "ModuleSection.h"

#include "../Theme/Palette.h"

#include <numeric>

namespace vm::ui
{
    ModuleSection::ModuleSection (juce::AudioProcessorValueTreeState& state, SectionConfig cfg, const juce::Image* faceTexture)
        : apvts (state), config (std::move (cfg)), face (faceTexture)
    {
        if (config.enableId != nullptr)
        {
            led = std::make_unique<LedToggle> (apvts, config.enableId);
            addAndMakeVisible (*led);
        }

        if (config.mixId != nullptr)
        {
            mix = std::make_unique<ParamKnob> (apvts, config.mixId, "Dry/Wet", true);
            addAndMakeVisible (*mix);
        }
    }

    void ModuleSection::addGroup (const juce::String& title)
    {
        groups.push_back ({ title, {} });
    }

    void ModuleSection::addCell (std::unique_ptr<juce::Component> cell)
    {
        jassert (! groups.empty());
        groups.back().cells.push_back (cell.get());
        addAndMakeVisible (*cell);
        cells.push_back (std::move (cell));
    }

    void ModuleSection::addKnob (const char* id, const juce::String& caption)
    {
        addCell (std::make_unique<ParamKnob> (apvts, id, caption));
    }

    void ModuleSection::addChoice (const char* id, const juce::String& caption)
    {
        addCell (std::make_unique<ChoiceSelector> (apvts, id, caption));
    }

    void ModuleSection::addToggle (const char* id, const juce::String& caption)
    {
        addCell (std::make_unique<PushToggle> (apvts, id, caption));
    }

    juce::Rectangle<int> ModuleSection::getBodyArea() const
    {
        auto area = getLocalBounds();

        if (config.number.isEmpty())
            return area.reduced (12, 8);

        if (config.inlineHeader)
            return area.withTrimmedLeft (300).reduced (12, 8);

        return area.withTrimmedTop (38).reduced (12, 0).withTrimmedBottom (10);
    }

    int ModuleSection::getRequiredBodyWidth() const
    {
        int total = 0;
        for (auto& group : groups)
            total += (int) group.cells.size() * getCellWidth() + 2 * groupPadding;

        return total + 8 * juce::jmax (0, (int) groups.size() - 1);
    }

    std::vector<juce::Rectangle<int>> ModuleSection::computeGroupBounds() const
    {
        std::vector<juce::Rectangle<int>> result;
        const auto body = getBodyArea();
        const int count = (int) groups.size();
        if (count == 0)
            return result;

        int widthSum = 0;
        for (auto& group : groups)
            widthSum += (int) group.cells.size() * getCellWidth() + 2 * groupPadding;

        const int gap = juce::jlimit (8, 40, (body.getWidth() - widthSum) / (count + 1));
        int x = body.getX() + (body.getWidth() - (widthSum + gap * (count - 1))) / 2;

        for (auto& group : groups)
        {
            const int w = (int) group.cells.size() * getCellWidth() + 2 * groupPadding;
            result.push_back ({ x, body.getY(), w, body.getHeight() });
            x += w + gap;
        }

        return result;
    }

    void ModuleSection::resized()
    {
        if (led != nullptr)
            led->setBounds (getWidth() - 12 - 28, 6, 28, 28);

        if (mix != nullptr)
            mix->setBounds (getWidth() - 12 - 28 - 12 - 120, 3, 120, 34);

        const auto bounds = computeGroupBounds();
        for (size_t g = 0; g < groups.size(); ++g)
        {
            const auto box = bounds[g];
            const auto inner = box.withTrimmedTop (groupTitleHeight).reduced (groupPadding, 0);
            const int y = inner.getY() + juce::jmax (0, (inner.getHeight() - getCellHeight()) / 2);

            for (size_t c = 0; c < groups[g].cells.size(); ++c)
                groups[g].cells[c]->setBounds (inner.getX() + (int) c * getCellWidth(), y, getCellWidth(), getCellHeight());
        }
    }

    void ModuleSection::paint (juce::Graphics& g)
    {
        const auto area = getLocalBounds().toFloat();

        // Faceplate.
        {
            juce::Graphics::ScopedSaveState state (g);
            juce::Path outline;
            outline.addRoundedRectangle (area, 6.0f);
            g.reduceClipRegion (outline);

            if (face != nullptr && face->isValid())
                g.drawImage (*face, 0, 0, getWidth(), getHeight(), getX(), getY(), getWidth(), getHeight());
            else
                g.fillAll (colour::faceTop);
        }

        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawRoundedRectangle (area.reduced (1.5f), 5.0f, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawRoundedRectangle (area.reduced (0.5f), 6.0f, 1.5f);

        // Screws.
        const juce::Point<float> corners[] { { 9.0f, 9.0f }, { area.getRight() - 9.0f, 9.0f },
                                             { 9.0f, area.getBottom() - 9.0f }, { area.getRight() - 9.0f, area.getBottom() - 9.0f } };
        int index = 0;
        for (auto c : corners)
        {
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff8f9295), c.x - 3.0f, c.y - 3.0f,
                                                     juce::Colour (0xff2a2b2d), c.x + 3.0f, c.y + 3.0f, false));
            g.fillEllipse (c.x - 4.0f, c.y - 4.0f, 8.0f, 8.0f);
            g.setColour (juce::Colours::black.withAlpha (0.7f));
            const float a = 0.6f + 0.9f * (float) index++;
            g.drawLine (c.x - 3.0f * std::cos (a), c.y - 3.0f * std::sin (a), c.x + 3.0f * std::cos (a), c.y + 3.0f * std::sin (a), 1.2f);
        }

        // Header.
        if (config.number.isNotEmpty())
        {
            const int headerY = config.inlineHeader ? (getHeight() - 34) / 2 : 4;

            g.setColour (colour::cream);
            g.setFont (makeTitleFont (30.0f));
            g.drawText (config.number, juce::Rectangle<int> (22, headerY, 56, 34), juce::Justification::centredLeft, false);

            g.setFont (makeFont (16.0f, "Bold"));
            g.drawText (config.title.toUpperCase(), juce::Rectangle<int> (78, headerY, 220, 34), juce::Justification::centredLeft, false);

            if (! config.inlineHeader)
            {
                g.setColour (colour::cream.withAlpha (0.20f));
                g.drawLine (14.0f, 38.0f, area.getRight() - 14.0f, 38.0f, 1.0f);
            }
        }

        // Group frames and names.
        const auto bounds = computeGroupBounds();
        for (size_t i = 0; i < groups.size(); ++i)
        {
            const auto box = bounds[i].toFloat().reduced (0.5f);
            g.setColour (juce::Colours::black.withAlpha (0.18f));
            g.fillRoundedRectangle (box, 5.0f);
            g.setColour (colour::cream.withAlpha (0.32f));
            g.drawRoundedRectangle (box, 5.0f, 1.0f);

            g.setColour (colour::creamDim);
            g.setFont (makeFont (10.5f, "Bold"));
            g.drawText (groups[i].title.toUpperCase(), bounds[i].withHeight (groupTitleHeight).reduced (8, 0),
                        juce::Justification::centredLeft, false);
        }
    }
}
