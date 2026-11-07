#include "SpaceProcessor.h"
#include "Core/ParameterIds.h"

#include <algorithm>
#include <cmath>

namespace vm
{
    void SpaceProcessor::prepare (const ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        numChannels = std::clamp (spec.numChannels, 1, maxChannels);

        ProcessSpec s = spec;
        s.numChannels = numChannels;

        delay.prepare (s);
        reverb.prepare (s);

        delayAmount.prepare (sampleRate, param::smoothingSeconds);
        reverbAmount.prepare (sampleRate, param::smoothingSeconds);
        widthAmount.prepare (sampleRate, param::smoothingSeconds);

        applyParameters();
        reset();
    }

    void SpaceProcessor::reset() noexcept
    {
        delay.reset();
        reverb.reset();
        delayAmount.setCurrentAndTarget (delayAmount.getTarget());
        reverbAmount.setCurrentAndTarget (reverbAmount.getTarget());
        widthAmount.setCurrentAndTarget (widthAmount.getTarget());
        reverbActive = false;
    }

    void SpaceProcessor::setTempo (double newBpm) noexcept
    {
        if (std::isfinite (newBpm) && newBpm > 1.0)
            bpm = std::min (newBpm, 999.0);
    }

    double SpaceProcessor::getDelayTimeSeconds() const noexcept
    {
        double seconds = parameters.sync
            ? (double) space::divisionBeats[(std::size_t) space::divisionIndex (parameters.time)] * 60.0 / bpm
            : (double) space::freeTimeSeconds (parameters.time);

        return clampValue (seconds, 0.001, space::maxDelaySeconds - 0.05);
    }

    void SpaceProcessor::setParameters (const SpaceParameters& p) noexcept
    {
        // Non-finite values keep the previous value.
        const auto clean = [] (float value, float previous)
        {
            return std::isfinite (value) ? clampValue (value, 0.0f, 1.0f) : previous;
        };

        SpaceParameters q = parameters;
        q.delay = clean (p.delay, q.delay);
        q.time = clean (p.time, q.time);
        q.feedback = clean (p.feedback, q.feedback);
        q.mix = clean (p.mix, q.mix);
        q.decay = clean (p.decay, q.decay);
        q.width = clean (p.width, q.width);
        q.sync = p.sync;
        q.type = (int) p.type >= 0 && (int) p.type <= 2 ? p.type : q.type;

        parameters = q;
        applyParameters();
    }

    void SpaceProcessor::applyParameters() noexcept
    {
        delayAmount.setTarget (parameters.delay);
        reverbAmount.setTarget (parameters.mix);
        widthAmount.setTarget (parameters.width);

        delay.setParams ((float) (getDelayTimeSeconds() * sampleRate), parameters.feedback, parameters.width);
        reverb.setParams (parameters.decay, parameters.type);
    }

    void SpaceProcessor::process (AudioBufferView block) noexcept
    {
        // The host tempo may change from block to block while Sync is on.
        if (parameters.sync)
            applyParameters();

        const int numSamples = block.getNumSamples();
        const int channels = std::min (block.getNumChannels(), numChannels);
        if (channels == 0)
            return;

        float* left = block.getChannel (0);
        float* right = channels > 1 ? block.getChannel (1) : nullptr;

        for (int i = 0; i < numSamples; ++i)
        {
            const float dm = delayAmount.getNext();
            const float rm = reverbAmount.getNext();
            const float w = widthAmount.getNext();

            const float inL = left[i];
            const float inR = right != nullptr ? right[i] : inL;

            float yL, yR;
            delay.process (inL, inR, yL, yR);

            const float zL = inL + dm * yL;
            const float zR = inR + dm * yR;

            float rL = 0.0f, rR = 0.0f;
            const bool reverbOn = rm > 0.0f || reverbAmount.isSmoothing();

            if (reverbOn)
            {
                reverb.process (zL, zR, rL, rR);

                // Stereo opening: scale the side component of the reverb.
                // 0 = mono, 50 % = the reverb's natural image, 100 % = twice as wide.
                const float mid = 0.5f * (rL + rR);
                const float side = 0.5f * (rL - rR) * (w * space::reverbSideGainAtFullWidth);
                rL = mid + side;
                rR = mid - side;
                reverbActive = true;
            }
            else if (reverbActive)
            {
                reverb.reset(); // no stale tail comes back when the knob is raised again
                reverbActive = false;
            }

            if (right != nullptr)
            {
                left[i] = zL + rm * rL;
                right[i] = zR + rm * rR;
            }
            else
            {
                left[i] = 0.5f * ((zL + zR) + rm * (rL + rR));
            }
        }
    }
}
