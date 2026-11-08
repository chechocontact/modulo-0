#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace vm::ui
{
    // Procedural textures, generated once when the editor opens (no image files
    // to ship or load). Both are deterministic for a given size and seed.
    juce::Image createWoodTexture (int width, int height, unsigned int seed = 37u);
    juce::Image createBrushedMetalTexture (int width, int height, unsigned int seed = 11u);
}
