#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

namespace vm
{
    // Mono circular delay with 4-point cubic Hermite interpolation for
    // fractional, time-varying delays. Allocates only in prepare().
    //
    // Usage per sample:  push (x);  y = read (delaySamples);
    // read (0) returns exactly the sample just pushed, so a zero delay is
    // bit-transparent. For delays below one sample the not-yet-written
    // "future" tap is replaced by the newest sample.
    class DelayLine
    {
    public:
        void prepare (int maxDelaySamples)
        {
            std::size_t size = 8;
            while ((int) size < maxDelaySamples + 8)
                size <<= 1;

            buffer.assign (size, 0.0f);
            mask = size - 1;
            maxDelay = (float) (size - 4);
            writeIndex = 0;
        }

        void reset() noexcept { std::fill (buffer.begin(), buffer.end(), 0.0f); writeIndex = 0; }

        void push (float x) noexcept
        {
            buffer[writeIndex] = x;
            writeIndex = (writeIndex + 1) & mask;
        }

        float read (float delay) const noexcept
        {
            delay = delay < 0.0f ? 0.0f : (delay > maxDelay ? maxDelay : delay);

            const int whole = (int) delay;
            const float t = delay - (float) whole;
            const std::size_t newest = (writeIndex - 1) & mask;

            const float y0 = buffer[(newest - (std::size_t) whole) & mask];
            const float y1 = buffer[(newest - (std::size_t) whole - 1) & mask];
            const float y2 = buffer[(newest - (std::size_t) whole - 2) & mask];
            const float ym1 = whole == 0 ? buffer[newest] : buffer[(newest - (std::size_t) whole + 1) & mask];

            const float c1 = 0.5f * (y1 - ym1);
            const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
            const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
            return ((c3 * t + c2) * t + c1) * t + y0;
        }

    private:
        std::vector<float> buffer;
        std::size_t mask = 0;
        std::size_t writeIndex = 0;
        float maxDelay = 0.0f;
    };
}
