#include "WowFlutter.h"
#include "TextureParameters.h"

#include <algorithm>
#include <cmath>

namespace vm
{
    namespace
    {
        constexpr double twoPi = 6.283185307179586476925286766559;
        constexpr double wowDepthRampSeconds = 0.4;
        constexpr double flutterDepthRampSeconds = 0.1;
        constexpr double rateRampSeconds = 0.1;
        constexpr double shapeRampSeconds = 0.05;
        constexpr double flutterRandomRateHz = 9.0;

        inline double wrap01 (double p) noexcept { return p - std::floor (p); }

    }

    void WowFlutter::prepare (const ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        numChannels = std::clamp (spec.numChannels, 1, maxChannels);

        wowExcursion = (float) (texture::wowMaxExcursionSeconds * sampleRate);
        flutterExcursion = (float) (texture::flutterMaxExcursionSeconds * sampleRate);

        // Largest possible delay: 2*wow + 2*flutter + drop, with margin.
        const int maxDelay = (int) std::ceil (sampleRate * 0.020);

        for (auto& line : lines)
            line.prepare (maxDelay);

        wowDepth.prepare (sampleRate, wowDepthRampSeconds);
        wowRate.prepare (sampleRate, rateRampSeconds);
        wowShape.prepare (sampleRate, shapeRampSeconds);
        flutterDepth.prepare (sampleRate, flutterDepthRampSeconds);
        flutterRate.prepare (sampleRate, rateRampSeconds);
        flutterVariation.prepare (sampleRate, shapeRampSeconds);

        reset();
    }

    void WowFlutter::reset() noexcept
    {
        for (auto& line : lines)
            line.reset();

        for (auto* s : { &wowDepth, &wowRate, &wowShape, &flutterDepth, &flutterRate, &flutterVariation })
            s->setCurrentAndTarget (s->getTarget());

        delay.fill (0.0f);
        wowPhase = 0.0;
        flutterPhase = 0.0;
        drift.reset();
        flutterRateRandom.reset();
        flutterAmpRandom.reset();
        noise.setSeed (0xC0FFEE01u);
    }

    void WowFlutter::setParams (const Params& p) noexcept
    {
        wowDepth.setTarget (clampValue (p.wowDepth, 0.0f, 1.0f));
        wowRate.setTarget (clampValue (p.wowRate, texture::wowRateMinHz, texture::wowRateMaxHz));
        wowShape.setTarget (clampValue (p.wowShape, 0.0f, 1.0f));
        flutterDepth.setTarget (clampValue (p.flutterDepth, 0.0f, 1.0f));
        flutterRate.setTarget (clampValue (p.flutterRate, texture::flutterRateMinHz, texture::flutterRateMaxHz));
        flutterVariation.setTarget (clampValue (p.flutterVariation, 0.0f, 1.0f));
    }

    void WowFlutter::advance (float extraDelaySamples) noexcept
    {
        const float wd = wowDepth.getNext();
        const float wr = wowRate.getNext();
        const float ws = wowShape.getNext();
        const float fd = flutterDepth.getNext();
        const float fr = flutterRate.getNext();
        const float fv = flutterVariation.getNext();

        wowPhase = wrap01 (wowPhase + (double) wr / sampleRate);

        // Random drift: smooth random walk, one new target per wow cycle.
        const float drifting = drift.next ((double) wr / sampleRate, noise);

        // Flutter randomness: two independent smooth random signals.
        const float nA = flutterRateRandom.next (flutterRandomRateHz / sampleRate, noise);
        const float nB = flutterAmpRandom.next (flutterRandomRateHz / sampleRate, noise);

        flutterPhase = wrap01 (flutterPhase + (double) fr * (1.0 + 0.5 * (double) fv * (double) nA) / sampleRate);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const double pw = wrap01 (wowPhase + 0.03 * ch);
            const float sine = (float) std::sin (twoPi * pw);
            const float tri = (float) (4.0 * std::abs (pw - 0.5) - 1.0);

            const float mw = ws < 0.5f ? sine + (tri - sine) * (ws * 2.0f)
                                       : tri + (drifting - tri) * ((ws - 0.5f) * 2.0f);

            const double pf = wrap01 (flutterPhase + 0.2 * ch);
            const float noiseTerm = ch == 0 ? nB : -nB;
            const float mf = (1.0f - 0.4f * fv) * (float) std::sin (twoPi * pf) + 0.4f * fv * noiseTerm;

            delay[(std::size_t) ch] = wd * wowExcursion * (1.0f + mw)
                                       + fd * flutterExcursion * (1.0f + mf)
                                       + extraDelaySamples;
        }
    }

    float WowFlutter::process (int channel, float input) noexcept
    {
        auto& line = lines[(std::size_t) channel];
        line.push (input);
        return line.read (delay[(std::size_t) channel]);
    }
}
