#include "VinylNoise.h"
#include "ParameterIds.h"

#include <algorithm>
#include <cmath>

namespace vm
{
    namespace
    {
        constexpr double twoPi = 6.283185307179586476925286766559;

        // All levels are at Amount = 1.
        struct CrackleConfig
        {
            float hissDb;            // level of the hiss
            float ticksPerSecond;    // crackle density
            float peakDb;            // size of a typical tick (the biggest are up to 1.5x)
            float minTickSeconds;    // tick length range
            float maxTickSeconds;
        };

        constexpr CrackleConfig configFor (CrackleType type) noexcept
        {
            switch (type)
            {
                case CrackleType::normal: return { -40.0f,  7.0f, -22.0f, 0.00005f, 0.00015f };
                case CrackleType::dusty:  return { -38.0f, 24.0f, -30.0f, 0.00003f, 0.00010f };
                case CrackleType::heavy:  return { -34.0f, 12.0f, -15.0f, 0.00008f, 0.00050f };
            }
            return { -40.0f, 7.0f, -22.0f, 0.00005f, 0.00015f };
        }

        constexpr double typeGlideSeconds = 0.05;
    }

    void VinylNoise::prepare (const ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        amount.prepare (sampleRate, param::smoothingSeconds);
        hissGain.prepare (sampleRate, typeGlideSeconds);
        peakGain.prepare (sampleRate, typeGlideSeconds);
        density.prepare (sampleRate, typeGlideSeconds);
        setCrackleType (crackleType);
        highpassCoef = 1.0f - std::exp ((float) (-twoPi * 300.0 / sampleRate));
        reset();
    }

    void VinylNoise::reset() noexcept
    {
        amount.setCurrentAndTarget (amount.getTarget());
        amountNow = amount.getCurrent();

        hissGain.setCurrentAndTarget (hissGain.getTarget());
        peakGain.setCurrentAndTarget (peakGain.getTarget());
        density.setCurrentAndTarget (density.getTarget());
        hissNow = hissGain.getTarget();
        peakNow = peakGain.getTarget();
        densityNow = density.getTarget();
        ticksStarted = 0;

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

    void VinylNoise::setCrackleType (CrackleType type) noexcept
    {
        if ((int) type < 0 || (int) type > 2)
            return;

        crackleType = type;
        const auto config = configFor (type);
        hissGain.setTarget (decibelsToGain (config.hissDb));
        peakGain.setTarget (decibelsToGain (config.peakDb));
        density.setTarget (config.ticksPerSecond);
        minTickSeconds = config.minTickSeconds;
        maxTickSeconds = config.maxTickSeconds;
    }

    void VinylNoise::advance() noexcept
    {
        amountNow = amount.getNext();
        hissNow = hissGain.getNext();
        peakNow = peakGain.getNext();
        densityNow = density.getNext();
    }

    float VinylNoise::next (int channel) noexcept
    {
        if (amountNow <= 0.0f)
            return 0.0f;

        auto& c = channels[(std::size_t) channel];

        // Hiss: pink noise, high-passed at 300 Hz
        const float pink = c.hissNoise.pink();
        c.highpassState += highpassCoef * (pink - c.highpassState);
        float out = (pink - c.highpassState) * amountNow * hissNow;

        // Crackle: short decaying noise bursts at random times
        if (c.burstRemaining <= 0)
        {
            const float probability = densityNow * amountNow / (float) sampleRate;

            if (c.crackleNoise.uniform01() < probability)
            {
                const float u = c.crackleNoise.uniform01();
                const float seconds = minTickSeconds + (maxTickSeconds - minTickSeconds) * c.crackleNoise.uniform01();
                const float level = std::min (1.5f, -std::log (std::max (1.0f - u * 0.999f, 1.0e-6f)) * 0.5f);

                c.burstLength = std::max (1, (int) (seconds * (float) sampleRate));
                c.burstRemaining = c.burstLength;
                c.burstAmplitude = level * amountNow * peakNow;
                ++ticksStarted;
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
