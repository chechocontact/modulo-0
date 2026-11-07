#include "VinylNoise.h"
#include "ParameterIds.h"

#include <algorithm>
#include <cmath>

namespace vm
{
    namespace
    {
        constexpr double twoPi = 6.283185307179586476925286766559;

        constexpr float hissLevelDb = -40.0f;       // at Amount = 1
        constexpr float densityPerSecond = 7.0f;    // crackle ticks per second at Amount = 1
        constexpr float peakDb = -22.0f;
        constexpr float minTickSeconds = 0.00005f;
        constexpr float maxTickSeconds = 0.00015f;
    }

    void VinylNoise::prepare (const ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        amount.prepare (sampleRate, param::smoothingSeconds);
        highpassCoef = 1.0f - std::exp ((float) (-twoPi * 300.0 / sampleRate));
        reset();
    }

    void VinylNoise::reset() noexcept
    {
        amount.setCurrentAndTarget (amount.getTarget());
        amountNow = amount.getCurrent();

        for (int ch = 0; ch < maxChannels; ++ch)
        {
            auto& c = channels[(std::size_t) ch];
            c.hissNoise.setSeed (0x1000u + (std::uint32_t) ch * 7919u);
            c.crackleNoise.setSeed (0x2000u + (std::uint32_t) ch * 104729u);
            c.highpassState = 0.0f;
            c.burstRemaining = 0;
            c.burstAmplitude = 0.0f;
        }
    }

    void VinylNoise::setAmount (float newAmount) noexcept
    {
        amount.setTarget (clampValue (newAmount, 0.0f, 1.0f));
    }

    void VinylNoise::advance() noexcept
    {
        amountNow = amount.getNext();
    }

    float VinylNoise::next (int channel) noexcept
    {
        if (amountNow <= 0.0f)
            return 0.0f;

        auto& c = channels[(std::size_t) channel];

        // Hiss: pink noise, high-passed at 300 Hz
        const float pink = c.hissNoise.pink();
        c.highpassState += highpassCoef * (pink - c.highpassState);
        float out = (pink - c.highpassState) * amountNow * decibelsToGain (hissLevelDb);

        // Crackle: short decaying noise bursts at random times
        if (c.burstRemaining <= 0)
        {
            const float probability = densityPerSecond * amountNow / (float) sampleRate;

            if (c.crackleNoise.uniform01() < probability)
            {
                const float u = c.crackleNoise.uniform01();
                const float seconds = minTickSeconds + (maxTickSeconds - minTickSeconds) * c.crackleNoise.uniform01();
                const float level = std::min (1.5f, -std::log (std::max (1.0f - u * 0.999f, 1.0e-6f)) * 0.5f);

                c.burstLength = std::max (1, (int) (seconds * (float) sampleRate));
                c.burstRemaining = c.burstLength;
                c.burstAmplitude = level * amountNow * decibelsToGain (peakDb);
            }
        }

        if (c.burstRemaining > 0)
        {
            const float envelope = (float) c.burstRemaining / (float) c.burstLength;
            out += c.burstAmplitude * envelope * c.crackleNoise.white();
            --c.burstRemaining;
        }

        return out;
    }
}
