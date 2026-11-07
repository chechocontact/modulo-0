#include "TextureProcessor.h"

#include <algorithm>
#include <cmath>

namespace vm
{
    void TextureProcessor::prepare (const ProcessSpec& spec)
    {
        numChannels = std::clamp (spec.numChannels, 1, maxChannels);

        ProcessSpec s = spec;
        s.numChannels = numChannels;

        wowFlutter.prepare (s);
        vinyl.prepare (s);
        degrade.prepare (s);

        dropExcursion = (float) (texture::dropMaxExcursionSeconds * spec.sampleRate);

        applyParameters();
        reset();
    }

    void TextureProcessor::reset() noexcept
    {
        wowFlutter.reset();
        vinyl.reset();
        degrade.reset();
    }

    void TextureProcessor::setParameters (const TextureParameters& p) noexcept
    {
        // Non-finite values (corrupt automation or state) keep the previous value.
        const auto clean = [] (float value, float previous, float low, float high)
        {
            return std::isfinite (value) ? clampValue (value, low, high) : previous;
        };

        TextureParameters q = parameters;
        q.wowDepth = clean (p.wowDepth, q.wowDepth, 0.0f, 1.0f);
        q.wowRate = clean (p.wowRate, q.wowRate, texture::wowRateMinHz, texture::wowRateMaxHz);
        q.wowShape = clean (p.wowShape, q.wowShape, 0.0f, 1.0f);
        q.flutterDepth = clean (p.flutterDepth, q.flutterDepth, 0.0f, 1.0f);
        q.flutterRate = clean (p.flutterRate, q.flutterRate, texture::flutterRateMinHz, texture::flutterRateMaxHz);
        q.flutterVariation = clean (p.flutterVariation, q.flutterVariation, 0.0f, 1.0f);
        q.vinylAmount = clean (p.vinylAmount, q.vinylAmount, 0.0f, 1.0f);
        q.vinylDrop = clean (p.vinylDrop, q.vinylDrop, 0.0f, 1.0f);
        q.degradeAmount = clean (p.degradeAmount, q.degradeAmount, 0.0f, 1.0f);
        q.degradeBandwidth = clean (p.degradeBandwidth, q.degradeBandwidth, 0.0f, 1.0f);
        q.degradeSmoothing = clean (p.degradeSmoothing, q.degradeSmoothing, 0.0f, 1.0f);
        q.crackleType = (int) p.crackleType >= 0 && (int) p.crackleType <= 2 ? p.crackleType : q.crackleType;

        parameters = q;
        applyParameters();
    }

    void TextureProcessor::applyParameters() noexcept
    {
        wowFlutter.setParams ({ parameters.wowDepth, parameters.wowRate, parameters.wowShape,
                                parameters.flutterDepth, parameters.flutterRate, parameters.flutterVariation });
        vinyl.setParams (parameters.vinylAmount, parameters.crackleType, parameters.vinylDrop);
        degrade.setParams (parameters.degradeAmount, parameters.degradeBandwidth, parameters.degradeSmoothing);
    }

    void TextureProcessor::process (AudioBufferView block) noexcept
    {
        const int numSamples = block.getNumSamples();
        const int channels = std::min (block.getNumChannels(), numChannels);

        for (int i = 0; i < numSamples; ++i)
        {
            vinyl.advance();
            degrade.advance();

            const float drop = vinyl.getDropEnvelope() * vinyl.getDropAmount();
            wowFlutter.advance (drop * dropExcursion);

            // During a drop the signal also dips in level (up to -35 %).
            const float dropGain = 1.0f - 0.35f * drop;

            for (int ch = 0; ch < channels; ++ch)
            {
                float* data = block.getChannel (ch);

                float y = wowFlutter.process (ch, data[i]);
                y = degrade.process (ch, y);
                y *= dropGain;
                y += vinyl.next (ch);

                data[i] = y;
            }
        }
    }
}
