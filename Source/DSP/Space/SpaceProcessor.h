#pragma once

#include "Core/ModuleProcessor.h"
#include "Core/SmoothedValue.h"
#include "DelayProcessor.h"
#include "ReverbProcessor.h"
#include "SpaceParameters.h"

namespace vm
{
    // Module 4 - SPACE.
    //
    //   z   = x + Delay * delayWet (x)
    //   out = z + Mix * reverbWet (z)        (reverb after the delay)
    //
    // Stereo opening (Width) shapes the delay's ping-pong and the reverb's side level.
    // Always 100 % wet at module level (bypass and dry/wet belong to ModuleSlot).
    // No reported latency. With Delay and Mix at zero it is bit-transparent.
    class SpaceProcessor final : public ModuleProcessor
    {
    public:
        void prepare (const ProcessSpec& spec) override;
        void process (AudioBufferView block) noexcept override;
        void reset() noexcept override;
        const char* getName() const noexcept override { return "Space"; }

        // Audio thread, start of block. Only sets smoothing targets.
        void setParameters (const SpaceParameters& newParameters) noexcept;
        SpaceParameters getParameters() const noexcept { return parameters; }

        // Host tempo (beats per minute) for Sync. Non-finite or non-positive values are ignored.
        void setTempo (double bpm) noexcept;

        // Seconds the delay lines currently aim for (free time or synced division).
        double getDelayTimeSeconds() const noexcept;

    private:
        void applyParameters() noexcept;

        double sampleRate = 44100.0;
        int numChannels = 2;
        double bpm = space::defaultBpm;
        bool reverbActive = false;

        SpaceParameters parameters;
        DelayProcessor delay;
        ReverbProcessor reverb;
        LinearSmoothedValue delayAmount, reverbAmount, widthAmount;
    };
}
