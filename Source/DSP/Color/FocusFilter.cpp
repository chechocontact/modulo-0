#include "FocusFilter.h"
#include "Core/ParameterIds.h"

#include <algorithm>
#include <cmath>

namespace vm
{
    namespace
    {
        constexpr double pi = 3.14159265358979323846;
        constexpr float k = 1.41421356f; // 1/Q, Butterworth
    }

    void FocusFilter::prepare (const ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        low.prepare (sampleRate, 0.05);
        high.prepare (sampleRate, 0.05);
        amount.prepare (sampleRate, param::smoothingSeconds);
        reset();
    }

    void FocusFilter::reset() noexcept
    {
        low.setCurrentAndTarget (low.getTarget());
        high.setCurrentAndTarget (high.getTarget());
        amount.setCurrentAndTarget (amount.getTarget());
        amountNow = amount.getCurrent();
        active = amountNow > 0.0f;
        highpass.fill (Svf {});
        lowpass.fill (Svf {});
        updateCoefficients (low.getCurrent(), high.getCurrent());
    }

    void FocusFilter::setParams (float lowHz, float highHz, float newAmount) noexcept
    {
        low.setTarget (clampValue (lowHz, color::focusLowMinHz, color::focusLowMaxHz));
        high.setTarget (clampValue (highHz, color::focusHighMinHz, color::focusHighMaxHz));
        amount.setTarget (clampValue (newAmount, 0.0f, 1.0f));
    }

    void FocusFilter::updateCoefficients (float lowHz, float highHz) noexcept
    {
        const double lp = std::min (0.45 * sampleRate, (double) highHz);

        const float gh = (float) std::tan (pi * (double) lowHz / sampleRate);
        hpA1 = 1.0f / (1.0f + gh * (gh + k));
        hpA2 = gh * hpA1;
        hpA3 = gh * hpA2;

        const float gl = (float) std::tan (pi * lp / sampleRate);
        lpA1 = 1.0f / (1.0f + gl * (gl + k));
        lpA2 = gl * lpA1;
        lpA3 = gl * lpA2;
    }

    void FocusFilter::advance() noexcept
    {
        const bool moving = low.isSmoothing() || high.isSmoothing();
        const float l = low.getNext();
        const float h = high.getNext();

        if (moving)
            updateCoefficients (l, h);

        amountNow = amount.getNext();
        const bool nowActive = amountNow > 0.0f;

        if (nowActive && ! active)
        {
            highpass.fill (Svf {});
            lowpass.fill (Svf {});
        }

        active = nowActive;
    }

    float FocusFilter::process (int channel, float input) noexcept
    {
        if (! active)
            return input;

        auto& hp = highpass[(std::size_t) channel];
        auto& lp = lowpass[(std::size_t) channel];

        float x = input;
        {
            const float v3 = x - hp.ic2;
            const float v1 = hpA1 * hp.ic1 + hpA2 * v3;
            const float v2 = hp.ic2 + hpA2 * hp.ic1 + hpA3 * v3;
            hp.ic1 = 2.0f * v1 - hp.ic1;
            hp.ic2 = 2.0f * v2 - hp.ic2;
            x = x - k * v1 - v2;
        }
        {
            const float v3 = x - lp.ic2;
            const float v1 = lpA1 * lp.ic1 + lpA2 * v3;
            const float v2 = lp.ic2 + lpA2 * lp.ic1 + lpA3 * v3;
            lp.ic1 = 2.0f * v1 - lp.ic1;
            lp.ic2 = 2.0f * v2 - lp.ic2;
            x = v2;
        }

        return input + amountNow * (x - input);
    }
}
