#pragma once

#include <array>
#include <cmath>

namespace vm
{
    // Hann window as a lookup table (computed once, no allocation).
    // Zero at both ends, so a grain starts and stops without a click.
    class GrainWindow
    {
    public:
        GrainWindow() noexcept
        {
            constexpr double twoPi = 6.283185307179586;
            for (int i = 0; i <= tableSize; ++i)
                table[(std::size_t) i] = (float) (0.5 - 0.5 * std::cos (twoPi * i / tableSize));
        }

        // phase in [0, 1]
        float get (float phase) const noexcept
        {
            const float p = phase < 0.0f ? 0.0f : (phase > 1.0f ? 1.0f : phase);
            const float x = p * (float) tableSize;
            const int i = (int) x;
            const float f = x - (float) i;
            const int j = i < tableSize ? i + 1 : tableSize;
            return table[(std::size_t) i] + f * (table[(std::size_t) j] - table[(std::size_t) i]);
        }

    private:
        static constexpr int tableSize = 1024;
        std::array<float, tableSize + 1> table {};
    };
}
