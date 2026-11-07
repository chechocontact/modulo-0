#pragma once

namespace vm
{
    enum class CrackleType
    {
        normal = 0,
        dusty,
        heavy
    };

    // Ranges shared by the DSP (clamping) and the plugin layer (host ranges).
    namespace texture
    {
        inline constexpr float wowRateMinHz = 0.1f;
        inline constexpr float wowRateMaxHz = 2.0f;
        inline constexpr float flutterRateMinHz = 4.0f;
        inline constexpr float flutterRateMaxHz = 15.0f;

        // Delay excursion at depth = 1 (peak, each side of the mean delay).
        // Pitch deviation = 2*pi*rate*excursion, e.g. wow 1 Hz -> +-1.9 %.
        inline constexpr double wowMaxExcursionSeconds = 0.003;
        inline constexpr double flutterMaxExcursionSeconds = 0.0002;
        inline constexpr double dropMaxExcursionSeconds = 0.0015;
    }

    // All amounts are 0..1; rates are in Hz.
    struct TextureParameters
    {
        // WOW: slow pitch drift
        float wowDepth = 0.25f;
        float wowRate = 0.6f;
        float wowShape = 0.0f;          // 0 sine -> 0.5 triangle -> 1 random drift

        // FLUTTER: fast pitch wobble
        float flutterDepth = 0.15f;
        float flutterRate = 8.0f;
        float flutterVariation = 0.3f;  // randomness of rate and amplitude

        // VINYL: hiss + crackle + occasional warp
        float vinylAmount = 0.2f;
        CrackleType crackleType = CrackleType::normal;
        float vinylDrop = 0.0f;         // 0 = never, 1 = frequent and deep

        // DEGRADE: band-limiting, softened transients, mild saturation
        float degradeAmount = 0.25f;
        float degradeBandwidth = 0.6f;  // 0 narrow (telephone) .. 1 wide
        float degradeSmoothing = 0.3f;  // transient softening

        bool operator== (const TextureParameters&) const = default;
    };
}
