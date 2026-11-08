#pragma once

#include "Core/ParameterIds.h"

#include <cstddef>
#include <iterator>
#include <cstring>

namespace vm::presets
{
    // Every host parameter with its real-unit range, without JUCE. Used to validate the
    // factory presets in the Core tests; the loader of the plugin additionally clamps
    // every value through the real parameter range, so a stale entry here can never
    // produce an out-of-range parameter.
    struct ParameterRange
    {
        const char* id;
        float minimum;
        float maximum;
    };

    inline constexpr ParameterRange parameterTable[] {
        { param::inputGain, -24.0f, 24.0f },
        { param::outputGain, -24.0f, 24.0f },
        { param::masterMix, 0.0f, 100.0f },
        { param::masterBypass, 0.0f, 1.0f },
        { param::magnitude, 0.0f, 100.0f },
        { param::vinylAmount, 0.0f, 100.0f },

        { param::texEnable, 0.0f, 1.0f },
        { param::texMix, 0.0f, 100.0f },
        { param::texWowDepth, 0.0f, 100.0f },
        { param::texWowRate, 0.1f, 2.0f },
        { param::texFlutterDepth, 0.0f, 100.0f },
        { param::texFlutterRate, 4.0f, 15.0f },
        { param::crackleType, 0.0f, 2.0f },
        { param::texVinylDrop, 0.0f, 100.0f },
        { param::texDegradeAmount, 0.0f, 100.0f },

        { param::colEnable, 0.0f, 1.0f },
        { param::colMix, 0.0f, 100.0f },
        { param::colDrive, 0.0f, 100.0f },
        { param::colType, 0.0f, 2.0f },
        { param::colTone, -100.0f, 100.0f },
        { param::colMagicChorus, 0.0f, 100.0f },
        { param::colOversampling, 0.0f, 2.0f },

        { param::movEnable, 0.0f, 1.0f },
        { param::movMix, 0.0f, 100.0f },
        { param::movGrain, 0.0f, 100.0f },
        { param::movFeedback, 0.0f, 100.0f },
        { param::movTremolo, 0.0f, 100.0f },
        { param::movRate, 0.1f, 15.0f },
        { param::movStyle, 0.0f, 2.0f },

        { param::spcEnable, 0.0f, 1.0f },
        { param::spcMix, 0.0f, 100.0f },
        { param::spcDelay, 0.0f, 100.0f },
        { param::spcTime, 0.0f, 100.0f },
        { param::spcFeedback, 0.0f, 100.0f },
        { param::spcSync, 0.0f, 1.0f },
        { param::spcReverbMix, 0.0f, 100.0f },
        { param::spcDecay, 0.0f, 100.0f },
        { param::spcType, 0.0f, 2.0f },
        { param::spcWidth, 0.0f, 100.0f },

        { param::engEnable, 0.0f, 1.0f },
        { param::engFlux, 0.0f, 100.0f },
        { param::engRate, 0.0f, 100.0f },
        { param::engDepth, 0.0f, 100.0f },
        { param::engCharacter, 0.0f, 2.0f },
    };

    inline const ParameterRange* findParameter (const char* id) noexcept
    {
        for (const auto& entry : parameterTable)
            if (std::strcmp (entry.id, id) == 0)
                return &entry;

        return nullptr;
    }
}
