#include "SaturationProcessor.h"
#include "Core/ParameterIds.h"

#include <algorithm>
#include <cmath>

namespace vm
{
    namespace
    {
        constexpr double twoPi = 6.283185307179586476925286766559;
        constexpr double tubeBias = 0.35;
        constexpr double referenceLevel = 0.3; // level used for loudness compensation

        // ln(cosh(x)), stable for large |x|.
        inline double logCosh (double x) noexcept
        {
            const double a = std::abs (x);
            return a + std::log1p (std::exp (-2.0 * a)) - 0.69314718055994531;
        }
    }

    void SaturationProcessor::prepare (const ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        drive.prepare (sampleRate, param::smoothingSeconds);
        typeFade.prepare (sampleRate, 0.01);
        dcCoef = 1.0f - (float) (twoPi * 8.0 / sampleRate);
        reset();
    }

    void SaturationProcessor::reset() noexcept
    {
        drive.setCurrentAndTarget (drive.getTarget());
        activeType = type;
        typeFade.setCurrentAndTarget (1.0f);
        driveNow = drive.getCurrent();
        active = driveNow > 0.0f;
        for (auto& s : states)
            s = ChannelState {};
        updateMakeup();
        blendNow = blend;
    }

    void SaturationProcessor::setParams (float newDrive, ColorType newType) noexcept
    {
        drive.setTarget (clampValue (newDrive, 0.0f, 1.0f));
        type = newType;
    }

    double SaturationProcessor::shape (double u) const noexcept
    {
        switch (activeType)
        {
            case ColorType::tube:    return std::tanh (u + tubeBias) - std::tanh (tubeBias);
            case ColorType::tape:    return u / std::sqrt (1.0 + u * u);
            case ColorType::console: break;
        }

        const double a = std::min (std::abs (u), 1.0);
        const double y = a - a * a * a / 3.0;
        return u < 0.0 ? -y : y;
    }

    double SaturationProcessor::antiderivative (double u) const noexcept
    {
        switch (activeType)
        {
            case ColorType::tube:    return logCosh (u + tubeBias) - std::tanh (tubeBias) * u;
            case ColorType::tape:    return std::sqrt (1.0 + u * u);
            case ColorType::console: break;
        }

        const double a = std::abs (u);
        return a <= 1.0 ? a * a / 2.0 - a * a * a * a / 12.0 : (2.0 / 3.0) * a - 0.25;
    }

    void SaturationProcessor::updateMakeup() noexcept
    {
        preGain = std::pow (10.0, (double) driveNow * color::driveMaxDb / 20.0);
        // Average of both polarities: the tube curve is asymmetric.
        const double reference = 0.5 * (std::abs (shape (referenceLevel * preGain)) + std::abs (shape (-referenceLevel * preGain)));
        makeup = reference > 1.0e-6 ? std::min (1.6, referenceLevel / reference) : 1.0;
        blend = std::min (1.0f, driveNow * 10.0f);

        // Tape head loss: the low-pass closes from 16 kHz to 7 kHz with Drive.
        const double cutoff = std::min (0.45 * sampleRate, 16000.0 - 9000.0 * (double) driveNow);
        tapeCoef = 1.0f - (float) std::exp (-twoPi * cutoff / sampleRate);
    }

    void SaturationProcessor::advance() noexcept
    {
        const bool moving = drive.isSmoothing();
        driveNow = drive.getNext();

        if (moving)
            updateMakeup();

        const bool nowActive = driveNow > 0.0f;

        if (nowActive && ! active)
            for (auto& s : states)
                s = ChannelState {};

        active = nowActive;

        // Switching curves: fade the effect out, swap, fade back in (no click).
        if (activeType != type)
        {
            if (! active)
            {
                activeType = type;
                updateMakeup();
            }
            else
            {
                typeFade.setTarget (0.0f);

                if (! typeFade.isSmoothing() && typeFade.getCurrent() <= 0.0f)
                {
                    activeType = type;
                    for (auto& s : states)
                        s.previousU = 0.0;
                    updateMakeup();
                    typeFade.setTarget (1.0f);
                }
            }
        }
        else
        {
            typeFade.setTarget (1.0f);
        }

        blendNow = blend * typeFade.getNext();
    }

    float SaturationProcessor::process (int channel, float input) noexcept
    {
        if (! active)
            return input;

        auto& s = states[(std::size_t) channel];
        const double u = (double) input * preGain;

        double y;
        const double du = u - s.previousU;

        if (antiAliasing && std::abs (du) > 1.0e-5)
            y = (antiderivative (u) - antiderivative (s.previousU)) / du;
        else
            y = shape (antiAliasing ? 0.5 * (u + s.previousU) : u);

        s.previousU = u;
        float wet = (float) (y * makeup);

        if (activeType == ColorType::tube)
        {
            // DC blocker (the biased curve produces an offset)
            const float out = wet - s.dcIn + dcCoef * s.dcOut;
            s.dcIn = wet;
            s.dcOut = out;
            wet = out;
        }
        else if (activeType == ColorType::tape)
        {
            s.lowpass += tapeCoef * (wet - s.lowpass);
            wet = s.lowpass;
        }

        return input + blendNow * (wet - input);
    }
}
