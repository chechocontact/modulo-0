#pragma once

#include "AudioBufferView.h"
#include "NoiseGenerator.h"
#include "SmoothedValue.h"

#include <array>

namespace vm
{
    // Vinyl surface noise: pink hiss plus random crackle ticks. It is generated
    // and added at the very end of the plugin (MasterProcessor), after every
    // module, the master dry/wet and the output gain, so none of them can
    // change how the vinyl sounds. Amount 0 = off.
    //
    // Call advance() once per sample frame, then next() for each channel.
    class VinylNoise
    {
    public:
        void prepare (const ProcessSpec& spec);
        void reset() noexcept;                   // snaps the smoother, re-seeds
        void setAmount (float amount) noexcept;

        void advance() noexcept;
        float next (int channel) noexcept;

        bool isIdle() const noexcept { return amountNow <= 0.0f && ! amount.isSmoothing(); }

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

        double sampleRate = 44100.0;
        std::array<Channel, maxChannels> channels;
        LinearSmoothedValue amount;
        float amountNow = 0.0f;
        float highpassCoef = 0.0f;
    };
}
