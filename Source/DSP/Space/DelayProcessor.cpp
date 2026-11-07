#include "DelayProcessor.h"
#include "Core/ParameterIds.h"

#include <algorithm>
#include <cmath>

namespace vm
{
    namespace
    {
        float onePoleCoef (float cutoffHz, double sampleRate) noexcept
        {
            const double c = 1.0 - std::exp (-6.283185307179586 * (double) cutoffHz / sampleRate);
            return (float) clampValue (c, 0.0, 1.0);
        }
    }

    void DelayProcessor::prepare (const ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

        const int maxSamples = (int) std::ceil (space::maxDelaySeconds * sampleRate) + 4;
        lineL.prepare (maxSamples);
        lineR.prepare (maxSamples);

        time.prepare (sampleRate, space::timeGlideSeconds);
        feedback.prepare (sampleRate, param::smoothingSeconds);
        width.prepare (sampleRate, param::smoothingSeconds);

        hpCoef = onePoleCoef (space::feedbackHighPassHz, sampleRate);
        lpCoef = onePoleCoef (space::feedbackLowPassHz, sampleRate);

        reset();
    }

    void DelayProcessor::reset() noexcept
    {
        lineL.reset();
        lineR.reset();
        hpState[0] = hpState[1] = lpState[0] = lpState[1] = 0.0f;

        time.setCurrentAndTarget (time.getTarget());
        feedback.setCurrentAndTarget (feedback.getTarget());
        width.setCurrentAndTarget (width.getTarget());
    }

    void DelayProcessor::setParams (float timeSamples, float newFeedback, float newWidth) noexcept
    {
        if (! timeInitialised)
        {
            time.setCurrentAndTarget (timeSamples); // first value: no glide from zero
            timeInitialised = true;
        }

        time.setTarget (timeSamples);
        feedback.setTarget (newFeedback * space::feedbackMaxGain);
        width.setTarget (newWidth);
    }

    void DelayProcessor::process (float inL, float inR, float& wetL, float& wetR) noexcept
    {
        const float d = time.getNext();
        const float fb = feedback.getNext();
        const float w = width.getNext();

        const float yL = lineL.read (d);
        const float yR = lineR.read (d);

        float fbL = fb * ((1.0f - w) * yL + w * yR);
        float fbR = fb * ((1.0f - w) * yR + w * yL);

        // Darker, thinner repeats: low-pass, then high-pass (x - lowpass).
        const auto shape = [this] (int ch, float x)
        {
            lpState[ch] += lpCoef * (x - lpState[ch]);
            const float low = lpState[ch];
            hpState[ch] += hpCoef * (low - hpState[ch]);
            const float y = low - hpState[ch];
            return std::fabs (y) < 1.0e-20f ? 0.0f : std::tanh (y);
        };

        fbL = shape (0, fbL);
        fbR = shape (1, fbR);

        const float x = 0.5f * (inL + inR);
        lineL.push (x + fbL);
        lineR.push (x * (1.0f - w) + fbR);

        wetL = yL;
        wetR = yR;
    }
}
