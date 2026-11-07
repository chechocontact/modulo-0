#include "MovementProcessor.h"

#include <cmath>

namespace vm
{
    void MovementProcessor::prepare (const ProcessSpec& spec)
    {
        ProcessSpec s = spec;
        s.numChannels = spec.numChannels < 1 ? 1 : (spec.numChannels > maxChannels ? maxChannels : spec.numChannels);

        engine.prepare (s);
        tremolo.prepare (s);

        applyParameters();
        reset();
    }

    void MovementProcessor::reset() noexcept
    {
        engine.reset();
        tremolo.reset();
    }

    void MovementProcessor::setParameters (const MovementParameters& p) noexcept
    {
        // Non-finite values keep the previous value.
        const auto clean = [] (float value, float previous, float low, float high)
        {
            return std::isfinite (value) ? clampValue (value, low, high) : previous;
        };

        MovementParameters q = parameters;
        q.grain = clean (p.grain, q.grain, 0.0f, 1.0f);
        q.feedback = clean (p.feedback, q.feedback, 0.0f, 1.0f);
        q.tremolo = clean (p.tremolo, q.tremolo, 0.0f, 1.0f);
        q.rateHz = clean (p.rateHz, q.rateHz, movement::rateMinHz, movement::rateMaxHz);
        q.style = (int) p.style >= 0 && (int) p.style <= 2 ? p.style : q.style;

        parameters = q;
        applyParameters();
    }

    void MovementProcessor::applyParameters() noexcept
    {
        engine.setParams (parameters.grain, parameters.feedback, parameters.style);
        tremolo.setParams (parameters.tremolo, parameters.rateHz);
    }

    void MovementProcessor::process (AudioBufferView block) noexcept
    {
        engine.process (block);
        tremolo.process (block);
    }
}
