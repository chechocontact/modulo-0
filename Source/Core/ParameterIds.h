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

    // Ramp time for gain/mix/bypass changes.
    inline constexpr double smoothingSeconds = 0.02;
}
