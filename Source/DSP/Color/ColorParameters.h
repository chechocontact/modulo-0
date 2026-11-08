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

    // Oversampling of the non-linear stage (the saturation). Off = 1x.
    enum class OversamplingMode
    {
        off = 0,
        x2,
        x4
    };

    inline constexpr int oversamplingFactor (OversamplingMode mode) noexcept
    {
        return mode == OversamplingMode::x4 ? 4 : mode == OversamplingMode::x2 ? 2 : 1;
    }

    namespace color
    {
        inline constexpr float chorusRateMinHz = 0.05f;
        inline constexpr float chorusRateMaxHz = 5.0f;

        // Tone: tilt of +-toneMaxDb around tonePivotHz.
        inline constexpr float toneMaxDb = 6.0f;
        inline constexpr float tonePivotHz = 700.0f;

        // Drive 0..1 maps to 0..driveMaxDb of gain into the waveshaper.
        inline constexpr float driveMaxDb = 24.0f;

        // Magic Chorus is one knob; everything else is fixed.
        inline constexpr ChorusMode magicChorusMode = ChorusMode::ensemble;
        inline constexpr float magicChorusRateHz = 0.6f;
        inline constexpr float magicChorusDepth = 1.0f;
        inline constexpr float magicChorusWidth = 0.8f;
        inline constexpr float magicChorusMix = 0.5f;
    }

    struct ColorParameters
    {
        float drive = 0.25f;                     // 0..1
        ColorType type = ColorType::tube;
        float tone = 0.0f;                       // -1 (dark) .. +1 (bright)

        // MAGIC CHORUS: one knob. It scales the modulation depth of a lush
        // 3-voice ensemble and fades the effect in (0 = off, bit-transparent).
        float magicChorus = 0.5f;                // 0..1

        OversamplingMode oversampling = OversamplingMode::off;

        bool operator== (const ColorParameters&) const = default;
    };
}
