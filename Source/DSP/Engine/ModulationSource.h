#pragma once

#include "Core/NoiseGenerator.h"

#include <cmath>
#include <cstdint>

namespace vm
{
    // Smooth random signal in [-1, 1]: a new random target every segment, joined
    // by a raised-cosine curve, so it has no jumps and no sudden slope changes.
    // Real-time safe: no allocation, no locks.
    class ModulationSource
    {
    public:
        void setSeed (std::uint32_t seed) noexcept
        {
            random.setSeed (seed);
            from = random.white();
            to = random.white();
            phase = random.uniform01(); // start at a different point of the segment
            segmentStep = 1.0;
        }

        // cyclesPerSecond: segments per second. jitter 0 = regular, 1 = very irregular.
        void setSpeed (double cyclesPerSecond, double sampleRate, float newJitter) noexcept
        {
            baseStep = cyclesPerSecond / sampleRate;
            jitter = newJitter;
        }

        // Moves the source on by numSamples and returns its value.
        float advance (int numSamples) noexcept
        {
            double move = baseStep * segmentStep * (double) numSamples;
            if (move > 4.0) move = 4.0; // never loop for long, even with huge blocks

            phase += move;
            while (phase >= 1.0)
            {
                phase -= 1.0;
                from = to;
                to = random.white();
                segmentStep = 1.0 + (double) jitter * (random.uniform01() - 0.5);
            }

            const float t = (float) (0.5 - 0.5 * std::cos (3.14159265358979323846 * phase));
            return from + (to - from) * t;
        }

    private:
        NoiseGenerator random;
        float from = 0.0f, to = 0.0f;
        double phase = 0.0;
        double baseStep = 0.0;
        double segmentStep = 1.0;
        float jitter = 0.0f;
    };
}
