#pragma once

// Stable parameter identifiers and ranges, shared by the JUCE-free Core and the
// plugin layer. NEVER rename or remove an ID once released: hosts store
// automation and projects by these strings. Add new parameters with a new ID.

namespace vm::param
{
    // Version stored with every JUCE ParameterID. Bump only for an ID that
    // changes meaning, never for existing ones.
    inline constexpr int idVersion = 1;

    // Version of the saved plugin state (the XML root attribute).
    inline constexpr int stateVersion = 1;

    // --- Global (Module 0) ---
    inline constexpr const char* inputGain    = "inputGain";    // dB
    inline constexpr const char* outputGain   = "outputGain";   // dB
    inline constexpr const char* masterMix    = "masterMix";    // percent
    inline constexpr const char* masterBypass = "masterBypass"; // bool

    inline constexpr float gainMinDb = -24.0f;
    inline constexpr float gainMaxDb = 24.0f;
    inline constexpr float gainDefaultDb = 0.0f;

    inline constexpr float mixMinPercent = 0.0f;
    inline constexpr float mixMaxPercent = 100.0f;
    inline constexpr float mixDefaultPercent = 100.0f;

    // --- Vinyl noise (global: added at the very end of the plugin) ---
    inline constexpr const char* vinylAmount       = "texVinylAmount";    // percent (ID kept from v0.2)

    // --- Module 1: TEXTURE (v1) ---
    inline constexpr const char* texEnable         = "texEnable";         // bool
    inline constexpr const char* texMix            = "texMix";            // percent
    inline constexpr const char* texWowDepth       = "texWowDepth";       // percent
    inline constexpr const char* texWowRate        = "texWowRate";        // Hz
    inline constexpr const char* texFlutterDepth   = "texFlutterDepth";   // percent
    inline constexpr const char* texFlutterRate    = "texFlutterRate";    // Hz
    inline constexpr const char* texVinylDrop      = "texVinylDrop";      // percent
    inline constexpr const char* texDegradeAmount  = "texDegradeAmount";  // percent (one-knob Degrade)

    // --- Module 2: COLOR (v1) ---
    inline constexpr const char* colEnable         = "colEnable";         // bool
    inline constexpr const char* colMix            = "colMix";            // percent
    inline constexpr const char* colDrive          = "colDrive";          // percent
    inline constexpr const char* colType           = "colType";           // choice: Tube/Tape/Console
    inline constexpr const char* colTone           = "colTone";           // -100..+100 percent
    inline constexpr const char* colMagicChorus    = "colChorusAmount";   // percent (ID kept from v0.3)

    // --- Module 3: MOVEMENT (v1) ---
    inline constexpr const char* movEnable         = "movEnable";         // bool
    inline constexpr const char* movMix            = "movMix";            // percent
    inline constexpr const char* movGrain          = "movGrain";          // percent
    inline constexpr const char* movFeedback       = "movFeedback";       // percent
    inline constexpr const char* movTremolo        = "movTremolo";        // percent (tremolo depth)
    inline constexpr const char* movRate           = "movRate";           // Hz (tremolo rate)
    inline constexpr const char* movStyle          = "movStyle";          // choice: Frozen/Drift/Cloud

    // --- Module 4: SPACE (v1) ---
    inline constexpr const char* spcEnable         = "spcEnable";         // bool
    inline constexpr const char* spcMix            = "spcMix";            // percent (slot Dry/Wet)
    inline constexpr const char* spcDelay          = "spcDelay";          // percent (delay amount)
    inline constexpr const char* spcTime           = "spcTime";           // percent (free time / division)
    inline constexpr const char* spcFeedback       = "spcFeedback";       // percent
    inline constexpr const char* spcSync           = "spcSync";           // bool
    inline constexpr const char* spcReverbMix      = "spcReverbMix";      // percent (reverb amount)
    inline constexpr const char* spcDecay          = "spcDecay";          // percent
    inline constexpr const char* spcType           = "spcType";           // choice: Room/Plate/Hall
    inline constexpr const char* spcWidth          = "spcWidth";          // percent (stereo opening)

    // --- Module 5: VINTAGE ENGINE (v1) ---
    inline constexpr const char* engFlux           = "engFlux";           // percent
    inline constexpr const char* engRate           = "engRate";           // percent
    inline constexpr const char* engDepth          = "engDepth";          // percent
    inline constexpr const char* engCharacter      = "engCharacter";      // choice: Slow/Organic/Random

    // Ramp time for gain/mix/bypass changes.
    inline constexpr double smoothingSeconds = 0.02;
}
