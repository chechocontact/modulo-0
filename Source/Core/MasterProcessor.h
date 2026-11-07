#pragma once

#include "AudioEngine.h"
#include "SmoothedValue.h"
#include "VinylNoise.h"

#include <vector>

namespace vm
{
    struct MasterParameters
    {
        float inputGainDb = 0.0f;   // -24 .. +24
        float outputGainDb = 0.0f;  // -24 .. +24
        float mix = 1.0f;           // 0 .. 1 (dry = original input, before input gain)
        float vinylAmount = 0.0f;   // 0 .. 1: vinyl noise added at the very end
        bool bypass = false;

        bool operator== (const MasterParameters&) const = default;
    };

    // Top level of the Core. Owns the AudioEngine and applies, per sample:
    //
    //   wet       = Engine (inputGain * x)
    //   processed = outputGain * (x + mix * (wet - x))
    //   y         = x + active * (processed - x)        active: 1 = on, 0 = bypassed
    //   out       = y + active * vinylNoise
    //
    // The vinyl noise is added last, so no module, the master mix or the output
    // gain can alter it. It follows the bypass (silent when bypassed).
    //
    // Output gain is applied after the master mix so it sets the plugin's final
    // level at any mix position. Master bypass is a true, click-free pass-through
    // (output gain does not apply when bypassed).
    //
    // Blocks larger than the prepared maxBlockSize are split internally, since
    // some hosts (FL Studio among them) may exceed the size given to prepareToPlay.
    class MasterProcessor
    {
    public:
        MasterProcessor();

        AudioEngine& getEngine() noexcept { return engine; }
        const AudioEngine& getEngine() const noexcept { return engine; }

        // Message thread, audio stopped. Allocates.
        void prepare (const ProcessSpec& spec);
        void reset() noexcept;

        // Audio thread, start of block. Only sets smoothing targets.
        void setParameters (const MasterParameters& newParameters) noexcept;
        MasterParameters getParameters() const noexcept { return parameters; }

        // Audio thread. In place. Channels beyond maxChannels are left untouched.
        void process (AudioBufferView block) noexcept;

        int getLatencySamples() const noexcept { return engine.getLatencySamples(); }
        bool isPrepared() const noexcept { return prepared; }

    private:
        void processChunk (AudioBufferView block) noexcept;
        void addVinyl (AudioBufferView block) noexcept; // output stage, fully-on path

        AudioEngine engine;
        VinylNoise vinyl;
        std::vector<float> dryStorage;
        ProcessSpec currentSpec;
        MasterParameters parameters;

        LinearSmoothedValue mix;
        LinearSmoothedValue outputGain;
        LinearSmoothedValue active;
        bool engineNeedsReset = false;
        bool prepared = false;
    };
}
