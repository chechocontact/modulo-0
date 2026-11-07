#include "GranularEngine.h"
#include "Core/ParameterIds.h"

#include <algorithm>
#include <cmath>

namespace vm
{
    void GranularEngine::prepare (const ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        channels = std::clamp (spec.numChannels, 1, maxChannels);

        buffer.prepare (sampleRate, movement::bufferSeconds);
        generator.prepare (sampleRate, buffer.getSize(), channels);

        grainAmount.prepare (sampleRate, param::smoothingSeconds);
        feedbackAmount.prepare (sampleRate, param::smoothingSeconds);

        reset();
    }

    void GranularEngine::reset() noexcept
    {
        buffer.clear();
        generator.reset();
        killVoices();

        grainAmount.setCurrentAndTarget (grainAmount.getTarget());
        feedbackAmount.setCurrentAndTarget (feedbackAmount.getTarget());
    }

    void GranularEngine::setParams (float grain, float feedback, MovementStyle newStyle) noexcept
    {
        grainAmount.setTarget (grain);
        feedbackAmount.setTarget (feedback);

        if (newStyle != style)
        {
            style = newStyle;
            generator.setStyle (style);
        }
    }

    int GranularEngine::getActiveVoices() const noexcept
    {
        int n = 0;
        for (const auto& v : voices)
            n += v.active ? 1 : 0;
        return n;
    }

    void GranularEngine::killVoices() noexcept
    {
        for (auto& v : voices)
            v.active = false;

        anyVoices = false;
    }

    void GranularEngine::startGrain (const GrainSpec& spec) noexcept
    {
        for (auto& v : voices)
        {
            if (v.active)
                continue;

            v.active = true;
            v.age = 0;
            v.length = spec.length;
            v.speed = spec.speed;
            v.position = buffer.wrap ((double) buffer.getWriteIndex() - spec.delay);
            v.gainL = spec.gainL;
            v.gainR = spec.gainR;
            anyVoices = true;
            return;
        }
        // All voices busy: this grain is skipped.
    }

    void GranularEngine::process (AudioBufferView block) noexcept
    {
        const int numSamples = block.getNumSamples();
        const int chs = std::min (block.getNumChannels(), channels);
        if (chs == 0)
            return;

        float* left = block.getChannel (0);
        float* right = chs > 1 ? block.getChannel (1) : nullptr;
        const bool stereo = channels > 1;
        const bool capture = ! generator.freezesCapture();

        for (int i = 0; i < numSamples; ++i)
        {
            const float grain = grainAmount.getNext();
            const float feedback = feedbackAmount.getNext() * movement::feedbackMaxGain;

            const float inL = left[i];
            const float inR = right != nullptr ? right[i] : inL;

            float wetL = 0.0f, wetR = 0.0f;
            const bool active = grain > 0.0f || grainAmount.isSmoothing();

            if (active)
            {
                if (generator.shouldSpawn())
                    startGrain (generator.nextGrain());

                if (anyVoices)
                {
                    for (auto& v : voices)
                        if (v.active)
                            v.render (buffer, window, stereo, wetL, wetR);
                }

                const float norm = generator.getNormalisation();
                wetL *= norm;
                wetR *= norm;
            }
            else if (anyVoices)
            {
                killVoices(); // grain fully faded out: nothing audible is lost
            }

            if (active)
            {
                left[i] = inL * (1.0f - grain) + wetL * grain;
                if (right != nullptr)
                    right[i] = inR * (1.0f - grain) + wetR * grain;
            }

            if (capture)
            {
                float wl = inL + feedback * std::tanh (wetL);
                float wr = inR + feedback * std::tanh (wetR);

                // Flush tiny values so the loop never produces denormals.
                if (std::fabs (wl) < 1.0e-20f) wl = 0.0f;
                if (std::fabs (wr) < 1.0e-20f) wr = 0.0f;

                buffer.write (0, wl);
                buffer.write (1, wr);
                buffer.advance();
            }
        }
    }
}
