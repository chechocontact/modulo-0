#pragma once

#include "Core/ModuleProcessor.h"
#include "Core/SmoothedValue.h"

namespace vm
{
    // Placeholder module used in Module 0 to verify the chain before the real
    // effects exist. It applies a smoothed gain (and optional polarity flip), so
    // tests can check order, level and smoothing through every slot.
    //
    // In the plugin it sits in all four slots at unity gain, which makes the
    // plugin bit-transparent at default settings. Its parameters are NOT exposed
    // to the host: they are for tests only and will disappear when each slot
    // receives its real module.
    class TestProcessor final : public ModuleProcessor
    {
    public:
        struct Parameters
        {
            float gainDb = 0.0f;
            bool invertPolarity = false;

            bool operator== (const Parameters&) const = default;
        };

        void prepare (const ProcessSpec& spec) override;
        void process (AudioBufferView block) noexcept override;
        void reset() noexcept override;
        const char* getName() const noexcept override { return "Test"; }

        void setParameters (const Parameters& newParameters) noexcept;
        Parameters getParameters() const noexcept { return parameters; }

    private:
        float targetGain() const noexcept;

        Parameters parameters;
        LinearSmoothedValue gain;
    };
}
