#pragma once

#include "ColorParameters.h"
#include "Core/AudioBufferView.h"
#include "Core/SmoothedValue.h"

#include <array>

namespace vm
{
    // Harmonic saturation with three curves and first-order antiderivative
    // anti-aliasing (ADAA): instead of sampling f(x[n]), it outputs the average
    // of f between x[n-1] and x[n], computed exactly from the antiderivative F.
    // That removes much of the aliasing with no latency and no oversampling.
    //
    //   Tube     tanh(u + bias) - tanh(bias): asymmetric, even + odd harmonics,
    //            followed by a DC blocker
    //   Tape     u / sqrt(1 + u^2): soft, symmetric (odd harmonics), then a
    //            low-pass that closes as Drive rises (tape head loss)
    //   Console  u - u^3/3, clipped at |u| = 1: symmetric, tighter and drier
    //
    // Drive 0..1 = 0..+24 dB into the curve, with level compensation so the
    // loudness stays roughly constant. Below Drive = 0.1 the effect is
    // crossfaded in, and at Drive = 0 it is skipped (bit-transparent).
    //
    // Call advance() once per sample frame, then process() for each channel.
    class SaturationProcessor
    {
    public:
        void prepare (const ProcessSpec& spec);
        void reset() noexcept;
        void setParams (float drive, ColorType type) noexcept;
        void setAntiAliasing (bool enabled) noexcept { antiAliasing = enabled; } // tests / future Oversampling control

        void advance() noexcept;
        float process (int channel, float input) noexcept;

    private:
        struct ChannelState
        {
            double previousU = 0.0;
            float lowpass = 0.0f;
            float dcIn = 0.0f, dcOut = 0.0f;
        };

        double shape (double u) const noexcept;
        double antiderivative (double u) const noexcept;
        void updateMakeup() noexcept;

        double sampleRate = 44100.0;
        std::array<ChannelState, maxChannels> states;

        LinearSmoothedValue drive;
        ColorType type = ColorType::tube;
        ColorType activeType = ColorType::tube;
        bool antiAliasing = true;

        float driveNow = 0.0f;
        double preGain = 1.0;
        double makeup = 1.0;
        float blend = 0.0f;
        float blendNow = 0.0f;
        LinearSmoothedValue typeFade; // dips the effect while the curve is switched
        float tapeCoef = 1.0f;
        float dcCoef = 0.999f;
        bool active = false;
    };
}
