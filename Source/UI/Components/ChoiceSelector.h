#pragma once

#include "HostContextMenu.h"

#include <memory>

namespace vm::ui
{
    // Selector for a choice parameter: the options are stacked and one click picks one.
    class ChoiceSelector : public juce::Component
    {
    public:
        ChoiceSelector (juce::AudioProcessorValueTreeState& state, const char* parameterId, juce::String captionText);

        void resized() override {}
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;

    private:
        static constexpr int textHeight = 30;

        juce::Rectangle<int> getOptionsArea() const;

        juce::RangedAudioParameter* parameter = nullptr;
        juce::StringArray options;
        juce::String caption;
        int selected = 0;
        std::unique_ptr<juce::ParameterAttachment> attachment;
    };
}
