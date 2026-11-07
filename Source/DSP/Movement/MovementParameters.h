#pragma once

namespace vm
{
    enum class MovementStyle
    {
        frozen = 0, // capture stops; grains replay the last moment as a held pad
        drift,      // read point wanders slowly through the recent past
        cloud       // random read points, wide stereo, occasional octave grains
    };

    namespace movement
    {
        inline constexpr float rateMinHz = 0.1f;   // tremolo rate
        inline constexpr float rateMaxHz = 15.0f;

        inline constexpr double bufferSeconds = 2.0; // capture length (rounded up to a power of two of samples)
        inline constexpr int maxVoices = 32;

        // The feedback knob (0..1) maps to this gain on the grain output that is
        // written back into the capture buffer. Below 1, so the loop always decays.
        inline constexpr float feedbackMaxGain = 0.85f;

        // Tremolo: sine shape, left/right offset by a quarter cycle.
        inline constexpr float tremoloStereoPhase = 0.25f;

        // Fixed grain behaviour of each style (Size, Density, Pitch and Spread are
        // not user controls).
        struct StyleConfig
        {
            float sizeSeconds;       // grain length
            float density;           // grains per second
            float intervalJitter;    // 0 = regular, 1 = very irregular
            float delayMinSeconds;   // how far back in the past a grain starts (min / range)
            float delayRangeSeconds;
            float spread;            // 0..1 stereo spread of the grains
            float detuneCents;       // random +- detune of each grain
            float octaveUpChance;    // probability of a +12 st grain
            float octaveDownChance;  // probability of a -12 st grain
            float driftHz;           // speed of the wandering read point (Drift only)
            bool freezeCapture;
        };

        inline constexpr StyleConfig styleConfig (MovementStyle style) noexcept
        {
            switch (style)
            {
                case MovementStyle::frozen: return { 0.22f, 14.0f, 0.2f, 0.0f,  0.40f, 0.6f,  4.0f, 0.00f, 0.00f, 0.00f, true  };
                case MovementStyle::drift:  return { 0.16f, 18.0f, 0.25f, 0.12f, 0.50f, 0.5f,  8.0f, 0.00f, 0.00f, 0.06f, false };
                case MovementStyle::cloud:  return { 0.10f, 32.0f, 0.6f,  0.02f, 0.98f, 0.9f,  6.0f, 0.12f, 0.08f, 0.00f, false };
            }
            return { 0.10f, 32.0f, 0.6f, 0.02f, 0.98f, 0.9f, 6.0f, 0.12f, 0.08f, 0.00f, false };
        }
    }

    struct MovementParameters
    {
        float grain = 0.4f;          // 0..1: amount of granular signal (0 = bit-transparent)
        float feedback = 0.3f;       // 0..1: grain output fed back into the capture buffer
        float tremolo = 0.25f;       // 0..1: tremolo depth (0 = off)
        float rateHz = 4.0f;         // tremolo rate
        MovementStyle style = MovementStyle::cloud;

        bool operator== (const MovementParameters&) const = default;
    };
}
