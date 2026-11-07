#include "DegradeProcessor.h"
#include "Core/ParameterIds.h"

#include <algorithm>
#include <cmath>

namespace vm
{
    namespace
    {
        constexpr double pi = 3.14159265358979323846;
        constexpr float butterworthK = 1.41421356f; // 1/Q for Q = 1/sqrt(2)

        inline float envCoef (double seconds, double sampleRate) noexcept
        {
            return 1.0f - (float) std::exp (-1.0 / (seconds * sampleRate));
        }
    }

    void DegradeProcessor::prepare (const ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        numChannels = std::clamp (spec.numChannels, 1, maxChannels);

        amount.prepare (sampleRate, param::smoothingSeconds);
        bandwidth.prepare (sampleRate, 0.05);
        smoothing.prepare (sampleRate, param::smoothingSeconds);

        fastAttack = envCoef (0.0001, sampleRate);
        fastRelease = envCoef (0.003, sampleRate);
        slowAttack = envCoef (0.008, sampleRate);
        slowRelease = envCoef (0.060, sampleRate);

        reset();
    }

    void DegradeProcessor::reset() noexcept
    {
        amount.setCurrentAndTarget (amount.getTarget());
        bandwidth.setCurrentAndTarget (bandwidth.getTarget());
        smoothing.setCurrentAndTarget (smoothing.getTarget());
        amountNow = amount.getCurrent();
        smoothingNow = smoothing.getCurrent();
        active = amountNow > 0.0f;
        clearStates();
        updateCoefficients (bandwidth.getCurrent());
    }

    void DegradeProcessor::clearStates() noexcept
    {
        for (auto& s : states)
            s = ChannelState {};
    }

    void DegradeProcessor::setParams (float newAmount, float newBandwidth, float newSmoothing) noexcept
    {
        amount.setTarget (clampValue (newAmount, 0.0f, 1.0f));
        bandwidth.setTarget (clampValue (newBandwidth, 0.0f, 1.0f));
        smoothing.setTarget (clampValue (newSmoothing, 0.0f, 1.0f));
    }

    void DegradeProcessor::updateCoefficients (float bw) noexcept
    {
        const double nyquistLimit = 0.45 * sampleRate;
        const double lowpassHz = std::min (nyquistLimit, 1200.0 * std::pow (10.0, (double) bw)); // 1.2k .. 12k
        const double highpassHz = 500.0 * std::pow (0.08, (double) bw);                          // 500 .. 40

        const float gl = (float) std::tan (pi * lowpassHz / sampleRate);
        lpA1 = 1.0f / (1.0f + gl * (gl + butterworthK));
        lpA2 = gl * lpA1;
        lpA3 = gl * lpA2;

        const float gh = (float) std::tan (pi * highpassHz / sampleRate);
        hpA1 = 1.0f / (1.0f + gh * (gh + butterworthK));
        hpA2 = gh * hpA1;
        hpA3 = gh * hpA2;
    }

    void DegradeProcessor::advance() noexcept
    {
        const bool bandwidthMoving = bandwidth.isSmoothing();
        const float bw = bandwidth.getNext();

        if (bandwidthMoving)
            updateCoefficients (bw);

        amountNow = amount.getNext();
        smoothingNow = smoothing.getNext();

        const bool nowActive = amountNow > 0.0f;

        if (nowActive && ! active)
            clearStates(); // coming back from "skipped": drop stale filter memory

        active = nowActive;
    }

    float DegradeProcessor::process (int channel, float input) noexcept
    {
        if (! active)
            return input;

        auto& s = states[(std::size_t) channel];
        float x = input;

        // 1. Transient softening: fast envelope vs slow envelope.
        const float level = std::abs (x);
        s.fastEnv += (level > s.fastEnv ? fastAttack : fastRelease) * (level - s.fastEnv);
        s.slowEnv += (level > s.slowEnv ? slowAttack : slowRelease) * (level - s.slowEnv);

        if (smoothingNow > 0.0f && s.slowEnv > 1.0e-4f)
        {
            const float ratio = s.fastEnv / s.slowEnv;

            if (ratio > 1.0f)
                x *= std::max (0.35f, 1.0f / (1.0f + smoothingNow * (ratio - 1.0f) * 0.6f));
        }

        // 2. Mild saturation, unity gain for small signals.
        const float drive = 1.0f + 1.5f * amountNow;
        x = std::tanh (drive * x) / drive;

        // 3. High-pass, then low-pass (TPT state-variable filters).
        {
            const float v3 = x - s.highpass.ic2;
            const float v1 = hpA1 * s.highpass.ic1 + hpA2 * v3;
            const float v2 = s.highpass.ic2 + hpA2 * s.highpass.ic1 + hpA3 * v3;
            s.highpass.ic1 = 2.0f * v1 - s.highpass.ic1;
            s.highpass.ic2 = 2.0f * v2 - s.highpass.ic2;
            x = x - butterworthK * v1 - v2;
        }
        {
            const float v3 = x - s.lowpass.ic2;
            const float v1 = lpA1 * s.lowpass.ic1 + lpA2 * v3;
            const float v2 = s.lowpass.ic2 + lpA2 * s.lowpass.ic1 + lpA3 * v3;
            s.lowpass.ic1 = 2.0f * v1 - s.lowpass.ic1;
            s.lowpass.ic2 = 2.0f * v2 - s.lowpass.ic2;
            x = v2;
        }

        return input + amountNow * (x - input);
    }
}
