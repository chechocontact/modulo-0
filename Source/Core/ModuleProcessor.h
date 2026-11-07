#pragma once

#include "AudioBufferView.h"

namespace vm
{
    // Common contract for the four DSP modules (spec §4).
    //
    // Threading:
    //   prepare()  - message thread, audio stopped. The only place that may allocate.
    //   process()  - audio thread. In place. No allocation, no locks, no I/O.
    //   reset()    - audio thread or message thread. Must be real-time safe:
    //                ModuleSlot calls it from the audio thread when a bypassed
    //                module is switched back on.
    //
    // Each concrete module also exposes its own Parameters struct through
    // setParameters (const Parameters&) / getParameters(). setParameters is
    // called on the audio thread at the start of each block and must only set
    // smoothing targets. Bypass and dry/wet are handled outside the module
    // by ModuleSlot, so modules always output 100 % wet.
    class ModuleProcessor
    {
    public:
        virtual ~ModuleProcessor() = default;

        virtual void prepare (const ProcessSpec& spec) = 0;
        virtual void process (AudioBufferView block) noexcept = 0;
        virtual void reset() noexcept = 0;

        // Processing delay the module introduces, in samples. Constant after prepare().
        virtual int getLatencySamples() const noexcept { return 0; }

        virtual const char* getName() const noexcept = 0;
    };
}
