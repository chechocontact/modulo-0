#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Colours, fonts and design-space constants of the interface (Module 7).
// Look: a wooden tape-machine cabinet with charcoal metal faceplates, cream
// engraved lettering and amber lights.
namespace vm::ui
{
    // The editor is laid out in this fixed design space and scaled to the window.
    inline constexpr int designWidth = 1360;
    inline constexpr int designHeight = 768;

    namespace colour
    {
        inline const juce::Colour cream { 0xffe9ddc0 };
        inline const juce::Colour creamDim { 0xff9c917a };
        inline const juce::Colour amber { 0xffffa733 };
        inline const juce::Colour amberDim { 0xff5a3a12 };
        inline const juce::Colour faceTop { 0xff34373b };
        inline const juce::Colour faceBottom { 0xff1f2124 };
        inline const juce::Colour recess { 0xff131415 };
        inline const juce::Colour woodDark { 0xff2a1509 };
        inline const juce::Colour woodLight { 0xff7a4524 };
    }

    inline juce::Font makeFont (float height, const juce::String& style = "Regular")
    {
        return juce::Font (juce::FontOptions().withName ("Segoe UI").withStyle (style).withHeight (height));
    }

    inline juce::Font makeTitleFont (float height)
    {
        return juce::Font (juce::FontOptions().withName ("Georgia").withStyle ("Bold Italic").withHeight (height));
    }
}
