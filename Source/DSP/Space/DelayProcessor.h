#pragma once

#include "Core/AudioBufferView.h"
#include "Core/DelayLine.h"
#include "Core/SmoothedValue.h"
#include "SpaceParameters.h"

namespace vm
{
    // Stereo delay with cross-fed feedback.
    //
    //   x = (L + R) / 2
    //   left line  <- x            + fb * ((1 - w) * yL + w * yR)
    //   right line <- x * (1 - w)  + fb * ((1 - w) * yR + w * yL)
    //
    // Width w = 0: two identical lines (mono delay). w = 1: ping-pong, the
    // echoes bounce between left and right. The feedback path is high-passed,
    // low-passed and soft-limited, so repeats get darker and can never run away.
    // Returns the wet signal only; SpaceProcessor adds it to the dry signal.
    class DelayProcessor
    {
    public:
        void prepare (const ProcessSpec& spec);
        void reset() noexcept;

        // Targets (audio thread). Time is in samples.
        void setParams (float timeSamples, float feedback, float width) noexcept;

        void process (float inL, float inR, float& wetL, float& wetR) noexcept;

    private:
        double sampleRate = 44100.0;
        DelayLine lineL, lineR;
        LinearSmoothedValue time, feedback, width;
        float hpCoef = 0.0f, lpCoef = 0.0f;
        float hpState[2] {}, lpState[2] {};
        bool timeInitialised = false;
    };
}
