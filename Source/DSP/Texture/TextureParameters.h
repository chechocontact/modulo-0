#pragma once

namespace vm
{
    // Ranges shared by the DSP (clamping) and the plugin layer (host ranges).
    namespace texture
    {
        inline constexpr float wowRateMinHz = 0.1f;
        inline constexpr float wowRateMaxHz = 2.0f;
        inline constexpr float flutterRateMinHz = 4.0f;
        inline constexpr float flutterRateMaxHz = 15.0f;

        // Delay excursion at depth = 1 (peak, each side of the mean delay).
        // Pitch deviation = 2*pi*rate*excursion, e.g. wow 1 Hz -> +-5 %, flutter 10 Hz -> +-3.8 %.
        inline constexpr double wowMaxExcursionSeconds = 0.008;
        inline constexpr double flutterMaxExcursionSeconds = 0.0006;
        inline constexpr double dropMaxExcursionSeconds = 0.0015;

        // Fixed (no longer user controls)
        inline constexpr float wowShape = 0.0f;           // sine
        inline constexpr float flutterVariation = 0.3f;
    }

    // All amounts are 0..1; rates are in Hz.
    //
    // The vinyl NOISE (Vinyl Amount) is not part of Texture: it is added at the
    // end of the plugin, see Core/VinylNoise.h and MasterParameters::vinylAmount.
    struct TextureParameters
    {
        float wowDepth = 0.25f;
        float wowRate = 0.6f;

        float flutterDepth = 0.15f;
        float flutterRate = 8.0f;

        float vinylDrop = 0.0f;   // warp of pitch and level: 0 = never, 1 = frequent and deep

        // One knob for the whole degradation. Amount crossfades from the clean
        // signal to the degraded one and, as it rises, also narrows the band
        // and softens transients (see TextureProcessor).
        float degrade = 0.25f;

        bool operator== (const TextureParameters&) const = default;
    };
}
