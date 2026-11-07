#include "ColorProcessor.h"

#include <algorithm>
#include <cmath>

namespace vm
{
    void ColorProcessor::prepare (const ProcessSpec& spec)
    {
        numChannels = std::clamp (spec.numChannels, 1, maxChannels);

        ProcessSpec s = spec;
        s.numChannels = numChannels;

        saturation.prepare (s);
        tone.prepare (s);
        chorus.prepare (s);

        applyParameters();
        reset();
    }

    void ColorProcessor::reset() noexcept
    {
        saturation.reset();
        tone.reset();
        chorus.reset();
    }

    void ColorProcessor::setParameters (const ColorParameters& p) noexcept
    {
        // Non-finite values keep the previous value.
        const auto clean = [] (float value, float previous, float low, float high)
        {
            return std::isfinite (value) ? clampValue (value, low, high) : previous;
        };

        ColorParameters q = parameters;
        q.drive = clean (p.drive, q.drive, 0.0f, 1.0f);
        q.tone = clean (p.tone, q.tone, -1.0f, 1.0f);
        q.magicChorus = clean (p.magicChorus, q.magicChorus, 0.0f, 1.0f);
        q.type = (int) p.type >= 0 && (int) p.type <= 2 ? p.type : q.type;

        parameters = q;
        applyParameters();
    }

    void ColorProcessor::applyParameters() noexcept
    {
        saturation.setParams (parameters.drive, parameters.type);
        tone.setTone (parameters.tone);
        chorus.setParams ({ parameters.magicChorus, color::magicChorusRateHz, color::magicChorusDepth,
                            color::magicChorusWidth, color::magicChorusMix, color::magicChorusMode });
    }

    void ColorProcessor::process (AudioBufferView block) noexcept
    {
        const int numSamples = block.getNumSamples();
        const int channels = std::min (block.getNumChannels(), numChannels);

        for (int i = 0; i < numSamples; ++i)
        {
            saturation.advance();
            tone.advance();
            chorus.advance();

            for (int ch = 0; ch < channels; ++ch)
            {
                float* data = block.getChannel (ch);

                float y = saturation.process (ch, data[i]);
                y = tone.process (ch, y);
                y = chorus.process (ch, y);

                data[i] = y;
            }
        }
    }
}
