#include "VinylProcessor.h"

#include <algorithm>
#include <cmath>

namespace vm
{
    namespace
    {
        constexpr double twoPi = 6.283185307179586476925286766559;

        // Full-scale (Amount = 1) hiss level and per-type crackle behaviour.
        constexpr float hissLevelDb = -40.0f;

        struct CrackleSpec
        {
            float densityPerSecond;
            float peakDb;
            float minSeconds, maxSeconds;
        };

        constexpr CrackleSpec crackleSpec (CrackleType t) noexcept
        {
            switch (t)
            {
                case CrackleType::dusty:  return { 40.0f, -30.0f, 0.00003f, 0.0001f };
                case CrackleType::heavy:  return { 14.0f, -16.0f, 0.0003f,  0.0020f };
                case CrackleType::normal: break;
            }
            return { 7.0f, -22.0f, 0.00005f, 0.00015f };
        }
    }

    void VinylProcessor::prepare (const ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        numChannels = std::clamp (spec.numChannels, 1, maxChannels);

        amount.prepare (sampleRate, param::smoothingSeconds);
        dropAmount.prepare (sampleRate, 0.1);
        highpassCoef = 1.0f - std::exp ((float) (-twoPi * 300.0 / sampleRate));

        reset();
    }

    void VinylProcessor::reset() noexcept
    {
        amount.setCurrentAndTarget (amount.getTarget());
        dropAmount.setCurrentAndTarget (dropAmount.getTarget());
        amountNow = amount.getCurrent();
        dropAmountNow = dropAmount.getCurrent();

        for (int ch = 0; ch < maxChannels; ++ch)
        {
            auto& c = channels[(std::size_t) ch];
            c.hissNoise.setSeed (0x1000u + (std::uint32_t) ch * 7919u);
            c.crackleNoise.setSeed (0x2000u + (std::uint32_t) ch * 104729u);
            c.highpassState = 0.0f;
            c.burstRemaining = 0;
            c.burstAmplitude = 0.0f;
        }

        dropNoise.setSeed (0x3001u);
        dropActive = false;
        dropEnvelope = 0.0f;
        scheduleNextDrop();
    }

    void VinylProcessor::setParams (float newAmount, CrackleType newType, float newDropAmount) noexcept
    {
        amount.setTarget (clampValue (newAmount, 0.0f, 1.0f));
        dropAmount.setTarget (clampValue (newDropAmount, 0.0f, 1.0f));
        type = newType;
    }

    void VinylProcessor::scheduleNextDrop() noexcept
    {
        const float depth = std::max (dropAmount.getTarget(), 0.1f);
        const float meanSeconds = 30.0f + (5.0f - 30.0f) * depth;
        const float u = std::max (dropNoise.uniform01(), 1.0e-6f);
        const float seconds = std::max (2.0f, -std::log (u) * meanSeconds);
        dropCountdown = (long long) ((double) seconds * sampleRate);
    }

    void VinylProcessor::forceDrop (double seconds) noexcept
    {
        dropActive = true;
        dropPosition = 0;
        dropLength = std::max (1LL, (long long) (seconds * sampleRate));
    }

    void VinylProcessor::advance() noexcept
    {
        amountNow = amount.getNext();
        dropAmountNow = dropAmount.getNext();

        if (dropActive)
        {
            ++dropPosition;

            if (dropPosition >= dropLength)
            {
                dropActive = false;
                dropEnvelope = 0.0f;
                scheduleNextDrop();
            }
            else
            {
                dropEnvelope = 0.5f * (1.0f - (float) std::cos (twoPi * (double) dropPosition / (double) dropLength));
            }
        }
        else if (dropAmountNow > 0.0f && --dropCountdown <= 0)
        {
            const double seconds = 0.15 + 0.30 * (double) dropNoise.uniform01();
            forceDrop (seconds);
        }
    }

    float VinylProcessor::next (int channel) noexcept
    {
        if (amountNow <= 0.0f)
            return 0.0f;

        auto& c = channels[(std::size_t) channel];

        // Hiss
        const float pink = c.hissNoise.pink();
        c.highpassState += highpassCoef * (pink - c.highpassState);
        float out = (pink - c.highpassState) * amountNow * decibelsToGain (hissLevelDb);

        // Crackle
        const CrackleSpec spec = crackleSpec (type);

        if (c.burstRemaining <= 0)
        {
            const float probability = spec.densityPerSecond * amountNow / (float) sampleRate;

            if (c.crackleNoise.uniform01() < probability)
            {
                const float u = c.crackleNoise.uniform01();
                const float seconds = spec.minSeconds + (spec.maxSeconds - spec.minSeconds) * c.crackleNoise.uniform01();
                const float level = std::min (1.5f, -std::log (std::max (1.0f - u * 0.999f, 1.0e-6f)) * 0.5f);

                c.burstLength = std::max (1, (int) (seconds * (float) sampleRate));
                c.burstRemaining = c.burstLength;
                c.burstAmplitude = level * amountNow * decibelsToGain (spec.peakDb);
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
