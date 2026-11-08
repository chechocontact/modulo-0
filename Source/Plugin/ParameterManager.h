#pragma once

#include "Chain/ChainParameters.h"
#include "Core/MasterProcessor.h"
#include "DSP/Color/ColorParameters.h"
#include "DSP/Movement/MovementParameters.h"
#include "DSP/Engine/VintageEngineParameters.h"
#include "DSP/Space/SpaceParameters.h"
#include "DSP/Texture/TextureParameters.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>

namespace vm
{
    // Owns the host-facing parameter set (APVTS) and translates it into the
    // JUCE-free Core parameter structs. The UI must change parameters only
    // through the APVTS, never by touching processor state (spec §9).
    class ParameterManager
    {
    public:
        explicit ParameterManager (juce::AudioProcessor& owner);

        juce::AudioProcessorValueTreeState& getState() noexcept { return state; }
        juce::AudioProcessorParameter* getBypassParameter() const noexcept { return bypassParameter; }

        // Audio thread. Lock-free: reads cached std::atomic<float> pointers.
        MasterParameters readMasterParameters() const noexcept;

        using SlotState = SlotSettings;

        // Everything for one block (the host tempo is added by the caller).
        ChainParameters readChainParameters() const noexcept;
        float readMagnitude() const noexcept;

        SlotState readTextureSlot() const noexcept;
        TextureParameters readTextureParameters() const noexcept;
        SlotState readColorSlot() const noexcept;
        ColorParameters readColorParameters() const noexcept;
        SlotState readMovementSlot() const noexcept;
        MovementParameters readMovementParameters() const noexcept;
        SlotState readSpaceSlot() const noexcept;
        SpaceParameters readSpaceParameters() const noexcept;
        VintageEngineParameters readEngineParameters() const noexcept;

        // Message thread (or any thread the host chooses; uses the APVTS lock).
        void saveState (juce::MemoryBlock& destination);
        void loadState (const void* data, int sizeInBytes);

        static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    private:
        juce::AudioProcessorValueTreeState state;

        std::atomic<float>* inputGain = nullptr;
        std::atomic<float>* outputGain = nullptr;
        std::atomic<float>* masterMix = nullptr;
        std::atomic<float>* masterBypass = nullptr;
        std::atomic<float>* vinylAmount = nullptr;
        std::atomic<float>* crackleType = nullptr;
        std::atomic<float>* magnitude = nullptr;
        juce::AudioProcessorParameter* bypassParameter = nullptr;

        struct TextureHandles
        {
            std::atomic<float>* enable = nullptr;
            std::atomic<float>* mix = nullptr;
            std::atomic<float>* wowDepth = nullptr;
            std::atomic<float>* wowRate = nullptr;
            std::atomic<float>* flutterDepth = nullptr;
            std::atomic<float>* flutterRate = nullptr;
            std::atomic<float>* vinylDrop = nullptr;
            std::atomic<float>* degrade = nullptr;
        } tex;

        struct ColorHandles
        {
            std::atomic<float>* enable = nullptr;
            std::atomic<float>* mix = nullptr;
            std::atomic<float>* drive = nullptr;
            std::atomic<float>* type = nullptr;
            std::atomic<float>* tone = nullptr;
            std::atomic<float>* magicChorus = nullptr;
        } col;

        struct MovementHandles
        {
            std::atomic<float>* enable = nullptr;
            std::atomic<float>* mix = nullptr;
            std::atomic<float>* grain = nullptr;
            std::atomic<float>* feedback = nullptr;
            std::atomic<float>* tremolo = nullptr;
            std::atomic<float>* rate = nullptr;
            std::atomic<float>* style = nullptr;
        } mov;

        struct SpaceHandles
        {
            std::atomic<float>* enable = nullptr;
            std::atomic<float>* mix = nullptr;
            std::atomic<float>* delay = nullptr;
            std::atomic<float>* time = nullptr;
            std::atomic<float>* feedback = nullptr;
            std::atomic<float>* sync = nullptr;
            std::atomic<float>* reverbMix = nullptr;
            std::atomic<float>* decay = nullptr;
            std::atomic<float>* type = nullptr;
            std::atomic<float>* width = nullptr;
        } spc;

        struct EngineHandles
        {
            std::atomic<float>* enable = nullptr;
            std::atomic<float>* flux = nullptr;
            std::atomic<float>* rate = nullptr;
            std::atomic<float>* depth = nullptr;
            std::atomic<float>* character = nullptr;
        } eng;
    };
}
