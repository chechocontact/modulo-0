#pragma once

#include "ModuleSlot.h"
#include "SmoothedValue.h"

#include <array>
#include <memory>

namespace vm
{
    // Serial chain: Input Gain -> TEXTURE -> COLOR -> MOVEMENT -> SPACE.
    // Output gain, master mix and master bypass live in MasterProcessor.
    class AudioEngine
    {
    public:
        enum class Slot
        {
            texture = 0,
            color,
            movement,
            space
        };

        static constexpr int numSlots = 4;

        AudioEngine();

        // Message thread, audio stopped (before prepare, or between release and prepare).
        void setModule (Slot slot, std::unique_ptr<ModuleProcessor> module);
        ModuleProcessor* getModule (Slot slot) const noexcept;

        ModuleSlot& getSlot (Slot slot) noexcept { return slots[(std::size_t) slot]; }
        const ModuleSlot& getSlot (Slot slot) const noexcept { return slots[(std::size_t) slot]; }

        void prepare (const ProcessSpec& spec);
        void reset() noexcept;

        // Audio thread.
        void setInputGainDb (float decibels) noexcept;
        void process (AudioBufferView block) noexcept;

        int getLatencySamples() const noexcept;

    private:
        void applyInputGain (AudioBufferView block) noexcept;

        std::array<std::unique_ptr<ModuleProcessor>, numSlots> modules;
        std::array<ModuleSlot, numSlots> slots;
        LinearSmoothedValue inputGain;
    };
}
