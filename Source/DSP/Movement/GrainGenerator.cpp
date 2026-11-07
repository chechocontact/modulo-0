#include "GrainGenerator.h"

#include <algorithm>
#include <cmath>

namespace vm
{
    void GrainGenerator::prepare (double newSampleRate, int newBufferSize, int newChannels) noexcept
    {
        sampleRate = newSampleRate;
        bufferSize = newBufferSize;
        channels = newChannels;
        setStyle (style);
        reset();
    }

    void GrainGenerator::reset() noexcept
    {
        random.setSeed (0x1234ABCDu);
        driftPhase = 0.0;
        samplesToNext = 0.0;
    }

    void GrainGenerator::setStyle (MovementStyle newStyle) noexcept
    {
        style = newStyle;
        config = movement::styleConfig (newStyle);

        // N overlapping grains of mean-square 0.375 (Hann) -> power N * 0.375.
        const float overlap = config.density * config.sizeSeconds;
        normalisation = 1.0f / std::sqrt (std::max (0.5f, overlap * 0.375f));
    }

    void GrainGenerator::scheduleNext() noexcept
    {
        const double interval = sampleRate / (double) config.density;
        const double jitter = 1.0 + (double) config.intervalJitter * (2.0 * random.uniform01() - 1.0);
        samplesToNext += std::max (1.0, interval * jitter);
    }

    bool GrainGenerator::shouldSpawn() noexcept
    {
        if (config.driftHz > 0.0f)
        {
            driftPhase += (double) config.driftHz / sampleRate;
            if (driftPhase >= 1.0)
                driftPhase -= 1.0;
        }

        samplesToNext -= 1.0;
        if (samplesToNext > 0.0)
            return false;

        scheduleNext();
        return true;
    }

    GrainSpec GrainGenerator::nextGrain() noexcept
    {
        GrainSpec g;
        g.length = std::max (8, (int) std::lround ((double) config.sizeSeconds * sampleRate));

        // Pitch: small random detune, plus an occasional octave in Cloud.
        const double cents = (double) config.detuneCents * (2.0 * random.uniform01() - 1.0);
        double speed = std::pow (2.0, cents / 1200.0);
        const float octave = random.uniform01();
        if (octave < config.octaveUpChance)
            speed *= 2.0;
        else if (octave < config.octaveUpChance + config.octaveDownChance)
            speed *= 0.5;
        g.speed = speed;

        // Where in the past it starts (seconds behind the write head).
        double offsetSeconds;
        if (config.driftHz > 0.0f)
        {
            const double centre = (double) config.delayMinSeconds + 0.5 * (double) config.delayRangeSeconds;
            offsetSeconds = centre + 0.4 * (double) config.delayRangeSeconds * std::sin (6.283185307179586 * driftPhase)
                            + 0.03 * (2.0 * random.uniform01() - 1.0);
        }
        else
        {
            offsetSeconds = (double) config.delayMinSeconds + (double) config.delayRangeSeconds * random.uniform01();
        }

        // Keep the read point behind the write head for the whole grain:
        //  - moving head: a faster grain catches up by length * (speed - 1)
        //  - frozen head: the grain moves forward by length * speed
        const double len = (double) g.length;
        const double safe = (config.freezeCapture ? len * speed : std::max(0.0, len * (speed - 1.0))) + 2.0;
        double delay = safe + std::max (0.0, offsetSeconds) * sampleRate;

        // ...and inside the buffer (a slower grain falls back by length * (1 - speed)).
        const double maxDelay = (double) bufferSize - 4.0 - std::max (0.0, len * (1.0 - speed));
        g.delay = std::min (delay, maxDelay);

        // Equal-power pan; centre = unity gain.
        if (channels > 1)
        {
            const double angle = 0.78539816339744831 * (1.0 + (double) config.spread * (2.0 * random.uniform01() - 1.0));
            g.gainL = (float) (std::cos (angle) * 1.4142135623730951);
            g.gainR = (float) (std::sin (angle) * 1.4142135623730951);
        }
        else
        {
            g.gainL = g.gainR = 1.0f;
        }

        return g;
    }
}
