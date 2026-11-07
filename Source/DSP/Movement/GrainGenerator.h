#pragma once

#include "Core/NoiseGenerator.h"
#include "MovementParameters.h"

namespace vm
{
    // What the engine needs to start one grain.
    struct GrainSpec
    {
        int length = 1;      // samples
        double speed = 1.0;  // read speed (1 = original pitch)
        double delay = 0.0;  // how far behind the write head it starts, in samples
        float gainL = 1.0f;
        float gainR = 1.0f;
    };

    // Decides when grains start and what each one looks like, according to the Style.
    class GrainGenerator
    {
    public:
        void prepare (double newSampleRate, int newBufferSize, int newChannels) noexcept;
        void reset() noexcept;
        void setStyle (MovementStyle newStyle) noexcept;

        // Call once per sample. True when a grain should start now.
        bool shouldSpawn() noexcept;
        GrainSpec nextGrain() noexcept;

        bool freezesCapture() const noexcept { return config.freezeCapture; }

        // Gain that keeps the level of the overlapping grains about constant
        // whatever the style (power-matched to the input).
        float getNormalisation() const noexcept { return normalisation; }

    private:
        void scheduleNext() noexcept;

        double sampleRate = 44100.0;
        int bufferSize = 0;
        int channels = 2;

        MovementStyle style = MovementStyle::cloud;
        movement::StyleConfig config = movement::styleConfig (MovementStyle::cloud);
        float normalisation = 1.0f;

        double samplesToNext = 0.0;
        double driftPhase = 0.0;
        NoiseGenerator random { 0x1234ABCDu };
    };
}
