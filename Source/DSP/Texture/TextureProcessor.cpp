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
        drop.prepare (spec.sampleRate);
        degrade.prepare (s);

        dropExcursion = (float) (texture::dropMaxExcursionSeconds * spec.sampleRate);

        applyParameters();
        reset();
    }

    void TextureProcessor::reset() noexcept
    {
        wowFlutter.reset();
        drop.reset();
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
        q.flutterDepth = clean (p.flutterDepth, q.flutterDepth, 0.0f, 1.0f);
        q.flutterRate = clean (p.flutterRate, q.flutterRate, texture::flutterRateMinHz, texture::flutterRateMaxHz);
        q.vinylDrop = clean (p.vinylDrop, q.vinylDrop, 0.0f, 1.0f);
        q.degrade = clean (p.degrade, q.degrade, 0.0f, 1.0f);

        parameters = q;
        applyParameters();
    }

    void TextureProcessor::applyParameters() noexcept
    {
        wowFlutter.setParams ({ parameters.wowDepth, parameters.wowRate, texture::wowShape,
                                parameters.flutterDepth, parameters.flutterRate, texture::flutterVariation });
        drop.setAmount (parameters.vinylDrop);

        const float d = parameters.degrade;
        degrade.setParams (d, 1.0f - 0.8f * d, 0.7f * d);
    }

    void TextureProcessor::process (AudioBufferView block) noexcept
    {
        const int numSamples = block.getNumSamples();
        const int channels = std::min (block.getNumChannels(), numChannels);

        for (int i = 0; i < numSamples; ++i)
        {
            drop.advance();
            degrade.advance();

            const float dropNow = drop.getEnvelope() * drop.getAmount();
            wowFlutter.advance (dropNow * dropExcursion);

            // During a drop the signal also dips in level (up to -35 %).
            const float dropGain = 1.0f - 0.35f * dropNow;

            for (int ch = 0; ch < channels; ++ch)
            {
                float* data = block.getChannel (ch);

                float y = wowFlutter.process (ch, data[i]);
                y = degrade.process (ch, y);
                y *= dropGain;

                data[i] = y;
            }
        }
    }
}
