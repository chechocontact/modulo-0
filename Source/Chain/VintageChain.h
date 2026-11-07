#pragma once

#include "ChainParameters.h"
#include "Core/LevelMeter.h"
#include "Core/MasterProcessor.h"
#include "DSP/Color/ColorProcessor.h"
#include "DSP/Engine/VintageEngine.h"
#include "DSP/Movement/MovementProcessor.h"
#include "DSP/Space/SpaceProcessor.h"
#include "DSP/Texture/TextureProcessor.h"

namespace vm
{
    // The whole plugin without JUCE: MasterProcessor with the four modules in its
    // chain, the Vintage Engine, Magnitude and the level meters.
    //
    //   host parameters -> Vintage Engine offsets -> Magnitude -> module parameters
    //
    //   input meter -> Input Gain -> Texture -> Color -> Movement -> Space
    //               -> master mix / Output Gain / bypass -> vinyl noise -> output meter
    //
    // PluginProcessor is a thin adapter around this class, and the integration tests
    // drive it directly, so what is tested is what runs in the plugin.
    class VintageChain
    {
    public:
        VintageChain();

        // Message thread, audio stopped. Allocates. Starts from p without ramping.
        void prepare (const ProcessSpec& spec, const ChainParameters& p);
        void reset() noexcept;

        // Audio thread. In place.
        void process (AudioBufferView block, const ChainParameters& p) noexcept;

        int getLatencySamples() const noexcept { return master.getLatencySamples(); }

        const LevelMeter& getInputMeter() const noexcept { return inputMeter; }
        const LevelMeter& getOutputMeter() const noexcept { return outputMeter; }

        MasterProcessor& getMaster() noexcept { return master; }
        const VintageEngine& getVintageEngine() const noexcept { return vintageEngine; }

    private:
        void applyParameters (const ChainParameters& p) noexcept;

        MasterProcessor master;
        VintageEngine vintageEngine;
        LevelMeter inputMeter, outputMeter;

        TextureProcessor* texture = nullptr; // owned by master's engine
        ColorProcessor* color = nullptr;
        MovementProcessor* movement = nullptr;
        SpaceProcessor* space = nullptr;
    };
}
