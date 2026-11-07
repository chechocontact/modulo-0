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

    // --- Module 1: TEXTURE (v1) ---
    inline constexpr const char* texEnable           = "texEnable";           // bool
    inline constexpr const char* texMix              = "texMix";              // percent
    inline constexpr const char* texWowDepth         = "texWowDepth";         // percent
    inline constexpr const char* texWowRate          = "texWowRate";          // Hz
    inline constexpr const char* texWowShape         = "texWowShape";         // percent
    inline constexpr const char* texFlutterDepth     = "texFlutterDepth";     // percent
    inline constexpr const char* texFlutterRate      = "texFlutterRate";      // Hz
    inline constexpr const char* texFlutterVariation = "texFlutterVariation"; // percent
    inline constexpr const char* texVinylAmount      = "texVinylAmount";      // percent
    inline constexpr const char* texCrackleType      = "texCrackleType";      // choice: Normal/Dusty/Heavy
    inline constexpr const char* texVinylDrop        = "texVinylDrop";        // percent
    inline constexpr const char* texDegradeAmount    = "texDegradeAmount";    // percent
    inline constexpr const char* texDegradeBandwidth = "texDegradeBandwidth"; // percent
    inline constexpr const char* texDegradeSmoothing = "texDegradeSmoothing"; // percent

    // Ramp time for gain/mix/bypass changes.
    inline constexpr double smoothingSeconds = 0.02;
}
