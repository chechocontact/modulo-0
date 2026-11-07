#include "TremoloProcessor.h"
#include "Core/ParameterIds.h"
#include "MovementParameters.h"

#include <algorithm>
#include <cmath>

namespace vm
{
    void TremoloProcessor::prepare (const ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        depth.prepare (sampleRate, param::smoothingSeconds);
        rate.prepare (sampleRate, param::smoothingSeconds);
        reset();
    }

    void TremoloProcessor::reset() noexcept
    {
        phase = 0.0;
        depth.setCurrentAndTarget (depth.getTarget());
        rate.setCurrentAndTarget (rate.getTarget());
    }

    void TremoloProcessor::setParams (float newDepth, float rateHz) noexcept
    {
        depth.setTarget (newDepth);
        rate.setTarget (rateHz);
    }

    void TremoloProcessor::process (AudioBufferView block) noexcept
    {
        const int numSamples = block.getNumSamples();
        const int channels = block.getNumChannels();
        constexpr double twoPi = 6.283185307179586;

        for (int i = 0; i < numSamples; ++i)
        {
            const float d = depth.getNext();
            const float r = rate.getNext();

            phase += (double) r / sampleRate;
            if (phase >= 1.0)
                phase -= 1.0;

            if (d <= 0.0f && ! depth.isSmoothing())
                continue; // exactly transparent

            for (int ch = 0; ch < channels; ++ch)
            {
                double p = phase + (ch == 1 ? (double) movement::tremoloStereoPhase : 0.0);
                if (p >= 1.0)
                    p -= 1.0;

                const float gain = 1.0f - d * 0.5f * (1.0f + (float) std::sin (twoPi * p));
                block.getChannel (ch)[i] *= gain;
            }
        }
    }
}
