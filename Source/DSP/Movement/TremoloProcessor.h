#pragma once

#include "Core/AudioBufferView.h"
#include "Core/SmoothedValue.h"

namespace vm
{
    // Sine tremolo. Gain swings between 1 and (1 - depth). In stereo the right
    // channel runs a quarter cycle behind the left. Depth 0 is bit-transparent.
    class TremoloProcessor
    {
    public:
        void prepare (const ProcessSpec& spec);
        void reset() noexcept;
        void process (AudioBufferView block) noexcept;

        void setParams (float depth, float rateHz) noexcept;

    private:
        double sampleRate = 44100.0;
        double phase = 0.0;
        LinearSmoothedValue depth;
        LinearSmoothedValue rate;
    };
}
