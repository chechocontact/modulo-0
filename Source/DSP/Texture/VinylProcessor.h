#pragma once

#include "Core/AudioBufferView.h"
#include "Core/SmoothedValue.h"
#include "NoiseGenerator.h"
#include "TextureParameters.h"
#include "Core/ParameterIds.h"

#include <array>
#include <cstdint>

namespace vm
{
    // Vinyl surface noise and the occasional "drop" (a brief warp of pitch and
    // level, as if the record flexed). Generates noise only: Texture adds it to
    // the signal, independent of the input level.
    //
    //   hiss     pink noise, high-passed, level proportional to Amount
    //   crackle  random bursts. Density and level scale with Amount.
    //              Normal  sparse, soft ticks
    //              Dusty   many small ticks
    //              Heavy   few, loud, longer pops
    //   drop     raised-cosine envelope 0..1..0 over 0.15-0.45 s, at random
    //            intervals (about every 30 s at Drop = 0 .. 5 s at Drop = 1)
    //
    // Call advance() once per sample frame, then next() for each channel.
    class VinylProcessor
    {
    public:
        void prepare (const ProcessSpec& spec);
        void reset() noexcept;                   // snaps smoothers, re-seeds, reschedules
        void setParams (float amount, CrackleType type, float dropAmount) noexcept;

        // Starts a drop now with the given duration (also used by tests).
        void forceDrop (double seconds) noexcept;

        void advance() noexcept;
        float next (int channel) noexcept;

        float getDropEnvelope() const noexcept { return dropEnvelope; }  // 0..1
        float getDropAmount() const noexcept { return dropAmountNow; }   // 0..1, smoothed

    private:
        struct Channel
        {
            NoiseGenerator hissNoise;
            NoiseGenerator crackleNoise;
            float highpassState = 0.0f;
            int burstRemaining = 0;
            int burstLength = 1;
            float burstAmplitude = 0.0f;
        };

        void scheduleNextDrop() noexcept;

        double sampleRate = 44100.0;
        int numChannels = 2;
        std::array<Channel, maxChannels> channels;
        NoiseGenerator dropNoise;

        LinearSmoothedValue amount, dropAmount;
        CrackleType type = CrackleType::normal;
        float amountNow = 0.0f;
        float dropAmountNow = 0.0f;
        float highpassCoef = 0.0f;

        bool dropActive = false;
        long long dropPosition = 0;
        long long dropLength = 1;
        long long dropCountdown = 0;
        float dropEnvelope = 0.0f;
    };
}
