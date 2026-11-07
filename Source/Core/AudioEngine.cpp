#include "AudioEngine.h"
#include "ParameterIds.h"

namespace vm
{
    AudioEngine::AudioEngine()
    {
        inputGain.setCurrentAndTarget (1.0f);
    }

    void AudioEngine::setModule (Slot slot, std::unique_ptr<ModuleProcessor> module)
    {
        const auto index = (std::size_t) slot;
        modules[index] = std::move (module);
        slots[index].setProcessor (modules[index].get());
    }

    ModuleProcessor* AudioEngine::getModule (Slot slot) const noexcept
    {
        return modules[(std::size_t) slot].get();
    }

    void AudioEngine::prepare (const ProcessSpec& spec)
    {
        inputGain.prepare (spec.sampleRate, param::smoothingSeconds);

        for (auto& slot : slots)
            slot.prepare (spec);
    }

    void AudioEngine::reset() noexcept
    {
        inputGain.setCurrentAndTarget (inputGain.getTarget());

        for (auto& slot : slots)
            slot.reset();
    }

    void AudioEngine::setInputGainDb (float decibels) noexcept
    {
        inputGain.setTarget (decibelsToGain (clampValue (decibels, param::gainMinDb, param::gainMaxDb)));
    }

    int AudioEngine::getLatencySamples() const noexcept
    {
        int total = 0;

        for (const auto& slot : slots)
            total += slot.getLatencySamples();

        return total;
    }

    void AudioEngine::applyInputGain (AudioBufferView block) noexcept
    {
        const int numSamples = block.getNumSamples();
        const int numChannels = block.getNumChannels();

        if (! inputGain.isSmoothing())
        {
            const float gain = inputGain.getCurrent();

            if (gain == 1.0f)
                return;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                float* data = block.getChannel (ch);

                for (int i = 0; i < numSamples; ++i)
                    data[i] *= gain;
            }

            return;
        }

        for (int i = 0; i < numSamples; ++i)
        {
            const float gain = inputGain.getNext();

            for (int ch = 0; ch < numChannels; ++ch)
                block.getChannel (ch)[i] *= gain;
        }
    }

    void AudioEngine::process (AudioBufferView block) noexcept
    {
        if (block.isEmpty())
            return;

        applyInputGain (block);

        for (auto& slot : slots)
            slot.process (block);
    }
}
