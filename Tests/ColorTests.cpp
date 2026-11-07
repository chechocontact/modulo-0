// Vintage Maker — Module 2 (COLOR) tests. Console program, no JUCE.
// Exit code 0 = all checks passed.

#include "DSP/Color/ChorusProcessor.h"
#include "DSP/Color/ColorProcessor.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <random>
#include <vector>

// GCC mis-reports the replaced global allocators below once they are inlined.
#if defined(__GNUC__) && ! defined(__clang__)
 #pragma GCC diagnostic ignored "-Wmismatched-new-delete"
#endif

static std::atomic<bool> countAllocations { false };
static std::atomic<long> allocationCount { 0 };

void* operator new (std::size_t size)
{
    if (countAllocations.load (std::memory_order_relaxed))
        allocationCount.fetch_add (1, std::memory_order_relaxed);

    if (void* p = std::malloc (size == 0 ? 1 : size))
        return p;

    throw std::bad_alloc();
}

void operator delete (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }
void* operator new[] (std::size_t size)
{
    if (countAllocations.load (std::memory_order_relaxed))
        allocationCount.fetch_add (1, std::memory_order_relaxed);

    if (void* p = std::malloc (size == 0 ? 1 : size))
        return p;

    throw std::bad_alloc();
}
void operator delete[] (void* p) noexcept { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept { std::free (p); }

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

#define CHECK_RANGE(value, low, high)                                            \
    do {                                                                         \
        ++checks;                                                                \
        const double v_ = (value);                                               \
        if (! (v_ >= (low) && v_ <= (high))) {                                   \
            ++failures;                                                          \
            std::printf ("  FAIL line %d  %s = %.6g, expected [%g, %g]\n",       \
                         __LINE__, #value, v_, (double) (low), (double) (high)); \
        }                                                                        \
    } while (false)

using namespace vm;

namespace
{
    constexpr double pi = 3.14159265358979323846;

    struct Buffer
    {
        Buffer (int channels, int samples)
            : data ((std::size_t) channels, std::vector<float> ((std::size_t) samples, 0.0f))
        {
            rebuildPointers();
        }

        Buffer (const Buffer& o) : data (o.data) { rebuildPointers(); }
        Buffer& operator= (const Buffer& o) { data = o.data; rebuildPointers(); return *this; }

        void rebuildPointers()
        {
            pointers.clear();
            for (auto& c : data)
                pointers.push_back (c.data());
        }

        int channels() const { return (int) data.size(); }
        int samples() const { return (int) data[0].size(); }
        AudioBufferView view() { return { pointers.data(), channels(), samples() }; }

        std::vector<std::vector<float>> data;
        std::vector<float*> pointers;
    };

    void fillSine (Buffer& b, double freq, double sr, float amp)
    {
        for (int ch = 0; ch < b.channels(); ++ch)
            for (int i = 0; i < b.samples(); ++i)
                b.data[(std::size_t) ch][(std::size_t) i] = amp * (float) std::sin (2.0 * pi * freq * i / sr);
    }

    void fillNoise (Buffer& b, unsigned seed, float amp)
    {
        std::mt19937 rng (seed);
        std::uniform_real_distribution<float> d (-amp, amp);
        for (auto& c : b.data)
            for (auto& s : c)
                s = d (rng);
    }

    bool allFinite (const Buffer& b)
    {
        for (const auto& c : b.data)
            for (float s : c)
                if (! std::isfinite (s))
                    return false;
        return true;
    }

    double rms (const std::vector<float>& v, int from, int to)
    {
        double sum = 0.0;
        for (int i = from; i < to; ++i)
            sum += (double) v[(std::size_t) i] * v[(std::size_t) i];
        return std::sqrt (sum / std::max (1, to - from));
    }

    double toDb (double linear) { return 20.0 * std::log10 (std::max (linear, 1.0e-12)); }

    double maxStep (const std::vector<float>& v, int from, int to)
    {
        double m = 0.0;
        for (int i = std::max (from, 1); i < to; ++i)
            m = std::max (m, (double) std::abs (v[(std::size_t) i] - v[(std::size_t) i - 1]));
        return m;
    }

    // Everything off: the processor must be a no-op.
    ColorParameters allOff()
    {
        ColorParameters p;
        p.drive = p.tone = p.magicChorus = 0.0f;
        return p;
    }

    struct Rig
    {
        explicit Rig (const ColorParameters& p, double sampleRate = 48000.0, int channels = 2, int maxBlock = 512)
        {
            processor.prepare ({ sampleRate, maxBlock, channels });
            processor.setParameters (p);
            processor.reset();
        }

        void run (Buffer& b, int blockSize)
        {
            for (int start = 0; start < b.samples(); start += blockSize)
                processor.process (b.view().getSubBlock (start, std::min (blockSize, b.samples() - start)));
        }

        ColorProcessor processor;
    };

    // Drives a ChorusProcessor directly (its mode, rate, depth, width and mix are
    // fixed inside ColorProcessor, so the full control set is tested here).
    struct ChorusRig
    {
        explicit ChorusRig (const ChorusProcessor::Params& p, double sampleRate = 48000.0, int channels = 2)
        {
            chorus.prepare ({ sampleRate, 512, channels });
            chorus.setParams (p);
            chorus.reset();
        }

        void run (Buffer& b, int start = 0, int length = -1)
        {
            const int end = length < 0 ? b.samples() : start + length;
            for (int i = start; i < end; ++i)
            {
                chorus.advance();
                for (int ch = 0; ch < b.channels(); ++ch)
                {
                    float& x = b.data[(std::size_t) ch][(std::size_t) i];
                    x = chorus.process (ch, x);
                }
            }
        }

        ChorusProcessor chorus;
    };

    ChorusProcessor::Params chorusParams (ChorusMode mode, float amount = 1.0f, float rate = 0.8f, float depth = 1.0f,
                                          float width = 0.5f, float mix = 1.0f)
    {
        return { amount, rate, depth, width, mix, mode };
    }

    // ---- spectrum helpers ---------------------------------------------------

    using Complex = std::complex<double>;

    void fft (std::vector<Complex>& a)
    {
        const std::size_t n = a.size();
        for (std::size_t i = 1, j = 0; i < n; ++i)
        {
            std::size_t bit = n >> 1;
            for (; j & bit; bit >>= 1)
                j ^= bit;
            j ^= bit;
            if (i < j)
                std::swap (a[i], a[j]);
        }
        for (std::size_t len = 2; len <= n; len <<= 1)
        {
            const double angle = -2.0 * pi / (double) len;
            const Complex wl (std::cos (angle), std::sin (angle));
            for (std::size_t i = 0; i < n; i += len)
            {
                Complex w (1.0, 0.0);
                for (std::size_t j = 0; j < len / 2; ++j)
                {
                    const Complex u = a[i + j], v = a[i + j + len / 2] * w;
                    a[i + j] = u + v;
                    a[i + j + len / 2] = u - v;
                    w *= wl;
                }
            }
        }
    }

    constexpr int N = 8192;

    // Power spectrum (bins 0..N/2) of the last N samples. The test signal has
    // exactly `cycles` periods in N samples, so there is no leakage.
    std::vector<double> powerSpectrum (const std::vector<float>& y)
    {
        std::vector<Complex> a (N);
        const std::size_t offset = y.size() - N;
        for (int i = 0; i < N; ++i)
            a[(std::size_t) i] = (double) y[offset + (std::size_t) i];
        fft (a);

        std::vector<double> p (N / 2 + 1);
        for (int k = 0; k <= N / 2; ++k)
            p[(std::size_t) k] = std::norm (a[(std::size_t) k]) / ((double) N * N / 4.0);
        return p;
    }

    int foldedBin (int harmonic, int cycles)
    {
        int bin = (int) (((long long) harmonic * cycles) % N);
        return bin > N / 2 ? N - bin : bin;
    }

    // Level of harmonic h relative to the fundamental, in dB.
    double harmonicDb (const std::vector<double>& p, int h, int cycles)
    {
        return 10.0 * std::log10 (std::max (p[(std::size_t) foldedBin (h, cycles)], 1.0e-30)
                                  / std::max (p[(std::size_t) foldedBin (1, cycles)], 1.0e-30));
    }

    // Energy that is NOT on a harmonic of the fundamental (= aliasing), relative to the fundamental.
    double aliasDb (const std::vector<double>& p, int cycles)
    {
        // Only true harmonics (below Nyquist) count as signal; anything that
        // folded back from above Nyquist is aliasing.
        std::vector<char> harmonic (p.size(), 0);
        for (int h = 1; h * cycles < N / 2; ++h)
            harmonic[(std::size_t) (h * cycles)] = 1;

        double alias = 0.0;
        for (int k = 2; k < N / 2; ++k)
            if (! harmonic[(std::size_t) k])
                alias += p[(std::size_t) k];

        return 10.0 * std::log10 (std::max (alias, 1.0e-30) / p[(std::size_t) foldedBin (1, cycles)]);
    }

    // Renders a bin-centred sine (`cycles` periods per N samples) and returns its spectrum.
    std::vector<double> renderSpectrum (const ColorParameters& params, int cycles, float amp, bool antiAliasing = true)
    {
        Rig rig (params, 48000.0, 1);
        rig.processor.setAntiAliasing (antiAliasing);
        Buffer b (1, 3 * N);
        for (int i = 0; i < b.samples(); ++i)
            b.data[0][(std::size_t) i] = amp * (float) std::sin (2.0 * pi * cycles * i / N);
        rig.run(b, 512);
        return powerSpectrum (b.data[0]);
    }

    double gainAt (const ColorParameters& p, double freq, double sr = 48000.0, float amp = 0.05f)
    {
        Rig rig (p, sr);
        Buffer b (2, (int) sr);
        fillSine (b, freq, sr, amp);
        rig.run (b, 512);
        return rms (b.data[0], (int) (sr * 0.5), (int) sr) * std::sqrt (2.0) / amp;
    }

    // Min / max relative deviation of the instantaneous frequency of a sine.
    struct Deviation { double low, high; int cycles; };

    Deviation measureDeviation (const std::vector<float>& y, double sr, double f0, int from, int to)
    {
        std::vector<double> crossings;
        for (int i = std::max (from, 1); i < to; ++i)
        {
            const float a = y[(std::size_t) i - 1], b = y[(std::size_t) i];
            if (a < 0.0f && b >= 0.0f)
                crossings.push_back ((i - 1) + (double) (-a) / (double) (b - a));
        }

        Deviation d { 1.0e9, -1.0e9, 0 };
        for (std::size_t k = 1; k < crossings.size(); ++k)
        {
            const double dev = (sr / (crossings[k] - crossings[k - 1])) / f0 - 1.0;
            d.low = std::min (d.low, dev);
            d.high = std::max (d.high, dev);
            ++d.cycles;
        }
        return d;
    }

    // ------------------------------------------------------------------ tests

    void testBitTransparentWhenOff()
    {
        for (int channels : { 1, 2 })
        {
            Rig rig (allOff(), 48000.0, channels);
            Buffer in (channels, 48000);
            fillNoise (in, 1, 0.8f);
            Buffer out = in;
            rig.run (out, 333);
            CHECK (in.data == out.data);
        }
    }

    void testHarmonicSignatures()
    {
        const int cycles = 171; // 1.0 kHz at 48 kHz
        double h2[3], h3[3];
        const char* names[3] = { "Tube", "Tape", "Console" };

        for (int t = 0; t < 3; ++t)
        {
            ColorParameters p = allOff();
            p.drive = 0.5f;
            p.type = (ColorType) t;
            const auto spec = renderSpectrum (p, cycles, 0.5f);
            h2[t] = harmonicDb (spec, 2, cycles);
            h3[t] = harmonicDb (spec, 3, cycles);
            std::printf ("  %-8s H2 %6.1f dB   H3 %6.1f dB   H5 %6.1f dB\n", names[t], h2[t], h3[t],
                         harmonicDb (spec, 5, cycles));
            CHECK (h3[t] > -45.0 || t == 0);
        }

        CHECK (h2[0] > -35.0);           // Tube: strong 2nd harmonic
        CHECK (h2[0] > h2[1] + 30.0);    // ... which symmetric curves lack
        CHECK (h2[1] < -70.0);           // Tape: odd harmonics only
        CHECK (h2[2] < -70.0);           // Console: odd harmonics only
        CHECK (h3[1] > -40.0 && h3[2] > -40.0);
        CHECK (std::abs (h3[1] - h3[2]) > 1.0); // the two odd curves are not the same sound

        // No harmonics without Drive.
        ColorParameters off = allOff();
        const auto clean = renderSpectrum (off, cycles, 0.5f);
        CHECK (harmonicDb (clean, 3, cycles) < -100.0);
    }

    void testAntiAliasing()
    {
        const int cycles = 1000; // 5.86 kHz: harmonics fold back into the audio band
        for (int t = 0; t < 3; ++t)
        {
            ColorParameters p = allOff();
            p.drive = 1.0f;
            p.type = (ColorType) t;

            const double naive = aliasDb (renderSpectrum (p, cycles, 0.5f, false), cycles);
            const double adaa = aliasDb (renderSpectrum (p, cycles, 0.5f, true), cycles);
            std::printf ("  type %d aliasing: naive %.1f dB, ADAA %.1f dB (%.1f dB better)\n", t, naive, adaa, naive - adaa);
            CHECK (adaa < naive - 3.0);
        }
    }

    void testLevelCompensation()
    {
        for (int t = 0; t < 3; ++t)
            for (float drive : { 0.1f, 0.3f, 0.6f, 1.0f })
            {
                ColorParameters p = allOff();
                p.drive = drive;
                p.type = (ColorType) t;
                Rig rig (p);
                Buffer b (2, 48000);
                fillSine (b, 997.0, 48000.0, 0.3f);
                rig.run (b, 512);
                const double ratio = rms (b.data[0], 24000, 48000) / (0.3 / std::sqrt (2.0));
                std::printf ("  type %d drive %.1f: level x%.2f\n", t, drive, ratio);
                CHECK_RANGE (ratio, 0.6, 1.6);
            }
    }

    void testTone()
    {
        ColorParameters p = allOff();

        p.tone = 1.0f;
        const double brightHigh = toDb (gainAt (p, 8000.0)), brightLow = toDb (gainAt (p, 100.0));
        p.tone = -1.0f;
        const double darkHigh = toDb (gainAt (p, 8000.0)), darkLow = toDb (gainAt (p, 100.0));
        std::printf ("  tone +1: 100 Hz %.1f dB, 8 kHz %.1f dB | tone -1: 100 Hz %.1f dB, 8 kHz %.1f dB\n",
                     brightLow, brightHigh, darkLow, darkHigh);

        CHECK_RANGE (brightHigh, 4.5, 6.5);
        CHECK_RANGE (brightLow, -6.5, -4.5);
        CHECK_RANGE (darkHigh, -6.5, -4.5);
        CHECK_RANGE (darkLow, 4.5, 6.5);

        p.tone = 0.5f;
        CHECK_RANGE (toDb (gainAt (p, 8000.0)), 2.0, 3.5); // proportional
    }

    void testChorusPitchModulation()
    {
        auto p = chorusParams (ChorusMode::classic);

        for (double sr : { 22050.0, 44100.0, 48000.0, 96000.0, 192000.0 })
        {
            ChorusRig rig (p, sr);
            Buffer b (2, (int) (4 * sr));
            fillSine (b, 1000.0, sr, 0.5f);
            rig.run (b);
            CHECK (allFinite (b));

            // Expected: +-2*pi*0.8 Hz*3.5 ms = +-1.76 %
            const auto d = measureDeviation (b.data[0], sr, 1000.0, (int) (0.3 * sr), (int) (4 * sr));
            CHECK_RANGE (d.high, 0.0135, 0.0215);
            CHECK_RANGE (d.low, -0.0215, -0.0135);
        }

        // Depth 0.5 = half the deviation.
        p.depth = 0.5f;
        ChorusRig half (p);
        Buffer c (2, 4 * 48000);
        fillSine (c, 1000.0, 48000.0, 0.5f);
        half.run (c);
        const auto h = measureDeviation (c.data[0], 48000.0, 1000.0, 14400, 4 * 48000);
        CHECK_RANGE (h.high, 0.0068, 0.0110);
    }

    void testChorusModesAndMix()
    {
        Buffer in (2, 48000);
        fillNoise (in, 12, 0.5f);

        Buffer results[3] = { in, in, in };
        for (int m = 0; m < 3; ++m)
        {
            ChorusRig rig (chorusParams ((ChorusMode) m));
            rig.run (results[m]);
            CHECK (allFinite (results[m]));
            const double level = rms (results[m].data[0], 4800, 48000);
            CHECK_RANGE (level, 0.1, 0.5); // wet level stays sensible (input RMS ~0.29)
        }
        CHECK (results[0].data != results[1].data && results[1].data != results[2].data);

        // Mix 0 with the chorus on = exactly the input.
        ChorusRig dry (chorusParams (ChorusMode::ensemble, 1.0f, 0.8f, 1.0f, 0.5f, 0.0f));
        Buffer d = in;
        dry.run (d);
        CHECK (d.data == in.data);

        // Amount 0 = exactly the input.
        ChorusRig off (chorusParams (ChorusMode::ensemble, 0.0f));
        Buffer o = in;
        off.run (o);
        CHECK (o.data == in.data);
    }

    void testChorusWidth()
    {
        auto channelDifference = [&] (float width)
        {
            ChorusRig rig (chorusParams (ChorusMode::classic, 1.0f, 2.0f, 1.0f, width));
            Buffer b (2, 2 * 48000);
            fillSine (b, 440.0, 48000.0, 0.5f); // identical in both channels
            rig.run (b);

            double diff = 0.0;
            for (int i = 24000; i < b.samples(); ++i)
            {
                const double d = (double) b.data[0][(std::size_t) i] - b.data[1][(std::size_t) i];
                diff += d * d;
            }
            return std::sqrt (diff / (b.samples() - 24000));
        };

        CHECK (channelDifference (0.0f) < 1.0e-6);   // in phase: the channels stay identical
        CHECK (channelDifference (1.0f) > 0.1);      // opposite phase: wide
        CHECK (channelDifference (0.5f) < channelDifference (1.0f));
    }

    void testMagicChorusKnob()
    {
        // One knob: 0 = off, bit-transparent; up = more modulation and a wide stereo image.
        Buffer in (2, 2 * 48000);
        fillSine (in, 440.0, 48000.0, 0.5f); // identical in both channels

        auto render = [&] (float knob)
        {
            ColorParameters p = allOff();
            p.magicChorus = knob;
            Rig rig (p);
            Buffer b = in;
            rig.run (b, 512);
            return b;
        };

        CHECK (render (0.0f).data == in.data);

        auto difference = [&] (const Buffer& b)
        {
            double sum = 0.0;
            for (int i = 24000; i < b.samples(); ++i)
            {
                const double d = (double) b.data[0][(std::size_t) i] - in.data[0][(std::size_t) i];
                sum += d * d;
            }
            return std::sqrt (sum / (b.samples() - 24000));
        };

        const Buffer low = render (0.25f), high = render (1.0f);
        CHECK (allFinite (low) && allFinite (high));
        CHECK (difference (low) > 0.01);
        CHECK (difference (high) > difference (low));

        // The ensemble spreads an identical L/R input across the stereo field.
        double width = 0.0;
        for (int i = 24000; i < high.samples(); ++i)
        {
            const double d = (double) high.data[0][(std::size_t) i] - high.data[1][(std::size_t) i];
            width += d * d;
        }
        CHECK (std::sqrt (width / (high.samples() - 24000)) > 0.02);
    }

    void testClickFreeSwitching()
    {
        // Chorus mode changes.
        {
            ChorusRig rig (chorusParams (ChorusMode::classic, 1.0f, 0.8f, 0.8f));
            Buffer b (2, 48000);
            fillSine (b, 1000.0, 48000.0, 0.5f);

            for (int start = 0, k = 0; start < b.samples(); start += 4000, ++k)
            {
                rig.chorus.setParams (chorusParams ((ChorusMode) (k % 3), 1.0f, 0.8f, 0.8f));
                rig.run (b, start, std::min (4000, b.samples() - start));
            }
            CHECK (allFinite (b));
            CHECK (maxStep (b.data[0], 2400, 48000) < 0.4);
        }

        // Saturation type changes.
        {
            ColorParameters p = allOff();
            p.drive = 0.6f;
            Rig rig (p);
            Buffer b (2, 48000);
            fillSine (b, 500.0, 48000.0, 0.5f);

            for (int start = 0, k = 0; start < b.samples(); start += 3000, ++k)
            {
                p.type = (ColorType) (k % 3);
                rig.processor.setParameters (p);
                rig.processor.process (b.view().getSubBlock (start, std::min (3000, b.samples() - start)));
            }
            CHECK (allFinite (b));
            CHECK (maxStep (b.data[0], 2400, 48000) < 0.25);
        }

        // Drive sweep: no step beyond what the saturated signal itself has.
        {
            ColorParameters p = allOff();
            Rig rig (p);
            Buffer b (2, 48000);
            fillSine (b, 300.0, 48000.0, 0.5f);
            for (int start = 0, k = 0; start < b.samples(); start += 480, ++k)
            {
                p.drive = (k % 20) / 19.0f;
                rig.processor.setParameters (p);
                rig.processor.process (b.view().getSubBlock (start, std::min (480, b.samples() - start)));
            }
            CHECK (allFinite (b));
            CHECK (maxStep (b.data[0], 100, 48000) < 0.2);
        }
    }

    void testBlockSizeIndependence()
    {
        ColorParameters p;
        p.drive = 0.7f; p.type = ColorType::tape; p.tone = 0.4f; p.magicChorus = 0.9f;

        Buffer in (2, 30000);
        fillNoise (in, 21, 0.5f);

        auto render = [&] (int blockSize)
        {
            Rig rig (p);
            Buffer out = in;
            rig.run (out, blockSize);
            return out;
        };

        const Buffer reference = render (64);
        CHECK (allFinite (reference));
        for (int bs : { 1, 7, 100, 512, 4096, 30000 })
            CHECK (render (bs).data == reference.data);
    }

    void testExtremesAndAutomation()
    {
        Rig rig (ColorParameters {});
        Buffer b (2, 96000);
        fillNoise (b, 3, 1.0f);

        std::mt19937 rng (17);
        std::uniform_real_distribution<float> u (0.0f, 1.0f);

        for (int start = 0; start < b.samples(); start += 64)
        {
            ColorParameters p;
            p.drive = -1.0f + 3.0f * u (rng);
            p.type = (ColorType) (int) (u (rng) * 2.99f);
            p.tone = -2.0f + 4.0f * u (rng);
            p.magicChorus = u (rng) < 0.3f ? 0.0f : -0.5f + 2.0f * u (rng);
            rig.processor.setParameters (p);
            rig.processor.process (b.view().getSubBlock (start, std::min (64, b.samples() - start)));
        }
        CHECK (allFinite (b));

        float peak = 0.0f;
        for (float s : b.data[0]) peak = std::max (peak, std::abs (s));
        CHECK (peak < 6.0f);

        ColorParameters bad;
        bad.drive = std::numeric_limits<float>::quiet_NaN();
        bad.tone = std::numeric_limits<float>::infinity();
        bad.magicChorus = -std::numeric_limits<float>::infinity();
        bad.type = (ColorType) 99;
        Rig nanRig (ColorParameters {});
        nanRig.processor.setParameters (bad);
        Buffer c (2, 8192);
        fillSine (c, 500.0, 48000.0, 0.5f);
        nanRig.run (c, 256);
        CHECK (allFinite (c));
        const auto q = nanRig.processor.getParameters();
        CHECK (std::isfinite (q.drive) && q.type == ColorType::tube);
    }

    void testMonoAndEdgeCases()
    {
        ColorParameters p;
        p.drive = 0.8f; p.tone = 0.5f; p.magicChorus = 1.0f;

        Rig mono (p, 48000.0, 1);
        Buffer m (1, 48000);
        fillSine (m, 300.0, 48000.0, 0.5f);
        mono.run (m, 256);
        CHECK (allFinite (m));

        // Silence stays silent (no DC from the biased tube curve).
        Rig silent (p, 48000.0, 1);
        Buffer s (1, 48000);
        silent.run (s, 512);
        CHECK (rms (s.data[0], 24000, 48000) < 1.0e-6);

        // More channels than prepared: extra channel untouched.
        Rig prepMono (p, 48000.0, 1);
        Buffer two (2, 1000);
        fillNoise (two, 8, 0.5f);
        const auto ch1 = two.data[1];
        prepMono.run (two, 256);
        CHECK (two.data[1] == ch1);

        // Empty block and 1-sample blocks.
        AudioBufferView empty;
        prepMono.processor.process (empty);
        Buffer one (2, 1);
        Rig r (p);
        for (int i = 0; i < 100; ++i)
            r.processor.process (one.view());
        CHECK (allFinite (one));

        // Re-prepare at another format.
        r.processor.prepare ({ 96000.0, 2048, 2 });
        Buffer big (2, 5000);
        fillSine (big, 100.0, 96000.0, 0.5f);
        r.run (big, 5000);
        CHECK (allFinite (big));
    }

    void testResetIsRepeatable()
    {
        ColorParameters p;
        p.drive = 1.0f; p.tone = -0.5f; p.magicChorus = 1.0f;

        Rig rig (p);
        Buffer in (2, 24000);
        fillNoise (in, 31, 0.4f);

        Buffer first = in;
        rig.run (first, 480);
        rig.processor.reset();
        Buffer second = in;
        rig.run (second, 480);
        CHECK (first.data == second.data);
    }

    void testNoAllocationInProcess()
    {
        ColorParameters p;
        Rig rig (p);
        Buffer b (2, 48000);
        fillNoise (b, 4, 0.5f);

        std::mt19937 rng (9);
        std::uniform_real_distribution<float> u (0.0f, 1.0f);

        allocationCount = 0;
        countAllocations = true;

        for (int start = 0; start + 1000 <= b.samples(); start += 1000)
        {
            ColorParameters q;
            q.drive = u (rng); q.type = (ColorType) (int) (u (rng) * 2.99f); q.tone = u (rng) * 2.0f - 1.0f;
            q.magicChorus = u (rng);
            rig.processor.setParameters (q);
            rig.processor.process (b.view().getSubBlock (start, 1000));
        }
        rig.processor.reset();

        countAllocations = false;
        CHECK (allocationCount.load() == 0);
    }

    void testPerformance()
    {
        ColorParameters p;
        p.drive = 0.7f; p.tone = 0.3f; p.magicChorus = 0.8f;

        Rig rig (p, 48000.0, 2, 512);
        Buffer b (2, 512);
        fillNoise (b, 8, 0.5f);

        const int seconds = 60;
        const auto t0 = std::chrono::steady_clock::now();
        for (int k = 0; k < seconds * 48000 / 512; ++k)
            rig.processor.process (b.view());
        const auto t1 = std::chrono::steady_clock::now();

        const double elapsed = std::chrono::duration<double> (t1 - t0).count();
        std::printf ("  perf: %d s of stereo 48 kHz in %.3f s (%.0fx real time, %.2f %% of one core)\n",
                     seconds, elapsed, seconds / elapsed, 100.0 * elapsed / seconds);
        CHECK (allFinite (b));
       #ifdef NDEBUG
        CHECK (elapsed < seconds * 0.05);
       #else
        CHECK (elapsed < seconds * 0.5);
       #endif
    }

    struct TestCase { const char* name; void (*fn)(); };
}

int main()
{
    const TestCase tests[] = {
        { "Off = bit-transparent (mono+stereo)",      testBitTransparentWhenOff },
        { "Saturation: harmonic signatures (FFT)",    testHarmonicSignatures },
        { "Saturation: anti-aliasing (ADAA vs naive)", testAntiAliasing },
        { "Saturation: level compensation",           testLevelCompensation },
        { "Tone: bipolar tilt",                       testTone },
        { "Chorus: measured pitch modulation",        testChorusPitchModulation },
        { "Chorus: modes, mix, amount",               testChorusModesAndMix },
        { "Chorus: width",                            testChorusWidth },
        { "Magic Chorus: one knob",                   testMagicChorusKnob },
        { "Click-free: mode / type / drive changes",  testClickFreeSwitching },
        { "Block size independence (bit-exact)",      testBlockSizeIndependence },
        { "Extremes, automation, NaN/Inf",            testExtremesAndAutomation },
        { "Mono and edge cases",                      testMonoAndEdgeCases },
        { "reset() is repeatable",                    testResetIsRepeatable },
        { "No allocation in process()",               testNoAllocationInProcess },
        { "Performance",                              testPerformance },
    };

    for (const auto& t : tests)
    {
        const int before = failures;
        std::printf ("[ RUN  ] %s\n", t.name);
        t.fn();
        std::printf ("[ %s ] %s\n", failures == before ? " OK " : "FAIL", t.name);
    }

    std::printf ("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
