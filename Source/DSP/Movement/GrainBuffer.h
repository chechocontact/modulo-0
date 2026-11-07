#pragma once

#include "Core/AudioBufferView.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace vm
{
    // Circular capture buffer, stereo. Allocated once in prepare(); never again.
    // The size is a power of two so every index wraps with a mask and can never
    // leave the allocation.
    class GrainBuffer
    {
    public:
        void prepare (double sampleRate, double seconds)
        {
            std::size_t n = 1024;
            while ((double) n < sampleRate * seconds)
                n <<= 1;

            for (auto& c : data)
                c.assign (n, 0.0f);

            size = (int) n;
            mask = (int) n - 1;
            writeIndex = 0;
        }

        void clear() noexcept
        {
            for (auto& c : data)
                std::fill (c.begin(), c.end(), 0.0f);

            writeIndex = 0;
        }

        int getSize() const noexcept { return size; }
        int getWriteIndex() const noexcept { return writeIndex; }

        void write (int channel, float value) noexcept
        {
            data[(std::size_t) channel][(std::size_t) writeIndex] = value;
        }

        void advance() noexcept { writeIndex = (writeIndex + 1) & mask; }

        // Position in samples in [0, size). Linear interpolation.
        float read (int channel, double position) const noexcept
        {
            const int i0 = (int) position & mask;
            const int i1 = (i0 + 1) & mask;
            const float frac = (float) (position - std::floor (position));
            const auto& c = data[(std::size_t) channel];
            return c[(std::size_t) i0] + frac * (c[(std::size_t) i1] - c[(std::size_t) i0]);
        }

        double wrap (double position) const noexcept
        {
            const double s = (double) size;
            while (position >= s) position -= s;
            while (position < 0.0) position += s;
            return position;
        }

    private:
        std::vector<float> data[2];
        int size = 0;
        int mask = 0;
        int writeIndex = 0;
    };
}
