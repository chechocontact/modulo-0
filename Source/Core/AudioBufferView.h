#pragma once

#include <array>
#include <cassert>
#include <cstddef>

namespace vm
{
    // Vintage Maker processes mono and stereo only (spec §4).
    inline constexpr int maxChannels = 2;

    // Audio format the processors are prepared for. Passed to every prepare ().
    struct ProcessSpec
    {
        double sampleRate = 44100.0;
        int maxBlockSize = 512;
        int numChannels = 2;
    };

    // Non-owning view over planar float audio. Cheap to copy; never allocates.
    // Holds its own copy of the channel pointers so sub-blocks can be offset.
    class AudioBufferView
    {
    public:
        AudioBufferView() = default;

        AudioBufferView (float* const* channelData, int numChannelsIn, int numSamplesIn) noexcept
            : numChannels (numChannelsIn < maxChannels ? numChannelsIn : maxChannels),
              numSamples (numSamplesIn)
        {
            assert (numChannelsIn >= 0 && numSamplesIn >= 0);

            for (int ch = 0; ch < numChannels; ++ch)
                channels[(std::size_t) ch] = channelData[ch];
        }

        float* getChannel (int channel) const noexcept
        {
            assert (channel >= 0 && channel < numChannels);
            return channels[(std::size_t) channel];
        }

        int getNumChannels() const noexcept { return numChannels; }
        int getNumSamples() const noexcept { return numSamples; }
        bool isEmpty() const noexcept { return numChannels == 0 || numSamples == 0; }

        // A view of [startSample, startSample + length) of this block.
        AudioBufferView getSubBlock (int startSample, int length) const noexcept
        {
            assert (startSample >= 0 && length >= 0 && startSample + length <= numSamples);

            AudioBufferView sub;
            sub.numChannels = numChannels;
            sub.numSamples = length;

            for (int ch = 0; ch < numChannels; ++ch)
                sub.channels[(std::size_t) ch] = channels[(std::size_t) ch] + startSample;

            return sub;
        }

    private:
        std::array<float*, maxChannels> channels {};
        int numChannels = 0;
        int numSamples = 0;
    };
}
