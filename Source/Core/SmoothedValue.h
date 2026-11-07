#pragma once

#include <cmath>

namespace vm
{
    // Linear parameter ramp used to avoid zipper noise and clicks (spec §4).
    // Audio thread only once prepared. Never allocates.
    class LinearSmoothedValue
    {
    public:
        // Sets the ramp length and jumps to the current target.
        void prepare (double sampleRate, double rampSeconds) noexcept
        {
            const double samples = sampleRate * rampSeconds;
            rampLength = samples < 1.0 ? 1 : (int) std::lround (samples);
            setCurrentAndTarget (target);
        }

        void setCurrentAndTarget (float value) noexcept
        {
            if (! std::isfinite (value))
                return;

            current = target = value;
            countdown = 0;
            step = 0.0f;
        }

        void setTarget (float value) noexcept
        {
            // Exact comparison is intended: re-sending the same target every
            // block must not restart the ramp.
           #if defined(__GNUC__) || defined(__clang__)
            #pragma GCC diagnostic push
            #pragma GCC diagnostic ignored "-Wfloat-equal"
           #endif
            const bool unchanged = value == target;
           #if defined(__GNUC__) || defined(__clang__)
            #pragma GCC diagnostic pop
           #endif

            if (! std::isfinite (value) || unchanged)
                return;

            target = value;

            if (rampLength <= 1)
            {
                setCurrentAndTarget (value);
                return;
            }

            countdown = rampLength;
            step = (target - current) / (float) countdown;
        }

        float getNext() noexcept
        {
            if (countdown <= 0)
                return target;

            --countdown;
            current = (countdown == 0) ? target : current + step;
            return current;
        }

        void skip (int numSamples) noexcept
        {
            if (numSamples <= 0 || countdown <= 0)
                return;

            if (numSamples >= countdown)
            {
                current = target;
                countdown = 0;
                return;
            }

            current += step * (float) numSamples;
            countdown -= numSamples;
        }

        bool isSmoothing() const noexcept { return countdown > 0; }
        float getCurrent() const noexcept { return current; }
        float getTarget() const noexcept { return target; }
        int getRampLengthSamples() const noexcept { return rampLength; }

    private:
        float current = 0.0f;
        float target = 0.0f;
        float step = 0.0f;
        int countdown = 0;
        int rampLength = 1;
    };

    inline float decibelsToGain (float decibels) noexcept
    {
        return decibels <= -100.0f ? 0.0f : std::pow (10.0f, decibels * 0.05f);
    }

    template <typename T>
    constexpr T clampValue (T value, T low, T high) noexcept
    {
        return value < low ? low : (high < value ? high : value);
    }
}
