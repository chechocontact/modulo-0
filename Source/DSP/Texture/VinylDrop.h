#pragma once

#include "Core/NoiseGenerator.h"
#include "Core/SmoothedValue.h"

#include <algorithm>
#include <cmath>

namespace vm
{
    // The vinyl "drop": a brief warp of pitch and level, as if the record
    // flexed. It modifies the signal, so it lives inside Texture (the noise
    // part of the vinyl does not: see VinylNoise).
    //
    // A raised-cosine envelope 0..1..0 lasting 0.15-0.45 s, at random intervals
    // (about every 30 s at Drop = 0+, every 5 s at Drop = 1). Drop = 0 never fires.
    class VinylDrop
    {
    public:
        void prepare (double newSampleRate)
        {
            sampleRate = newSampleRate;
            amount.prepare (sampleRate, 0.1);
            reset();
        }

        void reset() noexcept
        {
            amount.setCurrentAndTarget (amount.getTarget());
            amountNow = amount.getCurrent();
            noise.setSeed (0x3001u);
            active = false;
            envelope = 0.0f;
            scheduleNext();
        }

        void setAmount (float value) noexcept { amount.setTarget (clampValue (value, 0.0f, 1.0f)); }

        void force (double seconds) noexcept
        {
            active = true;
            position = 0;
            length = std::max (1LL, (long long) (seconds * sampleRate));
        }

        // Once per sample frame.
        void advance() noexcept
        {
            amountNow = amount.getNext();

            if (active)
            {
                ++position;

                if (position >= length)
                {
                    active = false;
                    envelope = 0.0f;
                    scheduleNext();
                }
                else
                {
                    envelope = 0.5f * (1.0f - (float) std::cos (6.283185307179586 * (double) position / (double) length));
                }
            }
            else if (amountNow > 0.0f && --countdown <= 0)
            {
                force (0.15 + 0.30 * (double) noise.uniform01());
            }
        }

        float getEnvelope() const noexcept { return envelope; } // 0..1
        float getAmount() const noexcept { return amountNow; }  // 0..1, smoothed

    private:
        void scheduleNext() noexcept
        {
            const float depth = std::max (amount.getTarget(), 0.1f);
            const float meanSeconds = 30.0f + (5.0f - 30.0f) * depth;
            const float u = std::max (noise.uniform01(), 1.0e-6f);
            countdown = (long long) ((double) std::max (2.0f, -std::log (u) * meanSeconds) * sampleRate);
        }

        double sampleRate = 44100.0;
        NoiseGenerator noise;
        LinearSmoothedValue amount;
        float amountNow = 0.0f;
        bool active = false;
        long long position = 0, length = 1, countdown = 0;
        float envelope = 0.0f;
    };
}
