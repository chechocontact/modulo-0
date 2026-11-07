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
        inline constexpr float rateMinHz = 0.02f;
        inline constexpr float rateMaxHz = 0.5f;

        // The offsets follow their targets through a one-pole smoother, so even the
        // fastest character never makes a step.
        inline constexpr double smoothingSeconds = 0.08;

        // Largest offset of each destination at Flux = 1 and Depth = 1.
        // Amounts are in the unit of the destination (0..1 knobs, Hz for rates).
        inline constexpr float wowDepthRange = 0.20f;
        inline constexpr float wowRateRange = 0.35f;       // Hz
        inline constexpr float flutterDepthRange = 0.15f;
        inline constexpr float flutterRateRange = 3.0f;    // Hz
        inline constexpr float vinylAmountRange = 0.10f;
        inline constexpr float driveRange = 0.15f;
        inline constexpr float grainRange = 0.15f;
        inline constexpr float tremoloRateRange = 2.0f;    // Hz
        inline constexpr float widthRange = 0.20f;

        inline constexpr int numDestinations = 9;
    }

    struct VintageEngineParameters
    {
        float flux = 0.4f;     // 0..1: how much randomness (0 = engine off, bit-transparent)
        float rate = 0.3f;     // 0..1: speed of the variation
        float depth = 0.5f;    // 0..1: size of the excursion on every destination
        EngineCharacter character = EngineCharacter::organic;

        bool operator== (const VintageEngineParameters&) const = default;
    };
}
