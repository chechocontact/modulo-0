#include "TestProcessor.h"
#include "Core/ParameterIds.h"

namespace vm
{
    float TestProcessor::targetGain() const noexcept
    {
        const float g = decibelsToGain (parameters.gainDb);
        return parameters.invertPolarity ? -g : g;
    }

    void TestProcessor::prepare (const ProcessSpec& spec)
    {
        gain.prepare (spec.sampleRate, param::smoothingSeconds);
        reset();
    }

    void TestProcessor::reset() noexcept
    {
        gain.setCurrentAndTarget (targetGain());
    }

    void TestProcessor::setParameters (const Parameters& newParameters) noexcept
    {
        parameters = newParameters;
        parameters.gainDb = clampValue (parameters.gainDb, -60.0f, 24.0f);
        gain.setTarget (targetGain());
    }

    void TestProcessor::process (AudioBufferView block) noexcept
    {
        const int numSamples = block.getNumSamples();
        const int numChannels = block.getNumChannels();

        if (! gain.isSmoothing())
        {
            const float g = gain.getCurrent();

            if (g == 1.0f)
                return;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                float* data = block.getChannel (ch);

                for (int i = 0; i < numSamples; ++i)
                    data[i] *= g;
            }

            return;
        }

        for (int i = 0; i < numSamples; ++i)
        {
            const float g = gain.getNext();

            for (int ch = 0; ch < numChannels; ++ch)
                block.getChannel (ch)[i] *= g;
        }
    }
}
