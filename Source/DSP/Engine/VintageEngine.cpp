#include "VintageEngine.h"

#include <algorithm>
#include <cmath>

namespace vm
{
    namespace
    {
        float clamp01 (float v) noexcept { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

        float clampTo (float v, float low, float high) noexcept { return v < low ? low : (v > high ? high : v); }

        // Per Character: speed multipliers of the three drifts, their weights and the jitter.
        struct CharacterShape
        {
            float speed[3];
            float weight[3];
            float jitter;
        };

        CharacterShape shapeFor (EngineCharacter c) noexcept
        {
            switch (c)
            {
                case EngineCharacter::slow:    return { { 0.4f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, 0.0f };
                case EngineCharacter::organic: return { { 1.0f, 2.3f, 5.1f }, { 0.6f, 0.3f, 0.1f }, 0.3f };
                case EngineCharacter::random:  return { { 3.0f, 5.3f, 0.0f }, { 0.7f, 0.3f, 0.0f }, 0.7f };
            }
            return { { 1.0f, 2.3f, 5.1f }, { 0.6f, 0.3f, 0.1f }, 0.3f };
        }
    }

    void VintageEngine::prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        reset();
    }

    void VintageEngine::reset() noexcept
    {
        for (std::size_t d = 0; d < sources.size(); ++d)
            for (std::size_t c = 0; c < sources[d].size(); ++c)
                sources[d][c].setSeed (0xA5F10000u + (std::uint32_t) (d * 31 + c * 7 + 1));

        smoothed.fill (0.0f);
        offsets = {};
        justReset = true;
        configureSources();
    }

    void VintageEngine::configureSources() noexcept
    {
        const float rateHz = engine::rateMinHz * std::pow (engine::rateMaxHz / engine::rateMinHz, parameters.rate);
        const auto shape = shapeFor (parameters.character);

        for (std::size_t d = 0; d < sources.size(); ++d)
            for (std::size_t c = 0; c < (std::size_t) numComponents; ++c)
                sources[d][c].setSpeed ((double) (rateHz * shape.speed[c]), sampleRate, shape.jitter);

        componentWeight = { shape.weight[0], shape.weight[1], shape.weight[2] };
    }

    void VintageEngine::setParameters (const VintageEngineParameters& p) noexcept
    {
        // Non-finite values keep the previous value.
        const auto clean = [] (float value, float previous)
        {
            return std::isfinite (value) ? clamp01 (value) : previous;
        };

        VintageEngineParameters q = parameters;
        q.enabled = p.enabled;
        q.flux = clean (p.flux, q.flux);
        q.rate = clean (p.rate, q.rate);
        q.depth = clean (p.depth, q.depth);
        q.character = (int) p.character >= 0 && (int) p.character <= 2 ? p.character : q.character;

        if (! (q == parameters))
        {
            parameters = q;
            configureSources();
        }
    }

    void VintageEngine::advance(int numSamples) noexcept
    {
        if (numSamples <= 0)
            return;

        const float amount = parameters.enabled ? parameters.flux * parameters.depth : 0.0f;
        const float k = 1.0f - (float) std::exp (-(double) numSamples / (engine::smoothingSeconds * sampleRate));

        constexpr float ranges[engine::numDestinations] = {
            engine::wowDepthRange, engine::wowRateRange, engine::flutterDepthRange, engine::flutterRateRange,
            engine::vinylAmountRange, engine::driveRange, engine::grainRange, engine::tremoloRateRange, engine::widthRange };

        for (std::size_t d = 0; d < sources.size(); ++d)
        {
            // The drifts always run (so the pattern does not depend on when Flux is raised).
            float m = 0.0f;
            for (std::size_t c = 0; c < (std::size_t) numComponents; ++c)
            {
                const float v = sources[d][c].advance (numSamples);
                m += componentWeight[c] * v;
            }

            const float target = amount > 0.0f ? clampTo (m, -1.0f, 1.0f) * amount * ranges[d] : 0.0f;

            float& s = smoothed[d];
            if (justReset)
                s = target; // the very first block starts on the drift, no ramp from zero
            else
                s += k * (target - s);
            if (amount <= 0.0f && std::fabs (s) < 1.0e-6f)
                s = 0.0f; // fully off: exactly transparent
        }

        justReset = false;

        offsets.wowDepth = smoothed[0];
        offsets.wowRate = smoothed[1];
        offsets.flutterDepth = smoothed[2];
        offsets.flutterRate = smoothed[3];
        offsets.vinylAmount = smoothed[4];
        offsets.drive = smoothed[5];
        offsets.grain = smoothed[6];
        offsets.tremoloRate = smoothed[7];
        offsets.width = smoothed[8];
    }

    void VintageEngine::apply (TextureParameters& p) const noexcept
    {
        p.wowDepth = clamp01 (p.wowDepth + offsets.wowDepth);
        p.wowRate = clampTo (p.wowRate + offsets.wowRate, texture::wowRateMinHz, texture::wowRateMaxHz);
        p.flutterDepth = clamp01 (p.flutterDepth + offsets.flutterDepth);
        p.flutterRate = clampTo (p.flutterRate + offsets.flutterRate, texture::flutterRateMinHz, texture::flutterRateMaxHz);
    }

    void VintageEngine::apply (ColorParameters& p) const noexcept
    {
        p.drive = clamp01 (p.drive + offsets.drive);
    }

    void VintageEngine::apply (MovementParameters& p) const noexcept
    {
        p.grain = clamp01 (p.grain + offsets.grain);
        p.rateHz = clampTo (p.rateHz + offsets.tremoloRate, movement::rateMinHz, movement::rateMaxHz);
    }

    void VintageEngine::apply (SpaceParameters& p) const noexcept
    {
        p.width = clamp01 (p.width + offsets.width);
    }

    float VintageEngine::applyToVinylAmount (float amount) const noexcept
    {
        return clamp01 (amount + offsets.vinylAmount);
    }
}
