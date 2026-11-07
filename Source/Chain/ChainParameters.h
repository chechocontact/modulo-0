#pragma once

#include "Core/MasterProcessor.h"
#include "DSP/Color/ColorParameters.h"
#include "DSP/Engine/VintageEngineParameters.h"
#include "DSP/Movement/MovementParameters.h"
#include "DSP/Space/SpaceParameters.h"
#include "DSP/Texture/TextureParameters.h"

namespace vm
{
    struct SlotSettings
    {
        bool enabled = true;
        float mix = 1.0f; // 0..1 (the slot's Dry/Wet)
    };

    // Everything the plugin needs for one block, as plain values (no JUCE).
    struct ChainParameters
    {
        MasterParameters master;
        float magnitude = 1.0f;          // 0..1

        SlotSettings textureSlot, colorSlot, movementSlot, spaceSlot;
        TextureParameters texture;
        ColorParameters color;
        MovementParameters movement;
        SpaceParameters space;
        VintageEngineParameters engine;

        double tempoBpm = 0.0;           // host tempo; 0 = the host gave none
    };
}
