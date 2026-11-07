#pragma once

#include "ColorParameters.h"
#include "Core/AudioBufferView.h"
#include "Core/SmoothedValue.h"

#include <array>

namespace vm
{
    // Bipolar tilt EQ. A one-pole low-pass splits the signal at 700 Hz; the low
    // band gets -tone * 6 dB and the high band +tone * 6 dB, so tone > 0 is
    // brighter and tone < 0 darker, with the same overall level in the middle.
    // At tone = 0 the processor is skipped (bit-transparent).
    class ToneProcessor
    {
    public:
        void prepare (const ProcessSpec& spec);
        void reset() noexcept;
        void setTone (float tone) noexcept { toneSmoother.setTarget (clampValue (tone, -1.0f, 1.0f)); }

        void advance() noexcept;
        float process (int channel, float input) noexcept;

    private:
        double sampleRate = 44100.0;
        LinearSmoothedValue toneSmoother;
        std::array<float, maxChannels> lowState {};
        float coef = 0.0f;
        float lowGain = 1.0f, highGain = 1.0f;
        bool active = false;
    };
}
