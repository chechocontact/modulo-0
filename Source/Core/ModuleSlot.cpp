#include "ModuleSlot.h"
#include "ParameterIds.h"

#include <algorithm>
#include <cassert>
#include <cstring>

namespace vm
{
    void ModuleSlot::prepare (const ProcessSpec& spec)
    {
        maxBlockSize = std::max (1, spec.maxBlockSize);
        dryStorage.assign ((std::size_t) (maxChannels * maxBlockSize), 0.0f);

        enableGain.prepare (spec.sampleRate, param::smoothingSeconds);
        mix.prepare (spec.sampleRate, param::smoothingSeconds);
        enableGain.setCurrentAndTarget (enabledTarget ? 1.0f : 0.0f);
        mix.setCurrentAndTarget (mix.getTarget());

        if (processor != nullptr)
            processor->prepare (spec);

        needsReset = false;
    }

    void ModuleSlot::reset() noexcept
    {
        enableGain.setCurrentAndTarget (enabledTarget ? 1.0f : 0.0f);
        mix.setCurrentAndTarget (mix.getTarget());

        if (processor != nullptr)
            processor->reset();

        needsReset = false;
    }

    void ModuleSlot::setEnabled (bool shouldBeEnabled) noexcept
    {
        enabledTarget = shouldBeEnabled;
        enableGain.setTarget (shouldBeEnabled ? 1.0f : 0.0f);
    }

    void ModuleSlot::setMix (float mix01) noexcept
    {
        mix.setTarget (clampValue (mix01, 0.0f, 1.0f));
    }

    int ModuleSlot::getLatencySamples() const noexcept
    {
        return processor != nullptr ? processor->getLatencySamples() : 0;
    }

    void ModuleSlot::process (AudioBufferView block) noexcept
    {
        const int numSamples = block.getNumSamples();
        const int numChannels = block.getNumChannels();

        if (processor == nullptr || block.isEmpty())
            return;

        assert (numSamples <= maxBlockSize);

        // Fully bypassed: leave the buffer untouched and skip the module.
        if (! enabledTarget && ! enableGain.isSmoothing())
        {
            mix.skip (numSamples);
            needsReset = true;
            return;
        }

        if (needsReset)
        {
            processor->reset();
            needsReset = false;
        }

        // Fast path: fully on, 100 % wet. Output is exactly the module output.
        if (! enableGain.isSmoothing() && ! mix.isSmoothing()
             && enableGain.getCurrent() == 1.0f && mix.getCurrent() == 1.0f)
        {
            processor->process (block);
            return;
        }

        for (int ch = 0; ch < numChannels; ++ch)
            std::memcpy (dryStorage.data() + (std::size_t) (ch * maxBlockSize),
                         block.getChannel (ch), sizeof (float) * (std::size_t) numSamples);

        processor->process (block);

        for (int i = 0; i < numSamples; ++i)
        {
            const float wet = enableGain.getNext() * mix.getNext();

            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float dry = dryStorage[(std::size_t) (ch * maxBlockSize + i)];
                float* out = block.getChannel (ch);
                out[i] = dry + wet * (out[i] - dry);
            }
        }
    }
}
