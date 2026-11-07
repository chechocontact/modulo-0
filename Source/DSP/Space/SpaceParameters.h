#pragma once

#include <array>
#include <cmath>

namespace vm
{
    enum class ReverbType
    {
        room = 0,
        plate,
        hall
    };

    namespace space
    {
        // --- Delay time ---------------------------------------------------
        // The Time knob is 0..1. Free: exponential from 20 ms to 1.5 s.
        // Sync on: the same knob steps through the musical divisions below.
        inline constexpr float timeMinSeconds = 0.02f;
        inline constexpr float timeMaxSeconds = 1.5f;
        inline constexpr double maxDelaySeconds = 3.0;   // longest delay line (slow tempos in sync)
        inline constexpr double defaultBpm = 120.0;      // used when the host gives no tempo

        inline constexpr int numDivisions = 8;
        inline constexpr std::array<float, numDivisions> divisionBeats { 0.25f, 1.0f / 3.0f, 0.5f, 2.0f / 3.0f, 0.75f, 1.0f, 1.5f, 2.0f };
        inline constexpr std::array<const char*, numDivisions> divisionNames { "1/16", "1/8T", "1/8", "1/4T", "1/8.", "1/4", "1/4.", "1/2" };

        inline float freeTimeSeconds (float time) noexcept
        {
            const float t = time < 0.0f ? 0.0f : (time > 1.0f ? 1.0f : time);
            return timeMinSeconds * std::pow (timeMaxSeconds / timeMinSeconds, t);
        }

        inline int divisionIndex (float time) noexcept
        {
            const float t = time < 0.0f ? 0.0f : (time > 1.0f ? 1.0f : time);
            return (int) std::lround (t * (float) (numDivisions - 1));
        }

        // --- Delay feedback -----------------------------------------------
        inline constexpr float feedbackMaxGain = 0.92f;  // knob 1.0 -> this gain (always decays)
        inline constexpr float feedbackHighPassHz = 120.0f; // repeats get thinner and darker
        inline constexpr float feedbackLowPassHz = 5500.0f;
        inline constexpr double timeGlideSeconds = 0.12;  // delay-time changes glide (tape-like)

        // --- Reverb ---------------------------------------------------------
        // Decay knob 0..1 -> RT60 (seconds), exponential, per type.
        struct TypeConfig
        {
            float rt60Min, rt60Max;
            float preDelayMs;
            float dampingHz;                              // high-frequency absorption in the loop
            std::array<float, 8> lineMs;                  // feedback delay network lengths
            std::array<float, 4> diffuserMs;              // input diffusion
            float diffuserGain;
        };

        inline constexpr TypeConfig typeConfig (ReverbType type) noexcept
        {
            switch (type)
            {
                case ReverbType::room:
                    return { 0.2f, 2.5f, 4.0f, 6500.0f,
                             { 7.3f, 9.1f, 11.3f, 13.7f, 15.9f, 18.7f, 21.1f, 23.9f },
                             { 2.3f, 3.7f, 5.3f, 7.1f }, 0.6f };
                case ReverbType::plate:
                    return { 0.4f, 6.0f, 8.0f, 11000.0f,
                             { 19.1f, 23.3f, 27.7f, 31.9f, 36.1f, 41.3f, 45.7f, 49.9f },
                             { 3.1f, 4.9f, 6.7f, 9.1f }, 0.7f };
                case ReverbType::hall:
                    return { 0.8f, 12.0f, 30.0f, 4500.0f,
                             { 37.3f, 43.9f, 51.1f, 59.3f, 67.7f, 77.9f, 89.3f, 101.3f },
                             { 4.7f, 6.9f, 9.7f, 13.1f }, 0.7f };
            }
            return typeConfig (ReverbType::hall);
        }

        inline constexpr double typeFadeSeconds = 0.05;
        inline constexpr float reverbHighPassHz = 150.0f; // keeps the tail from getting muddy
    }

    struct SpaceParameters
    {
        // DELAY
        float delay = 0.25f;      // 0..1  amount of delay added to the signal
        float time = 0.55f;       // 0..1  free: 20 ms..1.5 s / sync: division
        float feedback = 0.35f;   // 0..1
        bool sync = false;        // true: Time selects a musical division of the host tempo

        // REVERB
        float mix = 0.25f;        // 0..1  amount of reverb added to the signal
        float decay = 0.5f;       // 0..1  RT60, range depends on Type
        ReverbType type = ReverbType::hall;

        // STEREO OPENING (both effects)
        float width = 0.8f;       // 0..1  delay: ping-pong amount / reverb: side level

        bool operator== (const SpaceParameters&) const = default;
    };
}
