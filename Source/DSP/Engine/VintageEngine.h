#pragma once

#include "DSP/Color/ColorParameters.h"
#include "DSP/Movement/MovementParameters.h"
#include "DSP/Space/SpaceParameters.h"
#include "DSP/Texture/TextureParameters.h"
#include "ModulationSource.h"
#include "VintageEngineParameters.h"

#include <array>

namespace vm
{
    // Module 5 - VINTAGE ENGINE.
    //
    // Not an audio processor. It produces slow, smooth, random drifts and adds them
    // as small OFFSETS to the parameters the host (or the user) set, at the moment
    // those are handed to the DSP modules. It never writes to host parameters, so
    // automation and presets are untouched.
    //
    //   Flux = 0 -> every offset is exactly 0: parameters pass through unchanged.
    //   amount   = Flux * Depth
    //   offset_i = smooth (modulation_i * amount * range_i)
    //
    // Destinations: Wow Depth/Rate, Flutter Depth/Rate, Vinyl Amount, Drive, Grain,
    // Tremolo Rate and Width. Results are always clamped to the destination's range.
    class VintageEngine
    {
    public:
        void prepare (double sampleRate);
        void reset() noexcept;

        // Audio thread, once per block.
        void setParameters (const VintageEngineParameters& newParameters) noexcept;
        VintageEngineParameters getParameters() const noexcept { return parameters; }

        // Moves the drifts on by one block and updates the offsets.
        void advance (int numSamples) noexcept;

        // Offsets in the destination's unit (0..1 knobs, Hz for rates), after smoothing.
        struct Offsets
        {
            float wowDepth = 0, wowRate = 0, flutterDepth = 0, flutterRate = 0, vinylAmount = 0,
                  drive = 0, grain = 0, tremoloRate = 0, width = 0;
        };
        const Offsets& getOffsets() const noexcept { return offsets; }

        // Add the offsets to parameters just read from the host. Clamped to valid ranges.
        void apply (TextureParameters& p) const noexcept;
        void apply (ColorParameters& p) const noexcept;
        void apply (MovementParameters& p) const noexcept;
        void apply (SpaceParameters& p) const noexcept;
        float applyToVinylAmount (float amount) const noexcept;

    private:
        static constexpr int numComponents = 3;

        void configureSources() noexcept;

        double sampleRate = 44100.0;
        VintageEngineParameters parameters;

        // Each destination has its own independent set of three drifts.
        std::array<std::array<ModulationSource, numComponents>, (std::size_t) engine::numDestinations> sources;
        std::array<float, numComponents> componentWeight {};
        std::array<float, (std::size_t) engine::numDestinations> smoothed {};
        Offsets offsets;
        bool justReset = true;
    };
}
