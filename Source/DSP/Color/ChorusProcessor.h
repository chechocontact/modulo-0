#pragma once

#include "ColorParameters.h"
#include "Core/AudioBufferView.h"
#include "Core/DelayLine.h"
#include "Core/SmoothedValue.h"

#include <array>

namespace vm
{
    // Magic Chorus: modulated-delay voices mixed with the dry signal.
    //
    //   Classic    1 voice, ~10 ms centre, sine LFO
    //   Dimension  2 voices in opposite phase, short (~6 ms) and subtle
    //   Ensemble   3 voices 120 degrees apart at slightly different rates, ~12-18 ms
    //
    //   Amount  scales the modulation depth; 0 switches the chorus off
    //   Rate    LFO speed (voices run at mode-specific multiples of it)
    //   Depth   modulation depth
    //   Width   L/R LFO phase offset, 0 = in phase, 1 = opposite (stereo only)
    //   Mix     dry/wet
    //
    // Switching Mode crossfades between the two voice sets over 30 ms.
    // The wet path has a constant delay of several ms, so the chorus is not
    // latency-compensated (that delay is the effect itself).
    //
    // Call advance() once per sample frame, then process() for each channel.
    class ChorusProcessor
    {
    public:
        struct Params
        {
            float amount = 0.0f;
            float rate = 0.8f;
            float depth = 0.5f;
            float width = 0.5f;
            float mix = 0.5f;
            ChorusMode mode = ChorusMode::classic;
        };

        void prepare (const ProcessSpec& spec);
        void reset() noexcept;
        void setParams (const Params& p) noexcept;

        void advance() noexcept;
        float process (int channel, float input) noexcept;

    private:
        static constexpr int maxVoices = 3;
        static constexpr int numModes = 3;

        float readVoices (int mode, int channel) const noexcept;

        double sampleRate = 44100.0;
        int numChannels = 2;
        std::array<DelayLine, maxChannels> lines;

        LinearSmoothedValue amount, rate, depth, width, mix, modeFade;
        ChorusMode targetMode = ChorusMode::classic;
        int currentMode = 0;
        int previousMode = 0;

        double phases[numModes][maxVoices] {};
        float delaySamples[numModes][maxChannels][maxVoices] {};

        float mixNow = 0.0f;
        float fadeNow = 1.0f;
        bool fading = false;
        bool active = false;
    };
}
