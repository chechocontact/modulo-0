#pragma once

#include "ColorParameters.h"
#include "Core/AudioBufferView.h"
#include "Core/SmoothedValue.h"

#include <array>

namespace vm
{
    // Focus: keeps the band [low, high]. A 12 dB/oct high-pass at `low` and a
    // 12 dB/oct low-pass at `high` (TPT state-variable filters), and Amount
    // crossfades from the untouched signal (0) to the band-limited one (1).
    // At Amount = 0 the filter is skipped (bit-transparent).
    class FocusFilter
    {
    public:
        void prepare (const ProcessSpec& spec);
        void reset() noexcept;
        void setParams (float lowHz, float highHz, float amount) noexcept;

        void advance() noexcept;
        float process (int channel, float input) noexcept;

    private:
        struct Svf { float ic1 = 0.0f, ic2 = 0.0f; };

        void updateCoefficients (float lowHz, float highHz) noexcept;

        double sampleRate = 44100.0;
        std::array<Svf, maxChannels> highpass, lowpass;
        LinearSmoothedValue low, high, amount;
        float amountNow = 0.0f;
        bool active = false;
        float hpA1 = 0, hpA2 = 0, hpA3 = 0;
        float lpA1 = 0, lpA2 = 0, lpA3 = 0;
    };
}
