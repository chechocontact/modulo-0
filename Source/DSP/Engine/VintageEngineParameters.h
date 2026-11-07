#pragma once

namespace vm
{
    enum class EngineCharacter
    {
        slow = 0, // very slow drifts, almost imperceptible
        organic,  // several speeds mixed, like a worn machine
        random    // faster, irregular changes (always smoothed)
    };

    namespace engine
    {
        // Rate knob 0..1 -> base speed of the drifts, exponential.
        inline constexpr float rateMinHz = 0.04f;
        inline constexpr float rateMaxHz = 1.2f;

        // The offsets follow their targets through a one-pole smoother, so even the
        // fastest character never makes a step.
        inline constexpr double smoothingSeconds = 0.06;

        // Largest offset of each destination at Flux = 1 and Depth = 1.
        // Amounts are in the unit of the destination (0..1 knobs, Hz for rates).
        inline constexpr float wowDepthRange = 0.45f;
        inline constexpr float wowRateRange = 0.80f;       // Hz
        inline constexpr float flutterDepthRange = 0.40f;
        inline constexpr float flutterRateRange = 6.0f;    // Hz
        inline constexpr float vinylAmountRange = 0.30f;
        inline constexpr float driveRange = 0.40f;
        inline constexpr float grainRange = 0.40f;
        inline constexpr float tremoloRateRange = 5.0f;    // Hz
        inline constexpr float widthRange = 0.45f;

        inline constexpr int numDestinations = 9;
    }

    struct VintageEngineParameters
    {
        bool enabled = true;   // module on/off: off = every offset glides back to 0 (80 ms)
        float flux = 0.6f;     // 0..1: how much randomness (0 = engine off, bit-transparent)
        float rate = 0.5f;     // 0..1: speed of the variation
        float depth = 0.7f;    // 0..1: size of the excursion on every destination
        EngineCharacter character = EngineCharacter::organic;

        bool operator== (const VintageEngineParameters&) const = default;
    };
}
