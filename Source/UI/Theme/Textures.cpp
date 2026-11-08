#include "Textures.h"

#include "Palette.h"

#include <cmath>

namespace vm::ui
{
    namespace
    {
        float hash2 (int x, int y, unsigned int seed) noexcept
        {
            auto h = (unsigned int) x * 374761393u + (unsigned int) y * 668265263u + seed * 2246822519u;
            h = (h ^ (h >> 13)) * 1274126177u;
            h ^= h >> 16;
            return (float) (h & 0xffffffu) / (float) 0xffffffu;
        }

        float smooth (float t) noexcept { return t * t * (3.0f - 2.0f * t); }

        // Value noise in [0, 1].
        float valueNoise (float x, float y, unsigned int seed) noexcept
        {
            const int ix = (int) std::floor (x);
            const int iy = (int) std::floor (y);
            const float fx = smooth (x - (float) ix);
            const float fy = smooth (y - (float) iy);
            const float a = hash2 (ix, iy, seed);
            const float b = hash2 (ix + 1, iy, seed);
            const float c = hash2 (ix, iy + 1, seed);
            const float d = hash2 (ix + 1, iy + 1, seed);
            return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
        }

        float fbm (float x, float y, unsigned int seed) noexcept
        {
            return 0.55f * valueNoise (x, y, seed)
                 + 0.30f * valueNoise (x * 2.1f, y * 2.1f, seed + 1u)
                 + 0.15f * valueNoise (x * 4.3f, y * 4.3f, seed + 2u);
        }

        juce::Colour mix (juce::Colour a, juce::Colour b, float t) noexcept
        {
            return a.interpolatedWith (b, juce::jlimit (0.0f, 1.0f, t));
        }
    }

    juce::Image createWoodTexture (int width, int height, unsigned int seed)
    {
        juce::Image image (juce::Image::ARGB, width, height, false);
        juce::Image::BitmapData data (image, juce::Image::BitmapData::writeOnly);

        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                const float fx = (float) x;
                const float fy = (float) y;

                // Grain runs along x; the rings wobble slowly along the board.
                const float warp = 26.0f * fbm (fx * 0.0025f, fy * 0.012f, seed);
                const float ring = std::sin ((fy + warp) * 0.21f + 3.0f * fbm (fx * 0.004f, fy * 0.05f, seed + 5u));
                const float rings = 0.5f + 0.5f * ring;

                // Fine fibres: stretched noise (long in x, short in y).
                const float fibres = valueNoise (fx * 0.018f, fy * 1.15f, seed + 9u);
                const float pores = valueNoise (fx * 0.35f, fy * 0.9f, seed + 13u);

                float tone = 0.35f + 0.40f * rings + 0.25f * fibres;
                tone -= 0.10f * (pores > 0.82f ? 1.0f : 0.0f);

                // Soft vignette so the cabinet reads as a lit object.
                const float vx = (fx / (float) width - 0.5f) * 2.0f;
                const float vy = (fy / (float) height - 0.5f) * 2.0f;
                tone *= 1.0f - 0.22f * (vx * vx * 0.6f + vy * vy * 0.4f);

                data.setPixelColour (x, y, mix (colour::woodDark, colour::woodLight, tone));
            }
        }

        return image;
    }

    juce::Image createBrushedMetalTexture (int width, int height, unsigned int seed)
    {
        juce::Image image (juce::Image::ARGB, width, height, false);
        juce::Image::BitmapData data (image, juce::Image::BitmapData::writeOnly);

        for (int y = 0; y < height; ++y)
        {
            const float gradient = (float) y / (float) juce::jmax (1, height - 1);
            const auto base = mix (colour::faceTop, colour::faceBottom, gradient);

            for (int x = 0; x < width; ++x)
            {
                // Horizontal brushing: strongly stretched noise.
                const float streak = valueNoise ((float) x * 0.05f, (float) y * 1.7f, seed)
                                   + 0.5f * valueNoise ((float) x * 0.2f, (float) y * 3.1f, seed + 3u);
                const float delta = (streak - 0.75f) * 0.07f;
                data.setPixelColour (x, y, delta >= 0.0f ? base.brighter (delta) : base.darker (-delta));
            }
        }

        return image;
    }
}
