#pragma once

#include "Core/ParameterIds.h"

#include <array>
#include <cstddef>

namespace vm::presets
{
    // Factory presets, in real units (percent, Hz, dB, choice index, 0/1 for switches).
    // Any parameter a preset does not list returns to its default, so every preset is a
    // complete, reproducible state. Bypass is never part of a preset.
    struct PresetValue
    {
        const char* id;
        float value;
    };

    struct FactoryPreset
    {
        const char* name;
        const PresetValue* values;
        std::size_t count;
    };

    namespace detail
    {
        // Choice indices (match the parameter layout)
        inline constexpr float tube = 0, tape = 1, console = 2;                 // Color Type
        inline constexpr float normal = 0, dusty = 1, heavy = 2;                // Crackle
        inline constexpr float frozen = 0, drift = 1, cloud = 2;                // Movement Style
        inline constexpr float room = 0, plate = 1, hall = 2;                   // Reverb Type
        inline constexpr float slow = 0, organic = 1, random = 2;               // Engine Character

        inline constexpr PresetValue cassetteVibes[] {
            { param::texWowDepth, 35 }, { param::texWowRate, 0.5f }, { param::texFlutterDepth, 20 }, { param::texFlutterRate, 7 },
            { param::vinylAmount, 8 }, { param::crackleType, normal }, { param::texVinylDrop, 10 }, { param::texDegradeAmount, 30 },
            { param::colDrive, 30 }, { param::colType, tape }, { param::colTone, -15 }, { param::colMagicChorus, 25 },
            { param::movGrain, 15 }, { param::movFeedback, 15 }, { param::movTremolo, 10 }, { param::movRate, 3.5f }, { param::movStyle, drift },
            { param::spcDelay, 18 }, { param::spcTime, 60 }, { param::spcFeedback, 30 }, { param::spcReverbMix, 22 },
            { param::spcDecay, 45 }, { param::spcType, plate }, { param::spcWidth, 70 },
            { param::engFlux, 45 }, { param::engRate, 35 }, { param::engDepth, 60 }, { param::engCharacter, organic },
        };

        inline constexpr PresetValue dustyVinyl[] {
            { param::texWowDepth, 20 }, { param::texFlutterDepth, 10 }, { param::vinylAmount, 45 }, { param::crackleType, dusty },
            { param::texVinylDrop, 35 }, { param::texDegradeAmount, 40 },
            { param::colDrive, 25 }, { param::colType, tube }, { param::colTone, -25 }, { param::colMagicChorus, 10 },
            { param::movGrain, 10 }, { param::movTremolo, 0 },
            { param::spcDelay, 0 }, { param::spcReverbMix, 18 }, { param::spcDecay, 30 }, { param::spcType, room }, { param::spcWidth, 50 },
            { param::engFlux, 30 }, { param::engCharacter, slow },
        };

        inline constexpr PresetValue warmTape[] {
            { param::texWowDepth, 15 }, { param::texFlutterDepth, 10 }, { param::texDegradeAmount, 15 },
            { param::colDrive, 60 }, { param::colType, tape }, { param::colTone, -10 }, { param::colMagicChorus, 0 },
            { param::movGrain, 0 }, { param::movTremolo, 0 },
            { param::spcDelay, 0 }, { param::spcReverbMix, 12 }, { param::spcType, room }, { param::spcDecay, 30 },
            { param::engFlux, 25 }, { param::engCharacter, organic },
        };

        inline constexpr PresetValue brokenReel[] {
            { param::texWowDepth, 80 }, { param::texWowRate, 0.8f }, { param::texFlutterDepth, 60 }, { param::texFlutterRate, 11 },
            { param::vinylAmount, 20 }, { param::crackleType, heavy }, { param::texVinylDrop, 60 }, { param::texDegradeAmount, 55 },
            { param::colDrive, 45 }, { param::colType, console }, { param::colTone, -35 }, { param::colMagicChorus, 0 },
            { param::movGrain, 10 }, { param::movTremolo, 35 }, { param::movRate, 5.5f },
            { param::spcDelay, 10 }, { param::spcReverbMix, 15 }, { param::spcType, room },
            { param::engFlux, 85 }, { param::engRate, 70 }, { param::engDepth, 85 }, { param::engCharacter, random },
        };

        inline constexpr PresetValue dreamCloud[] {
            { param::texWowDepth, 15 }, { param::texFlutterDepth, 8 }, { param::texDegradeAmount, 10 },
            { param::colDrive, 15 }, { param::colType, tube }, { param::colTone, 10 }, { param::colMagicChorus, 55 },
            { param::movGrain, 70 }, { param::movFeedback, 45 }, { param::movTremolo, 12 }, { param::movRate, 2.2f }, { param::movStyle, cloud },
            { param::spcDelay, 30 }, { param::spcTime, 70 }, { param::spcFeedback, 45 }, { param::spcReverbMix, 50 },
            { param::spcDecay, 75 }, { param::spcType, hall }, { param::spcWidth, 100 },
            { param::engFlux, 40 }, { param::engCharacter, slow },
        };

        inline constexpr PresetValue frozenMemory[] {
            { param::texWowDepth, 10 }, { param::texDegradeAmount, 20 }, { param::vinylAmount, 5 },
            { param::colDrive, 20 }, { param::colMagicChorus, 35 },
            { param::movGrain, 85 }, { param::movFeedback, 60 }, { param::movTremolo, 0 }, { param::movStyle, frozen },
            { param::spcDelay, 20 }, { param::spcFeedback, 50 }, { param::spcReverbMix, 45 }, { param::spcDecay, 85 },
            { param::spcType, hall }, { param::spcWidth, 90 },
            { param::engFlux, 35 }, { param::engCharacter, slow },
        };

        inline constexpr PresetValue loFiRoom[] {
            { param::texWowDepth, 30 }, { param::texFlutterDepth, 25 }, { param::vinylAmount, 15 }, { param::crackleType, dusty },
            { param::texDegradeAmount, 60 },
            { param::colDrive, 40 }, { param::colType, console }, { param::colTone, -30 }, { param::colMagicChorus, 10 },
            { param::movGrain, 5 }, { param::movTremolo, 15 },
            { param::spcDelay, 10 }, { param::spcTime, 40 }, { param::spcReverbMix, 30 }, { param::spcDecay, 35 }, { param::spcType, room },
            { param::engFlux, 40 }, { param::engCharacter, organic },
        };

        inline constexpr PresetValue slapbackTape[] {
            { param::texWowDepth, 15 }, { param::texFlutterDepth, 10 }, { param::texDegradeAmount, 15 },
            { param::colDrive, 35 }, { param::colType, tape }, { param::colTone, 10 }, { param::colMagicChorus, 0 },
            { param::movGrain, 0 }, { param::movTremolo, 0 },
            { param::spcDelay, 45 }, { param::spcTime, 35 }, { param::spcFeedback, 20 }, { param::spcSync, 0 },
            { param::spcReverbMix, 10 }, { param::spcType, plate }, { param::spcDecay, 35 }, { param::spcWidth, 60 },
            { param::engFlux, 30 }, { param::engCharacter, organic },
        };

        inline constexpr PresetValue ensembleGlow[] {
            { param::texWowDepth, 10 }, { param::texFlutterDepth, 8 }, { param::texDegradeAmount, 10 },
            { param::colDrive, 20 }, { param::colType, tube }, { param::colTone, 20 }, { param::colMagicChorus, 85 },
            { param::movGrain, 0 }, { param::movTremolo, 0 },
            { param::spcDelay, 0 }, { param::spcReverbMix, 25 }, { param::spcType, plate }, { param::spcDecay, 50 },
            { param::engFlux, 25 }, { param::engCharacter, slow },
        };

        inline constexpr PresetValue seasickTremolo[] {
            { param::texWowDepth, 55 }, { param::texWowRate, 0.4f }, { param::texFlutterDepth, 15 }, { param::texDegradeAmount, 20 },
            { param::colDrive, 25 }, { param::colType, tape }, { param::colMagicChorus, 40 },
            { param::movGrain, 0 }, { param::movTremolo, 70 }, { param::movRate, 6.0f },
            { param::spcDelay, 15 }, { param::spcReverbMix, 20 }, { param::spcType, plate },
            { param::engFlux, 50 }, { param::engCharacter, slow },
        };

        template <std::size_t N>
        constexpr FactoryPreset make (const char* name, const PresetValue (&values)[N]) { return { name, values, N }; }
    }

    // The first preset is the plain default state.
    inline constexpr std::array<FactoryPreset, 11> factoryPresets {{
        { "Default", nullptr, 0 },
        detail::make ("Cassette Vibes", detail::cassetteVibes),
        detail::make ("Dusty Vinyl", detail::dustyVinyl),
        detail::make ("Warm Tape", detail::warmTape),
        detail::make ("Broken Reel", detail::brokenReel),
        detail::make ("Dream Cloud", detail::dreamCloud),
        detail::make ("Frozen Memory", detail::frozenMemory),
        detail::make ("Lo-fi Room", detail::loFiRoom),
        detail::make ("Slapback Tape", detail::slapbackTape),
        detail::make ("Ensemble Glow", detail::ensembleGlow),
        detail::make ("Seasick Tremolo", detail::seasickTremolo),
    }};
}
