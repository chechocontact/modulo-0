#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace vm::ui
{
    // Right click on a control: show the DAW's own parameter menu (automation,
    // MIDI learn, ...) when the host provides one. Returns false when it does not,
    // in which case nothing is shown (there is no substitute menu).
    inline bool showHostContextMenu (juce::Component& source, const juce::AudioProcessorParameter* parameter)
    {
        if (parameter == nullptr)
            return false;

        if (auto* editor = source.findParentComponentOfClass<juce::AudioProcessorEditor>())
            if (auto* context = editor->getHostContext())
                if (auto menu = context->getContextMenuForParameter (parameter))
                {
                    menu->showNativeMenu (editor->getMouseXYRelative());
                    return true;
                }

        return false;
    }

    // Slider that opens the host menu on right click and otherwise behaves as usual:
    // double click = default value (set by the attachment), Ctrl/Cmd + drag = fine adjust.
    class ParameterSlider : public juce::Slider
    {
    public:
        explicit ParameterSlider (juce::RangedAudioParameter* p) : parameter (p)
        {
            setVelocityBasedMode (false);
            setVelocityModeParameters (0.35, 1, 0.0, true, juce::ModifierKeys::ctrlAltCommandModifiers);
            setMouseDragSensitivity (220);
            setScrollWheelEnabled (false);
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu())
            {
                showHostContextMenu (*this, parameter);
                return;
            }

            juce::Slider::mouseDown (e);
        }

    private:
        juce::RangedAudioParameter* parameter;
    };
}
