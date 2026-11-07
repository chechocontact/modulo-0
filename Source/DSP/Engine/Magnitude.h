#pragma once

#include "DSP/Color/ColorParameters.h"
#include "DSP/Movement/MovementParameters.h"
#include "DSP/Space/SpaceParameters.h"
#include "DSP/Texture/TextureParameters.h"

#include <cmath>

namespace vm::magnitude
{
    // MAGNITUDE: one global intensity control, 0..1 (1 = every effect as set).
    //
    // It scales the AMOUNT of each effect, as a factor applied to the values handed to
    // the modules; it never writes to the host's parameters, so the individual
    // settings stay exactly as the user left them. 0 turns every amount to zero,
    // which leaves the plugin transparent.
    //
    // Scaled: Wow Depth, Flutter Depth, Vinyl Drop, Degrade, Vinyl Amount (see
    // scaleVinyl), Drive, Tone, Magic Chorus, Grain, Feedback, Tremolo, Delay and
    // reverb Mix. Not scaled: rates, types, Time, Decay, Width, Sync, each module's
    // on/off and Dry/Wet, and the master gains and mix.
    inline float sanitise (float value) noexcept
    {
        return std::isfinite (value) ? (value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value)) : 1.0f;
    }

    inline void apply (float m, TextureParameters& p) noexcept
    {
        p.wowDepth *= m;
        p.flutterDepth *= m;
        p.vinylDrop *= m;
        p.degrade *= m;
    }

    inline void apply (float m, ColorParameters& p) noexcept
    {
        p.drive *= m;
        p.tone *= m;
        p.magicChorus *= m;
    }

    inline void apply (float m, MovementParameters& p) noexcept
    {
        p.grain *= m;
        p.feedback *= m;
        p.tremolo *= m;
    }

    inline void apply (float m, SpaceParameters& p) noexcept
    {
        p.delay *= m;
        p.mix *= m;
    }

    inline float scaleVinyl (float m, float vinylAmount) noexcept
    {
        return vinylAmount * m;
    }
}
