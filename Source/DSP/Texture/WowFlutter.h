#pragma once

#include "Core/AudioBufferView.h"
#include "Core/SmoothedValue.h"
#include "DelayLine.h"
#include "NoiseGenerator.h"

#include <array>
#include <cmath>

namespace vm
{
    // Pitch modulation by a time-varying delay (the way tape wow and flutter
    // really arise). The delay is  A * (1 + m(t))  with m in [-1, 1], so with
    // zero depth the delay is exactly zero: no latency and bit-transparent.
    //
    //   WOW     slow (0.1-2 Hz), up to 3 ms excursion. Shape: sine, triangle or
    //           random drift (filtered noise).
    //   FLUTTER fast (4-15 Hz), up to 0.2 ms excursion. Variation adds
    //           randomness to both its rate and its amplitude.
    //
    // Call advance() once per sample frame, then process() for each channel.
    class WowFlutter
    {
    public:
        // Smooth random signal in [-1, 1]: a new random target every 1/rate
        // seconds, cosine-interpolated. Unlike filtered white noise its slope
        // is bounded, so it cannot cause pitch spikes when used as a delay.
        struct SmoothRandom
        {
            void reset() noexcept { phase = 0.0; from = to = 0.0f; }

            float next (double increment, NoiseGenerator& source) noexcept
            {
                phase += increment;

                if (phase >= 1.0)
                {
                    phase -= (double) (int) phase;
                    from = to;
                    to = source.white();
                }

                return from + (to - from) * 0.5f * (1.0f - (float) std::cos (3.14159265358979323846 * phase));
            }

            double phase = 0.0;
            float from = 0.0f, to = 0.0f;
        };

        struct Params
        {
            float wowDepth = 0.0f;
            float wowRate = 0.6f;
            float wowShape = 0.0f;
            float flutterDepth = 0.0f;
            float flutterRate = 8.0f;
            float flutterVariation = 0.0f;
        };

        void prepare (const ProcessSpec& spec);
        void reset() noexcept;                   // snaps smoothers to their targets
        void setParams (const Params& p) noexcept;

        // extraDelaySamples: additional delay from the vinyl drop, >= 0.
        void advance (float extraDelaySamples) noexcept;
        float process (int channel, float input) noexcept;

    private:
        double sampleRate = 44100.0;
        int numChannels = 2;
        float wowExcursion = 0.0f;     // samples at depth 1
        float flutterExcursion = 0.0f;

        std::array<DelayLine, maxChannels> lines;
        std::array<float, maxChannels> delay {};

        // Depth ramps are slow on purpose: delay = depth * excursion, so a fast
        // depth change would itself bend the pitch.
        LinearSmoothedValue wowDepth, wowRate, wowShape;
        LinearSmoothedValue flutterDepth, flutterRate, flutterVariation;

        double wowPhase = 0.0;
        double flutterPhase = 0.0;
        SmoothRandom drift, flutterRateRandom, flutterAmpRandom;
        NoiseGenerator noise;
    };
}
