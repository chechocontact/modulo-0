#pragma once

#include "Core/AudioBufferView.h"
#include "Core/SmoothedValue.h"
#include "GrainBuffer.h"
#include "GrainGenerator.h"
#include "GrainVoice.h"
#include "GrainWindow.h"
#include "MovementParameters.h"

#include <array>

namespace vm
{
    // Capture buffer + grain voices + feedback. In place, stereo or mono.
    //
    //   out = in * cos (grain * pi/2) + grains * sin (grain * pi/2)    (equal power)
    //   buffer <- in + feedback * tanh (grains)       (capture stops in Frozen)
    //
    // With Grain at zero the output equals the input bit for bit.
    class GranularEngine
    {
    public:
        void prepare (const ProcessSpec& spec);
        void reset() noexcept;
        void process (AudioBufferView block) noexcept;

        void setParams (float grain, float feedback, MovementStyle style) noexcept;

        int getActiveVoices() const noexcept;

    private:
        void killVoices() noexcept;
        void startGrain (const GrainSpec& spec) noexcept;

        double sampleRate = 44100.0;
        int channels = 2;

        GrainBuffer buffer;
        GrainWindow window;
        GrainGenerator generator;
        std::array<GrainVoice, (std::size_t) movement::maxVoices> voices {};

        LinearSmoothedValue grainAmount;
        LinearSmoothedValue feedbackAmount;
        MovementStyle style = MovementStyle::cloud;
        bool anyVoices = false;
    };
}
