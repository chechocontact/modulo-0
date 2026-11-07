#pragma once

#include "Core/AudioBufferView.h"
#include "Core/SmoothedValue.h"

#include <array>

namespace vm
{
    // Signal degradation: softened transients -> mild tanh saturation ->
    // high-pass -> low-pass (the band-limiting set by Bandwidth).
    //
    //   Amount     crossfades from the clean input (0) to the degraded signal (1)
    //              and sets the saturation drive. At 0 the processor is skipped.
    //   Bandwidth  0 = narrow (500 Hz - 1.2 kHz, telephone) .. 1 = wide (40 Hz - 12 kHz)
    //   Smoothing  amount of transient softening (envelope-ratio gain reduction)
    //
    // Call advance() once per sample frame, then process() for each channel.
    class DegradeProcessor
    {
    public:
        void prepare (const ProcessSpec& spec);
        void reset() noexcept;
        void setParams (float amount, float bandwidth, float smoothing) noexcept;

        void advance() noexcept;
        float process (int channel, float input) noexcept;

    private:
        struct Svf
        {
            float ic1 = 0.0f, ic2 = 0.0f;
        };

        struct ChannelState
        {
            Svf highpass, lowpass;
            float fastEnv = 0.0f, slowEnv = 0.0f;
        };

        void updateCoefficients (float bandwidth) noexcept;
        void clearStates() noexcept;

        double sampleRate = 44100.0;
        int numChannels = 2;
        std::array<ChannelState, maxChannels> states;

        LinearSmoothedValue amount, bandwidth, smoothing;
        float amountNow = 0.0f, smoothingNow = 0.0f;
        bool active = false;

        // TPT state-variable filter coefficients (shared by both channels).
        float hpA1 = 0, hpA2 = 0, hpA3 = 0;
        float lpA1 = 0, lpA2 = 0, lpA3 = 0;

        float fastAttack = 0, fastRelease = 0, slowAttack = 0, slowRelease = 0;
    };
}
