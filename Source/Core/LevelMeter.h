#pragma once

#include "AudioBufferView.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>

namespace vm
{
    // Peak and RMS meter for the input and output of the plugin.
    //
    // The audio thread calls process() (no allocation, no locks, per-sample so the
    // result does not depend on the block size). The interface thread reads the
    // values with getPeak() / getRms(), which are lock-free atomic loads.
    //
    //   peak: instant attack, falls with a 0.5 s time constant
    //   RMS:  one-pole mean square with a 0.3 s time constant
    //
    // Non-finite samples are ignored (counted as silence), so a NaN upstream can
    // never freeze the meter.
    class LevelMeter
    {
    public:
        void prepare (double sampleRate) noexcept
        {
            peakFall = (float) std::exp (-1.0 / (0.5 * sampleRate));
            rmsCoef = (float) (1.0 - std::exp (-1.0 / (0.3 * sampleRate)));
            reset();
        }

        void reset() noexcept
        {
            for (std::size_t ch = 0; ch < (std::size_t) maxChannels; ++ch)
            {
                peakState[ch] = 0.0f;
                meanSquare[ch] = 0.0f;
                peakOut[ch].store (0.0f, std::memory_order_relaxed);
                rmsOut[ch].store (0.0f, std::memory_order_relaxed);
            }
        }

        void process (AudioBufferView block) noexcept
        {
            const int numSamples = block.getNumSamples();

            for (int ch = 0; ch < block.getNumChannels(); ++ch)
            {
                const float* data = block.getChannel (ch);
                float peak = peakState[(std::size_t) ch];
                float ms = meanSquare[(std::size_t) ch];

                for (int i = 0; i < numSamples; ++i)
                {
                    float a = std::fabs (data[i]);
                    if (! std::isfinite (a))
                        a = 0.0f;

                    peak = std::max (a, peak * peakFall);
                    ms += rmsCoef * (a * a - ms);
                }

                if (peak < 1.0e-20f) peak = 0.0f; // no denormals
                if (ms < 1.0e-20f) ms = 0.0f;

                peakState[(std::size_t) ch] = peak;
                meanSquare[(std::size_t) ch] = ms;
                peakOut[(std::size_t) ch].store (peak, std::memory_order_relaxed);
                rmsOut[(std::size_t) ch].store (std::sqrt (ms), std::memory_order_relaxed);
            }
        }

        // Any thread. Linear values (1.0 = full scale).
        float getPeak (int channel) const noexcept { return peakOut[index (channel)].load (std::memory_order_relaxed); }
        float getRms (int channel) const noexcept { return rmsOut[index (channel)].load (std::memory_order_relaxed); }

        static float toDecibels (float linear) noexcept
        {
            return linear <= 1.0e-5f ? -100.0f : 20.0f * std::log10 (linear);
        }

    private:
        static std::size_t index (int channel) noexcept
        {
            return (std::size_t) std::clamp (channel, 0, maxChannels - 1);
        }

        float peakFall = 0.99f;
        float rmsCoef = 0.001f;
        std::array<float, (std::size_t) maxChannels> peakState {}, meanSquare {};
        std::array<std::atomic<float>, (std::size_t) maxChannels> peakOut {}, rmsOut {};
    };
}
