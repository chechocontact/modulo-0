#pragma once

namespace vm
{
    enum class ColorType
    {
        tube = 0,
        tape,
        console
    };

    enum class ChorusMode
    {
        classic = 0,
        dimension,
        ensemble
    };

    namespace color
    {
        inline constexpr float focusLowMinHz = 20.0f;
        inline constexpr float focusLowMaxHz = 1000.0f;
        inline constexpr float focusHighMinHz = 2000.0f;
        inline constexpr float focusHighMaxHz = 20000.0f;
        inline constexpr float chorusRateMinHz = 0.05f;
        inline constexpr float chorusRateMaxHz = 5.0f;

        // Tone: tilt of +-toneMaxDb around tonePivotHz.
        inline constexpr float toneMaxDb = 6.0f;
        inline constexpr float tonePivotHz = 700.0f;

        // Drive 0..1 maps to 0..driveMaxDb of gain into the waveshaper.
        inline constexpr float driveMaxDb = 24.0f;
    }

    struct ColorParameters
    {
        // DRIVE + COLOR TYPE
        float drive = 0.25f;                     // 0..1
        ColorType type = ColorType::tube;

        // TONE
        float tone = 0.0f;                       // -1 (dark) .. +1 (bright)

        // FOCUS: band [focusLow, focusHigh]; amount crossfades from the clean signal
        float focusLow = 80.0f;                  // Hz
        float focusHigh = 14000.0f;              // Hz
        float focusAmount = 0.0f;                // 0..1

        // MAGIC CHORUS
        float chorusAmount = 0.6f;               // 0..1: modulation depth scale; 0 = off
        float chorusRate = 0.8f;                 // Hz
        float chorusDepth = 0.7f;                // 0..1
        float chorusWidth = 0.7f;                // 0..1: L/R LFO phase offset
        float chorusMix = 0.5f;                  // 0..1: dry/wet of the chorus
        ChorusMode chorusMode = ChorusMode::classic;

        bool operator== (const ColorParameters&) const = default;
    };
}
