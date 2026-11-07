#pragma once

#include "Core/ModuleProcessor.h"
#include "DegradeProcessor.h"
#include "TextureParameters.h"
#include "VinylDrop.h"
#include "WowFlutter.h"

namespace vm
{
    // Module 1 - TEXTURE.
    //
    //   input -> wow + flutter (pitch) -> degrade -> drop level dip
    //
    // Always 100 % wet (bypass and dry/wet belong to ModuleSlot). No latency:
    // with Wow, Flutter and Drop at zero the delay is exactly zero.
    // The vinyl noise is NOT here: it is added at the end of the plugin.
    //
    // Degrade (one knob, d = 0..1):
    //   amount     = d
    //   bandwidth  = 1 - 0.8 d   (wide -> 1.9 kHz low-pass / 300 Hz high-pass)
    //   smoothing  = 0.7 d       (transient softening)
    class TextureProcessor final : public ModuleProcessor
    {
    public:
        void prepare (const ProcessSpec& spec) override;
        void process (AudioBufferView block) noexcept override;
        void reset() noexcept override;
        const char* getName() const noexcept override { return "Texture"; }

        // Audio thread, start of block. Only sets smoothing targets.
        void setParameters (const TextureParameters& newParameters) noexcept;
        TextureParameters getParameters() const noexcept { return parameters; }

        // Starts a vinyl drop immediately (tests / future "trigger" control).
        void triggerDrop (double seconds) noexcept { drop.force (seconds); }

    private:
        void applyParameters() noexcept;

        TextureParameters parameters;
        WowFlutter wowFlutter;
        VinylDrop drop;
        DegradeProcessor degrade;
        int numChannels = 2;
        float dropExcursion = 0.0f; // samples at Drop = 1
    };
}
