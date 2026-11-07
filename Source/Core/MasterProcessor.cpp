#include "MasterProcessor.h"
#include "DenormalGuard.h"
#include "ParameterIds.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace vm
{
    MasterProcessor::MasterProcessor()
    {
        mix.setCurrentAndTarget (1.0f);
        outputGain.setCurrentAndTarget (1.0f);
        active.setCurrentAndTarget (1.0f);
    }

    void MasterProcessor::prepare (const ProcessSpec& spec)
    {
        currentSpec = spec;
        currentSpec.maxBlockSize = std::max (1, spec.maxBlockSize);
        currentSpec.numChannels = std::clamp (spec.numChannels, 1, maxChannels);

        dryStorage.assign ((std::size_t) (maxChannels * currentSpec.maxBlockSize), 0.0f);

        mix.prepare (spec.sampleRate, param::smoothingSeconds);
        outputGain.prepare (spec.sampleRate, param::smoothingSeconds);
        active.prepare (spec.sampleRate, param::smoothingSeconds);

        engine.prepare (currentSpec);
        vinyl.prepare (currentSpec);
        reset();
        prepared = true;
    }

    void MasterProcessor::reset() noexcept
    {
        mix.setCurrentAndTarget (mix.getTarget());
        outputGain.setCurrentAndTarget (outputGain.getTarget());
        active.setCurrentAndTarget (active.getTarget());
        engine.reset();
        vinyl.reset();
        engineNeedsReset = false;
    }

    void MasterProcessor::setParameters (const MasterParameters& p) noexcept
    {
        // Non-finite values (corrupt automation/state) keep the previous value.
        const auto sanitise = [] (float value, float previous, float low, float high)
        {
            return std::isfinite (value) ? clampValue (value, low, high) : previous;
        };

        parameters.inputGainDb = sanitise (p.inputGainDb, parameters.inputGainDb, param::gainMinDb, param::gainMaxDb);
        parameters.outputGainDb = sanitise (p.outputGainDb, parameters.outputGainDb, param::gainMinDb, param::gainMaxDb);
        parameters.mix = sanitise (p.mix, parameters.mix, 0.0f, 1.0f);
        parameters.vinylAmount = sanitise (p.vinylAmount, parameters.vinylAmount, 0.0f, 1.0f);
        parameters.bypass = p.bypass;

        engine.setInputGainDb (parameters.inputGainDb);
        vinyl.setAmount (parameters.vinylAmount);
        outputGain.setTarget (decibelsToGain (parameters.outputGainDb));
        mix.setTarget (parameters.mix);
        active.setTarget (parameters.bypass ? 0.0f : 1.0f);
    }

    void MasterProcessor::process (AudioBufferView block) noexcept
    {
        if (! prepared || block.isEmpty())
            return;

        ScopedDenormalGuard denormalGuard;

        const int total = block.getNumSamples();

        for (int start = 0; start < total; start += currentSpec.maxBlockSize)
            processChunk (block.getSubBlock (start, std::min (currentSpec.maxBlockSize, total - start)));
    }

    void MasterProcessor::addVinyl (AudioBufferView block) noexcept
    {
        if (vinyl.isIdle())
            return;

        const int numSamples = block.getNumSamples();
        const int numChannels = block.getNumChannels();

        for (int i = 0; i < numSamples; ++i)
        {
            vinyl.advance();

            for (int ch = 0; ch < numChannels; ++ch)
                block.getChannel (ch)[i] += vinyl.next (ch);
        }
    }

    void MasterProcessor::processChunk (AudioBufferView block) noexcept
    {
        const int numSamples = block.getNumSamples();
        const int numChannels = block.getNumChannels();

        // Fully bypassed: buffer stays exactly as received, engine is idle.
        if (parameters.bypass && ! active.isSmoothing())
        {
            mix.skip (numSamples);
            outputGain.skip (numSamples);
            engineNeedsReset = true;
            return;
        }

        if (engineNeedsReset)
        {
            engine.reset();
            engineNeedsReset = false;
        }

        const bool trivialMix = ! mix.isSmoothing() && ! outputGain.isSmoothing() && ! active.isSmoothing()
                                 && mix.getCurrent() == 1.0f && active.getCurrent() == 1.0f;

        if (trivialMix)
        {
            engine.process (block);

            const float gain = outputGain.getCurrent();

            if (gain != 1.0f)
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    float* data = block.getChannel (ch);

                    for (int i = 0; i < numSamples; ++i)
                        data[i] *= gain;
                }

            addVinyl (block);
            return;
        }

        const int stride = currentSpec.maxBlockSize;

        for (int ch = 0; ch < numChannels; ++ch)
            std::memcpy (dryStorage.data() + (std::size_t) (ch * stride), block.getChannel (ch),
                         sizeof (float) * (std::size_t) numSamples);

        engine.process (block);

        for (int i = 0; i < numSamples; ++i)
        {
            const float m = mix.getNext();
            const float g = outputGain.getNext();
            const float a = active.getNext();
            vinyl.advance();

            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float dry = dryStorage[(std::size_t) (ch * stride + i)];
                float* out = block.getChannel (ch);
                const float processed = g * (dry + m * (out[i] - dry));
                out[i] = dry + a * (processed - dry) + a * vinyl.next (ch); // noise last: untouched by modules, mix and gain
            }
        }
    }
}
