#pragma once

#include "ModuleProcessor.h"
#include "SmoothedValue.h"

#include <vector>

namespace vm
{
    // Wraps one ModuleProcessor with click-free bypass and dry/wet, so modules
    // never implement those themselves (spec §4, "activar, desactivar y mezclar
    // cada módulo sin modificar su implementación interna").
    //
    // When bypass has fully faded out the module is not processed at all (no
    // CPU). It is reset() before it is processed again, so no stale tail
    // (old delay lines, grains) plays back when it is re-enabled.
    class ModuleSlot
    {
    public:
        ModuleSlot() noexcept
        {
            enableGain.setCurrentAndTarget (1.0f);
            mix.setCurrentAndTarget (1.0f); // modules start enabled and 100 % wet
        }

        // Message thread, before prepare(). The slot does not own the processor.
        void setProcessor (ModuleProcessor* newProcessor) noexcept { processor = newProcessor; }
        ModuleProcessor* getProcessor() const noexcept { return processor; }

        void prepare (const ProcessSpec& spec);
        void reset() noexcept;

        // Audio thread, start of block.
        void setEnabled (bool shouldBeEnabled) noexcept;
        void setMix (float mix01) noexcept;

        bool isEnabled() const noexcept { return enabledTarget; }
        float getMix() const noexcept { return mix.getTarget(); }

        // Audio thread. block.getNumSamples() must be <= the prepared maxBlockSize.
        void process (AudioBufferView block) noexcept;

        int getLatencySamples() const noexcept;

    private:
        ModuleProcessor* processor = nullptr;
        std::vector<float> dryStorage;
        int maxBlockSize = 0;

        LinearSmoothedValue enableGain;
        LinearSmoothedValue mix;
        bool enabledTarget = true;
        bool needsReset = false;
    };
}
