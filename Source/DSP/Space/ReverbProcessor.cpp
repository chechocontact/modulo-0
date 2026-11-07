#include "ReverbProcessor.h"

#include <algorithm>
#include <cmath>

namespace vm
{
    namespace
    {
        constexpr double twoPi = 6.283185307179586;

        // Hadamard output signs: two orthogonal rows, so left and right decorrelate.
        constexpr float rowL[8] = { 1, 1, 1, 1, -1, -1, -1, -1 };
        constexpr float rowR[8] = { 1, -1, 1, -1, 1, -1, 1, -1 };

        float onePoleCoef (float cutoffHz, double sampleRate) noexcept
        {
            const double c = 1.0 - std::exp (-twoPi * (double) cutoffHz / sampleRate);
            return (float) clampValue (c, 0.0, 1.0);
        }

        float rt60For (ReverbType type, float decay) noexcept
        {
            const auto c = space::typeConfig (type);
            const float d = clampValue (decay, 0.0f, 1.0f);
            return c.rt60Min * std::pow (c.rt60Max / c.rt60Min, d);
        }
    }

    // ---- Ring -------------------------------------------------------------

    void ReverbProcessor::Ring::prepare (int minSize)
    {
        std::size_t n = 16;
        while ((int) n < minSize + 8)
            n <<= 1;

        data.assign (n, 0.0f);
        mask = (int) n - 1;
        write = 0;
    }

    void ReverbProcessor::Ring::clear() noexcept
    {
        std::fill (data.begin(), data.end(), 0.0f);
        write = 0;
    }

    float ReverbProcessor::Ring::read (float delay) const noexcept
    {
        const int whole = (int) delay;
        const float frac = delay - (float) whole;
        const float a = data[(std::size_t) ((write - whole) & mask)];
        const float b = data[(std::size_t) ((write - whole - 1) & mask)];
        return a + frac * (b - a);
    }

    // ---- Engine -----------------------------------------------------------

    void ReverbProcessor::Engine::prepare (double newSampleRate, ReverbType newType)
    {
        sampleRate = newSampleRate;
        type = newType;
        config = space::typeConfig (type);

        preDelaySamples = std::max (1, (int) std::lround ((double) config.preDelayMs * 0.001 * sampleRate));
        for (auto& p : preDelay)
            p.prepare (preDelaySamples + 4);

        for (int i = 0; i < numDiffusers; ++i)
        {
            diffuserSamples[i] = std::max (2, (int) std::lround ((double) config.diffuserMs[(std::size_t) i] * 0.001 * sampleRate));
            for (auto& d : diffuser)
                d[i].prepare (diffuserSamples[i] + 4);
        }

        modDepth = (float) (0.0004 * sampleRate); // 0.4 ms
        for (int i = 0; i < numLines; ++i)
        {
            lineSeconds[i] = config.lineMs[(std::size_t) i] * 0.001f;
            lineSamples[i] = lineSeconds[i] * (float) sampleRate;
            line[i].prepare ((int) std::ceil (lineSamples[i] + modDepth) + 4);
            lfoStep[i] = (float) ((0.31 + 0.073 * i) / sampleRate);
        }

        outputBase = type == ReverbType::hall ? 0.95f : 0.6f; // Hall's longer lines sound quieter for the same gain
        hpCoef = onePoleCoef (space::reverbHighPassHz, sampleRate);
        dampCoef = onePoleCoef (config.dampingHz, sampleRate);

        // |H| of the damping one-pole at 1 kHz: c / |1 - (1 - c) e^(-jw)|
        const double w = twoPi * 1000.0 / sampleRate;
        const double a = 1.0 - (double) dampCoef;
        const double re = 1.0 - a * std::cos (w), im = a * std::sin (w);
        dampMidGain = (float) ((double) dampCoef / std::sqrt (re * re + im * im));
        clear();
    }

    void ReverbProcessor::Engine::clear() noexcept
    {
        for (auto& p : preDelay) p.clear();
        for (auto& d : diffuser) for (auto& r : d) r.clear();
        for (auto& l : line) l.clear();

        hpState[0] = hpState[1] = 0.0f;
        for (int i = 0; i < numLines; ++i)
        {
            lowpass[i] = 0.0f;
            lfoPhase[i] = 0.125f * (float) i;
        }
    }

    void ReverbProcessor::Engine::updateGains (float rt60) noexcept
    {
        rt60 = std::max (0.05f, rt60);
        // Loop gain for the requested RT60, with the damping filter's loss at 1 kHz
        // taken out so the damping only shortens the decay of the highs.
        for (int i = 0; i < numLines; ++i)
            gain[i] = std::min (0.9995f, std::exp (-6.907755f * lineSeconds[i] / rt60) / dampMidGain);

        // Longer decays build up more energy for the same input: compensate so the
        // wet level stays roughly constant whatever the Decay.
        outputScale = outputBase * std::pow (std::max (rt60, 0.2f), -0.25f);
    }

    void ReverbProcessor::Engine::process (float inL, float inR, float& outL, float& outR) noexcept
    {
        float x[2] = { inL, inR };

        for (int ch = 0; ch < 2; ++ch)
        {
            // High-pass, then pre-delay.
            hpState[ch] += hpCoef * (x[ch] - hpState[ch]);
            float v = x[ch] - hpState[ch];

            preDelay[ch].push (v);
            v = preDelay[ch].readInt (preDelaySamples);

            // Schroeder all-pass diffusers.
            for (int d = 0; d < numDiffusers; ++d)
            {
                Ring& r = diffuser[ch][d];
                const float delayed = r.readInt (diffuserSamples[d]);
                const float w = v + config.diffuserGain * delayed;
                r.push (w);
                v = delayed - config.diffuserGain * w;
            }
            x[ch] = v;
        }

        // Read, damp and apply the loop gain of every line.
        float y[numLines];
        for (int i = 0; i < numLines; ++i)
        {
            lfoPhase[i] += lfoStep[i];
            if (lfoPhase[i] >= 1.0f)
                lfoPhase[i] -= 1.0f;

            const float delay = lineSamples[i] + modDepth * (1.0f + (float) std::sin (twoPi * lfoPhase[i]));
            float s = line[i].read (delay);

            lowpass[i] += dampCoef * (s - lowpass[i]);
            if (std::fabs (lowpass[i]) < 1.0e-20f)
                lowpass[i] = 0.0f;

            y[i] = lowpass[i];
        }

        // Output taps.
        float oL = 0.0f, oR = 0.0f;
        for (int i = 0; i < numLines; ++i)
        {
            oL += rowL[i] * y[i];
            oR += rowR[i] * y[i];
        }
        outL = oL * outputScale;
        outR = oR * outputScale;

        // Hadamard mixing (fast Walsh-Hadamard transform, orthonormal), then write back.
        float h[numLines];
        for (int i = 0; i < numLines; ++i)
            h[i] = y[i] * gain[i];

        for (int len = 1; len < numLines; len <<= 1)
            for (int i = 0; i < numLines; i += len << 1)
                for (int j = i; j < i + len; ++j)
                {
                    const float a = h[j], b = h[j + len];
                    h[j] = a + b;
                    h[j + len] = a - b;
                }

        constexpr float norm = 0.35355339f; // 1 / sqrt (8)
        for (int i = 0; i < numLines; ++i)
            line[i].push (h[i] * norm + 0.5f * ((i & 1) ? x[1] : x[0]));
    }

    // ---- ReverbProcessor ----------------------------------------------------

    ReverbProcessor::ReverbProcessor() = default;

    void ReverbProcessor::prepare (const ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

        engines[0].prepare (sampleRate, ReverbType::room);
        engines[1].prepare (sampleRate, ReverbType::plate);
        engines[2].prepare (sampleRate, ReverbType::hall);

        decay.prepare (sampleRate, 0.2);
        fadeStep = (float) (1.0 / std::max (1.0, space::typeFadeSeconds * sampleRate));

        reset();
    }

    void ReverbProcessor::reset() noexcept
    {
        for (auto& e : engines)
            e.clear();

        const int t = (int) type;
        for (int i = 0; i < 3; ++i)
        {
            engineGain[(std::size_t) i] = i == t ? 1.0f : 0.0f;
            running[(std::size_t) i] = i == t;
        }
        active = t;

        decay.setCurrentAndTarget (decay.getTarget());
        gainCountdown = 0;
    }

    void ReverbProcessor::setParams (float newDecay, ReverbType newType) noexcept
    {
        decay.setTarget (newDecay);

        if ((int) newType >= 0 && (int) newType <= 2 && newType != type)
        {
            type = newType;
            active = (int) newType;

            if (! running[(std::size_t) active])
            {
                engines[(std::size_t) active].clear();
                running[(std::size_t) active] = true;
                gainCountdown = 0; // make sure its gains are computed at once
            }
        }
    }

    void ReverbProcessor::process (float inL, float inR, float& wetL, float& wetR) noexcept
    {
        const float d = decay.getNext();

        if (gainCountdown <= 0)
        {
            for (int i = 0; i < 3; ++i)
                if (running[(std::size_t) i])
                    engines[(std::size_t) i].updateGains (rt60For ((ReverbType) i, d));

            gainCountdown = gainUpdateInterval;
        }
        --gainCountdown;

        float sumL = 0.0f, sumR = 0.0f;

        for (int i = 0; i < 3; ++i)
        {
            if (! running[(std::size_t) i])
                continue;

            float& g = engineGain[(std::size_t) i];
            g = i == active ? std::min (1.0f, g + fadeStep) : std::max (0.0f, g - fadeStep);

            float oL, oR;
            engines[(std::size_t) i].process (inL, inR, oL, oR);
            sumL += g * oL;
            sumR += g * oR;

            if (i != active && g <= 0.0f)
                running[(std::size_t) i] = false; // faded out: stop spending CPU on it
        }

        wetL = sumL;
        wetR = sumR;
    }
}
