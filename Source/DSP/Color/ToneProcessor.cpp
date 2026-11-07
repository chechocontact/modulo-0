#include "ToneProcessor.h"
#include "Core/ParameterIds.h"

#include <cmath>

namespace vm
{
    void ToneProcessor::prepare (const ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        toneSmoother.prepare (sampleRate, param::smoothingSeconds);
        coef = 1.0f - (float) std::exp (-6.283185307179586 * (double) color::tonePivotHz / sampleRate);
        reset();
    }

    void ToneProcessor::reset() noexcept
    {
        toneSmoother.setCurrentAndTarget (toneSmoother.getTarget());
        lowState.fill (0.0f);
        const float t = toneSmoother.getCurrent();
        active = t < 0.0f || t > 0.0f;
        const float dB = t * color::toneMaxDb;
        highGain = std::pow (10.0f, dB / 20.0f);
        lowGain = 1.0f / highGain;
    }

    void ToneProcessor::advance() noexcept
    {
        const bool moving = toneSmoother.isSmoothing();
        const float t = toneSmoother.getNext();

        if (moving)
        {
            const float dB = t * color::toneMaxDb;
            highGain = std::pow (10.0f, dB / 20.0f);
            lowGain = 1.0f / highGain;
        }

        const bool nowActive = t < 0.0f || t > 0.0f;

        if (nowActive && ! active)
            lowState.fill (0.0f);

        active = nowActive;
    }

    float ToneProcessor::process (int channel, float input) noexcept
    {
        if (! active)
            return input;

        float& low = lowState[(std::size_t) channel];
        low += coef * (input - low);
        return low * lowGain + (input - low) * highGain;
    }
}
