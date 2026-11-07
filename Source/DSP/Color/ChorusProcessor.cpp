#include "ChorusProcessor.h"
#include "Core/ParameterIds.h"

#include <algorithm>
#include <cmath>

namespace vm
{
    namespace
    {
        constexpr double twoPi = 6.283185307179586476925286766559;

        struct ModeSpec
        {
            int voices;
            float centreMs[3];
            float excursionMs[3];  // at depth = amount = 1
            float rateMult[3];
            float phaseOffset[3];  // cycles
            float wetGain;
        };

        constexpr ModeSpec modeSpecs[3] = {
            // Classic
            { 1, { 10.0f, 0.0f, 0.0f }, { 3.5f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 1.0f },
            // Dimension
            { 2, { 6.0f, 6.5f, 0.0f }, { 1.2f, 1.2f, 0.0f }, { 0.5f, 0.5f, 0.0f }, { 0.0f, 0.5f, 0.0f }, 0.7071f },
            // Ensemble
            { 3, { 12.0f, 15.0f, 18.0f }, { 3.0f, 3.5f, 3.0f }, { 1.0f, 1.13f, 0.87f }, { 0.0f, 0.3333f, 0.6667f }, 0.5774f },
        };

        inline double wrap01 (double p) noexcept { return p - std::floor (p); }
    }

    void ChorusProcessor::prepare (const ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        numChannels = std::clamp (spec.numChannels, 1, maxChannels);

        const int maxDelay = (int) std::ceil (sampleRate * 0.040);
        for (auto& l : lines)
            l.prepare (maxDelay);

        amount.prepare (sampleRate, param::smoothingSeconds);
        rate.prepare (sampleRate, 0.1);
        depth.prepare (sampleRate, 0.05);
        width.prepare (sampleRate, 0.05);
        mix.prepare (sampleRate, param::smoothingSeconds);
        modeFade.prepare (sampleRate, 0.03);

        reset();
    }

    void ChorusProcessor::reset() noexcept
    {
        for (auto* s : { &amount, &rate, &depth, &width, &mix })
            s->setCurrentAndTarget (s->getTarget());

        for (auto& l : lines)
            l.reset();

        currentMode = previousMode = (int) targetMode;
        modeFade.setCurrentAndTarget (1.0f);
        fading = false;
        fadeNow = 1.0f;

        for (auto& m : phases)
            for (auto& p : m)
                p = 0.0;

        for (auto& m : delaySamples)
            for (auto& c : m)
                for (auto& v : c)
                    v = 0.0f;

        mixNow = 0.0f;
        active = amount.getCurrent() > 0.0f;
    }

    void ChorusProcessor::setParams (const Params& p) noexcept
    {
        amount.setTarget (clampValue (p.amount, 0.0f, 1.0f));
        rate.setTarget (clampValue (p.rate, color::chorusRateMinHz, color::chorusRateMaxHz));
        depth.setTarget (clampValue (p.depth, 0.0f, 1.0f));
        width.setTarget (clampValue (p.width, 0.0f, 1.0f));
        mix.setTarget (clampValue (p.mix, 0.0f, 1.0f));
        targetMode = p.mode;
    }

    void ChorusProcessor::advance() noexcept
    {
        const float a = amount.getNext();
        const float r = rate.getNext();
        const float d = depth.getNext();
        const float w = width.getNext();
        const float m = mix.getNext();

        active = a > 0.0f;

        // Mode change: start a crossfade from the old voice set to the new one.
        if ((int) targetMode != currentMode)
        {
            previousMode = currentMode;
            currentMode = (int) targetMode;
            modeFade.setCurrentAndTarget (0.0f);
            modeFade.setTarget (1.0f);
            fading = true;
        }

        fadeNow = modeFade.getNext();

        if (fading && ! modeFade.isSmoothing())
            fading = false;

        // The effect fades out smoothly as Amount approaches 0.
        mixNow = m * std::min (1.0f, a * 20.0f);

        const float depthNow = d * a;
        const int modes[2] = { currentMode, previousMode };
        const int count = fading ? 2 : 1;

        for (int mi = 0; mi < count; ++mi)
        {
            const int mode = modes[mi];
            const ModeSpec& spec = modeSpecs[mode];

            for (int v = 0; v < spec.voices; ++v)
            {
                phases[mode][v] = wrap01 (phases[mode][v] + (double) r * (double) spec.rateMult[v] / sampleRate);

                for (int ch = 0; ch < numChannels; ++ch)
                {
                    const double phase = phases[mode][v] + (double) spec.phaseOffset[v] + 0.5 * (double) w * ch;
                    const float ms = spec.centreMs[v] + depthNow * spec.excursionMs[v] * (float) std::sin (twoPi * phase);
                    delaySamples[mode][ch][v] = ms * 0.001f * (float) sampleRate;
                }
            }
        }
    }

    float ChorusProcessor::readVoices (int mode, int channel) const noexcept
    {
        const ModeSpec& spec = modeSpecs[mode];
        float sum = 0.0f;

        for (int v = 0; v < spec.voices; ++v)
            sum += lines[(std::size_t) channel].read (delaySamples[mode][channel][v]);

        return sum * spec.wetGain;
    }

    float ChorusProcessor::process (int channel, float input) noexcept
    {
        lines[(std::size_t) channel].push (input); // keep the history running even when idle

        if (! active)
            return input;

        float wet = readVoices (currentMode, channel);

        if (fading)
        {
            const float old = readVoices (previousMode, channel);
            wet = old + (wet - old) * fadeNow;
        }

        return input + mixNow * (wet - input);
    }
}
