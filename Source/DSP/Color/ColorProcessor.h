#pragma once

#include "ChorusProcessor.h"
#include "ColorParameters.h"
#include "Core/ModuleProcessor.h"
#include "SaturationProcessor.h"
#include "ToneProcessor.h"

namespace vm
{
    // Module 2 - COLOR.
    //
    //   input -> Saturation (Drive, Type) -> Tone -> Magic Chorus
    //
    // Always 100 % wet (bypass and dry/wet belong to ModuleSlot). No latency.
    // With Drive, Tone and Magic Chorus at zero it is bit-transparent.
    class ColorProcessor final : public ModuleProcessor
    {
    public:
        void prepare (const ProcessSpec& spec) override;
        void process (AudioBufferView block) noexcept override;
        void reset() noexcept override;
        const char* getName() const noexcept override { return "Color"; }

        // Audio thread, start of block. Only sets smoothing targets.
        void setParameters (const ColorParameters& newParameters) noexcept;
        ColorParameters getParameters() const noexcept { return parameters; }

        // Tests / future Oversampling control.
        void setAntiAliasing (bool enabled) noexcept { saturation.setAntiAliasing (enabled); }

    private:
        void applyParameters() noexcept;

        ColorParameters parameters;
        SaturationProcessor saturation;
        ToneProcessor tone;
        ChorusProcessor chorus;
        int numChannels = 2;
    };
}
