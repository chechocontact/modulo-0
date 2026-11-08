// Vintage Maker — Module 8 (PRESETS) tests: the JUCE-free part, i.e. the factory
// preset data. Console program; exit code 0 = all checks passed.
//
// The file/UI side of the preset system (PresetManager, which needs JUCE) is
// verified by compiling it and by the pluginval state tests in CI.

#include "Presets/FactoryPresets.h"
#include "Presets/ParameterTable.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <set>
#include <string>

static int failures = 0;
static int checks = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        ++checks;                                                                \
        if (! (cond)) {                                                          \
            ++failures;                                                          \
            std::printf ("  FAIL line %d  %s\n", __LINE__, #cond);               \
        }                                                                        \
    } while (false)

using namespace vm::presets;

namespace
{
    void testNamesAndCount()
    {
        CHECK (factoryPresets.size() >= 8);
        CHECK (std::strcmp (factoryPresets[0].name, "Default") == 0);
        CHECK (factoryPresets[0].count == 0);

        std::set<std::string> names;
        for (const auto& preset : factoryPresets)
        {
            CHECK (preset.name != nullptr && preset.name[0] != '\0');
            CHECK (names.insert (preset.name).second); // unique

            // Names become file names when a user saves over them, so keep them plain.
            for (const char* c = preset.name; *c; ++c)
                CHECK (std::isalnum ((unsigned char) *c) || *c == ' ' || *c == '-');
        }
    }

    void testValuesAreValid()
    {
        for (const auto& preset : factoryPresets)
        {
            std::set<std::string> seen;
            for (std::size_t i = 0; i < preset.count; ++i)
            {
                const auto& v = preset.values[i];
                const auto* range = findParameter (v.id);

                if (range == nullptr)
                {
                    std::printf ("  preset '%s': unknown parameter '%s'\n", preset.name, v.id);
                    CHECK (false);
                    continue;
                }

                CHECK (seen.insert (v.id).second); // no duplicates within one preset
                CHECK (std::isfinite (v.value));

                const bool inRange = v.value >= range->minimum && v.value <= range->maximum;
                if (! inRange)
                    std::printf ("  preset '%s': %s = %g outside [%g, %g]\n", preset.name, v.id, v.value, range->minimum, range->maximum);
                CHECK (inRange);

                // Switches and choices are whole numbers.
                if (range->maximum <= 2.0f && range->minimum == 0.0f && range->maximum == std::floor (range->maximum))
                    CHECK (v.value == std::floor (v.value));
            }

            // A preset never carries bypass (it is a host-level switch, not part of the sound).
            CHECK (seen.count (vm::param::masterBypass) == 0);
        }
    }

    void testParameterTable()
    {
        std::set<std::string> ids;
        for (const auto& entry : parameterTable)
        {
            CHECK (ids.insert (entry.id).second);
            CHECK (entry.minimum < entry.maximum);
        }

        // Every ID of ParameterIds.h is in the table (a new parameter must be added here too).
        const char* expected[] {
            vm::param::inputGain, vm::param::outputGain, vm::param::masterMix, vm::param::masterBypass, vm::param::magnitude,
            vm::param::vinylAmount, vm::param::texEnable, vm::param::texMix, vm::param::texWowDepth, vm::param::texWowRate,
            vm::param::texFlutterDepth, vm::param::texFlutterRate, vm::param::crackleType, vm::param::texVinylDrop,
            vm::param::texDegradeAmount, vm::param::colEnable, vm::param::colMix, vm::param::colDrive, vm::param::colType,
            vm::param::colTone, vm::param::colMagicChorus, vm::param::colOversampling, vm::param::movEnable, vm::param::movMix,
            vm::param::movGrain, vm::param::movFeedback, vm::param::movTremolo, vm::param::movRate, vm::param::movStyle,
            vm::param::spcEnable, vm::param::spcMix, vm::param::spcDelay, vm::param::spcTime, vm::param::spcFeedback,
            vm::param::spcSync, vm::param::spcReverbMix, vm::param::spcDecay, vm::param::spcType, vm::param::spcWidth,
            vm::param::engEnable, vm::param::engFlux, vm::param::engRate, vm::param::engDepth, vm::param::engCharacter,
        };
        for (const char* id : expected)
            CHECK (findParameter (id) != nullptr);
        CHECK (sizeof (expected) / sizeof (expected[0]) == std::size (parameterTable));
    }

    void testPresetsDiffer()
    {
        // Different presets must actually differ (not copies of each other).
        std::set<std::string> signatures;
        for (const auto& preset : factoryPresets)
        {
            std::string s;
            for (std::size_t i = 0; i < preset.count; ++i)
                s += std::string (preset.values[i].id) + "=" + std::to_string (preset.values[i].value) + ";";
            CHECK (signatures.insert (s).second);
        }
    }
}

int main()
{
    struct Case { const char* name; void (*fn)(); };
    const Case cases[] {
        { "Factory presets: names", testNamesAndCount },
        { "Factory presets: parameters and ranges", testValuesAreValid },
        { "Parameter table", testParameterTable },
        { "Presets differ", testPresetsDiffer },
    };

    for (const auto& c : cases)
    {
        const int before = failures;
        std::printf ("[ RUN  ] %s\n", c.name);
        c.fn();
        std::printf ("[ %s ] %s\n", failures == before ? " OK " : "FAIL", c.name);
    }

    std::printf ("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
