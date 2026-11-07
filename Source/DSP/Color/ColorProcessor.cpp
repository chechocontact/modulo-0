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
        focus.prepare (s);
        chorus.prepare (s);

        applyParameters();
        reset();
    }

    void ColorProcessor::reset() noexcept
    {
        saturation.reset();
        tone.reset();
        focus.reset();
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
        q.focusLow = clean (p.focusLow, q.focusLow, color::focusLowMinHz, color::focusLowMaxHz);
        q.focusHigh = clean (p.focusHigh, q.focusHigh, color::focusHighMinHz, color::focusHighMaxHz);
        q.focusAmount = clean (p.focusAmount, q.focusAmount, 0.0f, 1.0f);
        q.chorusAmount = clean (p.chorusAmount, q.chorusAmount, 0.0f, 1.0f);
        q.chorusRate = clean (p.chorusRate, q.chorusRate, color::chorusRateMinHz, color::chorusRateMaxHz);
        q.chorusDepth = clean (p.chorusDepth, q.chorusDepth, 0.0f, 1.0f);
        q.chorusWidth = clean (p.chorusWidth, q.chorusWidth, 0.0f, 1.0f);
        q.chorusMix = clean (p.chorusMix, q.chorusMix, 0.0f, 1.0f);
        q.type = (int) p.type >= 0 && (int) p.type <= 2 ? p.type : q.type;
        q.chorusMode = (int) p.chorusMode >= 0 && (int) p.chorusMode <= 2 ? p.chorusMode : q.chorusMode;

        parameters = q;
        applyParameters();
    }

    void ColorProcessor::applyParameters() noexcept
    {
        saturation.setParams (parameters.drive, parameters.type);
        tone.setTone (parameters.tone);
        focus.setParams (parameters.focusLow, parameters.focusHigh, parameters.focusAmount);
        chorus.setParams ({ parameters.chorusAmount, parameters.chorusRate, parameters.chorusDepth,
                            parameters.chorusWidth, parameters.chorusMix, parameters.chorusMode });
    }

    void ColorProcessor::process (AudioBufferView block) noexcept
    {
        const int numSamples = block.getNumSamples();
        const int channels = std::min (block.getNumChannels(), numChannels);

        for (int i = 0; i < numSamples; ++i)
        {
            saturation.advance();
            tone.advance();
            focus.advance();
            chorus.advance();

            for (int ch = 0; ch < channels; ++ch)
            {
                float* data = block.getChannel (ch);

                float y = saturation.process (ch, data[i]);
                y = tone.process (ch, y);
                y = focus.process (ch, y);
                y = chorus.process (ch, y);

                data[i] = y;
            }
        }
    }
}
