#pragma once

#include "AudioBufferView.h"
#include "NoiseGenerator.h"
#include "SmoothedValue.h"

#include <array>
#include <cstdint>

namespace vm
{
    // Character of the vinyl surface noise.
    //   Normal: clean vinyl, a few soft ticks
    //   Dusty:  dusty record, many small fine ticks and a bit more hiss
    //   Heavy:  badly worn, fewer but much bigger pops and thumps, and more hiss
    enum class CrackleType
    {
        normal = 0,
        dusty,
        heavy
    };

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
        void setCrackleType (CrackleType type) noexcept;   // glides over 50 ms: no steps

        // Number of crackle ticks started so far (diagnostics and tests).
        std::uint64_t getTicksStarted() const noexcept { return ticksStarted; }

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

        // Per-type character, smoothed so changing the type never makes a step.
        CrackleType crackleType = CrackleType::normal;
        LinearSmoothedValue hissGain, peakGain, density;
        float hissNow = 0.0f, peakNow = 0.0f, densityNow = 0.0f;
        float minTickSeconds = 0.0f, maxTickSeconds = 0.0f;
        std::uint64_t ticksStarted = 0;
        float highpassCoef = 0.0f;
    };
}
