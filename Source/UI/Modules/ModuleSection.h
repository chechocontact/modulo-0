#pragma once

#include "../Components/ChoiceSelector.h"
#include "../Components/ParamKnob.h"
#include "../Components/ToggleControls.h"

#include <memory>
#include <vector>

namespace vm::ui
{
    struct SectionConfig
    {
        juce::String number;                 // "01"; empty for a plain control strip
        juce::String title;                  // "TEXTURE"
        const char* enableId = nullptr;      // LED parameter (nullptr = no LED)
        const char* mixId = nullptr;         // Dry/Wet parameter shown in the header (nullptr = none)
        int knobDiameter = 56;
        bool inlineHeader = false;           // title on the left, controls on the same rows (Vintage Engine)
    };

    // One faceplate of the editor: header (number, name, LED, Dry/Wet) and framed,
    // named groups of controls. All controls are bound to parameters through the
    // APVTS; the section holds no audio state.
    class ModuleSection : public juce::Component
    {
    public:
        ModuleSection (juce::AudioProcessorValueTreeState& state, SectionConfig config, const juce::Image* faceTexture);

        void addGroup (const juce::String& title);
        void addKnob (const char* parameterId, const juce::String& caption);
        void addChoice (const char* parameterId, const juce::String& caption);
        void addToggle (const char* parameterId, const juce::String& caption);

        void paint (juce::Graphics&) override;
        void resized() override;

        // Width the groups need, used to check that a layout fits.
        int getRequiredBodyWidth() const;

    private:
        struct Group
        {
            juce::String title;
            std::vector<juce::Component*> cells;
        };

        void addCell (std::unique_ptr<juce::Component> cell);
        int getCellWidth() const noexcept { return juce::jmax (64, config.knobDiameter + 10); }
        int getCellHeight() const noexcept { return config.knobDiameter + 30; }
        juce::Rectangle<int> getBodyArea() const;
        std::vector<juce::Rectangle<int>> computeGroupBounds() const;

        static constexpr int groupPadding = 8;
        static constexpr int groupTitleHeight = 20;

        juce::AudioProcessorValueTreeState& apvts;
        SectionConfig config;
        const juce::Image* face;

        std::vector<Group> groups;
        std::vector<std::unique_ptr<juce::Component>> cells;
        std::unique_ptr<LedToggle> led;
        std::unique_ptr<ParamKnob> mix;
    };
}
