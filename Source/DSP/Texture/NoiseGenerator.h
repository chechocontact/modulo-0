#pragma once

#include <cstdint>

namespace vm
{
    // Deterministic, allocation-free noise source (xorshift32). Same seed, same
    // sequence on every machine, so tests and renders are reproducible.
    class NoiseGenerator
    {
    public:
        explicit NoiseGenerator (std::uint32_t seed = 1u) noexcept { setSeed (seed); }

        void setSeed (std::uint32_t seed) noexcept
        {
            state = seed != 0u ? seed : 0x9E3779B9u;
            b0 = b1 = b2 = b3 = b4 = b5 = b6 = 0.0f;
        }

        // Uniform in [-1, 1).
        float white() noexcept
        {
            return (float) (int) nextBits() * (1.0f / 2147483648.0f);
        }

        // Uniform in [0, 1).
        float uniform01() noexcept
        {
            return (float) (nextBits() >> 8) * (1.0f / 16777216.0f);
        }

        // Approximately -3 dB/octave (Paul Kellett's economy filter), roughly unit-ish level.
        float pink() noexcept
        {
            const float w = white();
            b0 = 0.99886f * b0 + w * 0.0555179f;
            b1 = 0.99332f * b1 + w * 0.0750759f;
            b2 = 0.96900f * b2 + w * 0.1538520f;
            b3 = 0.86650f * b3 + w * 0.3104856f;
            b4 = 0.55000f * b4 + w * 0.5329522f;
            b5 = -0.7616f * b5 - w * 0.0168980f;
            const float out = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362f;
            b6 = w * 0.115926f;
            return out * 0.25f;
        }

    private:
        std::uint32_t nextBits() noexcept
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            return state;
        }

        std::uint32_t state = 1u;
        float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
    };
}
