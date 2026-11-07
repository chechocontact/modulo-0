#pragma once

#include "Core/ModuleProcessor.h"
#include "GranularEngine.h"
#include "MovementParameters.h"
#include "TremoloProcessor.h"

namespace vm
{
    // Module 3 - MOVEMENT.
    //
    //   input -> GranularEngine (Grain, Feedback, Style) -> Tremolo (Depth, Rate)
    //
    // Always 100 % wet (bypass and dry/wet belong to ModuleSlot). No latency.
    // With Grain and Tremolo at zero it is bit-transparent.
    class MovementProcessor final : public ModuleProcessor
    {
    public:
        void prepare (const ProcessSpec& spec) override;
        void process (AudioBufferView block) noexcept override;
        void reset() noexcept override;
        const char* getName() const noexcept override { return "Movement"; }

        // Audio thread, start of block. Only sets smoothing targets.
        void setParameters (const MovementParameters& newParameters) noexcept;
        MovementParameters getParameters() const noexcept { return parameters; }

        int getActiveVoices() const noexcept { return engine.getActiveVoices(); }

    private:
        void applyParameters() noexcept;

        MovementParameters parameters;
        GranularEngine engine;
        TremoloProcessor tremolo;
    };
}
