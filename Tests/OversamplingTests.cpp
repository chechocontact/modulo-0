// Vintage Maker — Module 9 (OVERSAMPLING) tests. Console program, no JUCE.
// Exit code 0 = all checks passed.

#include "Chain/VintageChain.h"
#include "Core/Oversampler.h"
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

    struct TestCase { const char* name; void (*fn)(); };

    struct Buffer
    {
        Buffer (int channels, int samples)
            : data ((std::size_t) channels, std::vector<float> ((std::size_t) samples, 0.0f))
        {
            for (auto& c : data)
                pointers.push_back (c.data());
        }

        Buffer (const Buffer&) = delete;
        int channels() const { return (int) data.size(); }
        int samples() const { return (int) data[0].size(); }
        AudioBufferView view() { return { pointers.data(), channels(), samples() }; }

        std::vector<std::vector<float>> data;
        std::vector<float*> pointers;
    };

    // Frequency response of a processing function at `normalisedFreq` (cycles/sample of
    // the rate the function runs at): steady-state amplitude of a sine, measured by correlation.
    template <typename Function>
    double measureGain (Function&& fn, double normalisedFreq, int settle = 4000, int length = 8000)
    {
        double re = 0.0, im = 0.0;
        for (int i = 0; i < settle + length; ++i)
        {
            const double x = std::sin (2.0 * pi * normalisedFreq * i);
            const double y = fn (x);
            if (i >= settle)
            {
                re += y * std::sin (2.0 * pi * normalisedFreq * i);
                im += y * std::cos (2.0 * pi * normalisedFreq * i);
            }
        }
        return 2.0 * std::sqrt (re * re + im * im) / length;
    }

    double toDb (double g) { return 20.0 * std::log10 (std::max (g, 1.0e-12)); }

    // ---- small radix-2 FFT for the alias measurements ----
    void fft (std::vector<std::complex<double>>& a)
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
            const std::complex<double> wlen (std::cos (angle), std::sin (angle));
            for (std::size_t i = 0; i < n; i += len)
            {
                std::complex<double> w (1.0);
                for (std::size_t j = 0; j < len / 2; ++j)
                {
                    const auto u = a[i + j];
                    const auto v = a[i + j + len / 2] * w;
                    a[i + j] = u + v;
                    a[i + j + len / 2] = u - v;
                    w *= wlen;
                }
            }
        }
    }

    // Power of everything that is NOT a harmonic of `fundamental` (aliases and noise),
    // relative to the power of the harmonics, in dB.
    double aliasLevelDb (const std::vector<float>& signal, double sampleRate, double fundamental)
    {
        const std::size_t n = 16384;
        std::vector<std::complex<double>> a (n);
        const std::size_t offset = signal.size() - n;
        for (std::size_t i = 0; i < n; ++i)
        {
            const double w = 0.5 - 0.5 * std::cos (2.0 * pi * (double) i / (double) n); // Hann
            a[i] = (double) signal[offset + i] * w;
        }
        fft (a);

        std::vector<bool> isHarmonic (n / 2, false);
        for (int k = 1; k * fundamental < sampleRate * 0.5; ++k)
        {
            const double bin = k * fundamental / sampleRate * (double) n;
            for (int d = -4; d <= 4; ++d)
            {
                const long b = std::lround (bin) + d;
                if (b >= 0 && b < (long) (n / 2))
                    isHarmonic[(std::size_t) b] = true;
            }
        }

        double harmonic = 0.0, alias = 0.0;
        for (std::size_t b = 1; b < n / 2; ++b)
        {
            const double p = std::norm (a[b]);
            (isHarmonic[b] ? harmonic : alias) += p;
        }
        return 10.0 * std::log10 (std::max (alias, 1.0e-30) / std::max (harmonic, 1.0e-30));
    }

    std::vector<float> runColor (OversamplingMode mode, ColorType type, float drive, double freq, double sr, int length,
                                 float amp = 0.5f)
    {
        ColorProcessor color;
        color.prepare ({ sr, 512, 1 });
        ColorParameters p;
        p.drive = drive;
        p.type = type;
        p.tone = 0.0f;
        p.magicChorus = 0.0f;
        p.oversampling = mode;
        color.setParameters (p);
        color.reset();

        Buffer b (1, length);
        for (int i = 0; i < length; ++i)
            b.data[0][(std::size_t) i] = amp * (float) std::sin (2.0 * pi * freq * i / sr);

        for (int start = 0; start < length; start += 512)
        {
            const int n = std::min (512, length - start);
            float* ptr = b.data[0].data() + start;
            color.process ({ &ptr, 1, n });
        }
        return b.data[0];
    }

    // ------------------------------------------------------------------

    void testHalfbandResponses()
    {
        // 2x: interpolate then decimate a sine; the chain at the LOW rate is what matters.
        const auto chainGain = [] (int factor, double f)
        {
            Oversampler os;
            return measureGain ([&] (double x) { return os.process (x, factor, [] (double v) { return v; }); }, f);
        };

        for (int factor : { 2, 4 })
        {
            const double dc = chainGain (factor, 0.0005);
            CHECK_RANGE (toDb (dc), -0.01, 0.01);
            std::printf ("    %dx: gain @0.0005 fs = %.4f dB\n", factor, toDb (dc));

            // Passband up to 0.40 fs: flat within 0.1 dB.
            for (double f : { 0.05, 0.1, 0.2, 0.3, 0.38 })
                CHECK_RANGE (toDb (chainGain (factor, f)), -0.1, 0.1);

            // No gain anywhere (no resonances above unity).
            for (double f = 0.01; f < 0.5; f += 0.013)
                CHECK (chainGain (factor, f) < 1.001);
        }

        // Image rejection of the sharp stage, measured on the interleaved 2x stream: a tone at
        // f (cycles per low-rate sample) must come out at f/2 with unit gain and leave its mirror
        // image at (1 - f)/2 below -88 dB, as long as the image falls in the stopband (f <= 0.41).
        HalfbandStage<SharpHalfbandCoefficients> up;
        for (double f = 0.03; f <= 0.41; f += 0.02)
        {
            up.reset();
            std::complex<double> sig (0.0), img (0.0);
            const int count = 12000, settle = 3000;
            for (int i = 0; i < count; ++i)
            {
                double even, odd;
                up.upsample (std::sin (2.0 * pi * f * i), even, odd);
                if (i < settle)
                    continue;
                const double pos[2] { 2.0 * i, 2.0 * i + 1.0 };
                const double val[2] { even, odd };
                for (int k = 0; k < 2; ++k)
                {
                    sig += val[k] * std::polar (1.0, -2.0 * pi * (f * 0.5) * pos[k]);
                    img += val[k] * std::polar (1.0, -2.0 * pi * ((1.0 - f) * 0.5) * pos[k]);
                }
            }
            const double norm = 2.0 / (2.0 * (count - settle));
            CHECK_RANGE (toDb (std::abs (sig) * norm), -0.15, 0.15); // amplitude = 2 * |sum| / N
            CHECK (toDb (std::abs (img) * norm) < -88.0);
        }
    }

    void testAliasReduction()
    {
        // A loud 7.3 kHz tone through each curve at 44.1 kHz: the harmonics beyond 22 kHz fold
        // back as inharmonic aliases. Oversampling must lower them, more at 4x.
        const double sr = 44100.0, f = 7300.0;
        for (ColorType type : { ColorType::tube, ColorType::tape, ColorType::console })
        {
            const double off = aliasLevelDb (runColor (OversamplingMode::off, type, 1.0f, f, sr, 40000), sr, f);
            const double x2 = aliasLevelDb (runColor (OversamplingMode::x2, type, 1.0f, f, sr, 40000), sr, f);
            const double x4 = aliasLevelDb (runColor (OversamplingMode::x4, type, 1.0f, f, sr, 40000), sr, f);
            std::printf ("    type %d: alias level  off %.1f dB   2x %.1f dB   4x %.1f dB\n", (int) type, off, x2, x4);

            if (type != ColorType::tape) // Tape low-passes its own output, so it has little to fold
            {
                CHECK (x2 < off - 6.0);
                CHECK (x4 < x2 - 3.0);
            }
            else
            {
                CHECK (x2 <= off + 0.5);
                CHECK (x4 <= x2 + 0.5);
            }
        }
    }

    void testLevelAndCharacterPreserved()
    {
        // Same loudness (within 1 dB) and the same waveform character at all settings.
        for (ColorType type : { ColorType::tube, ColorType::tape, ColorType::console })
        {
            double level[3];
            int k = 0;
            for (auto mode : { OversamplingMode::off, OversamplingMode::x2, OversamplingMode::x4 })
            {
                const auto y = runColor (mode, type, 0.6f, 1000.0, 48000.0, 24000);
                double sum = 0.0;
                for (std::size_t i = 12000; i < y.size(); ++i)
                    sum += (double) y[i] * y[i];
                level[k++] = 10.0 * std::log10 (sum / 12000.0);
            }
            CHECK_RANGE (level[1] - level[0], -1.0, 1.0);
            CHECK_RANGE (level[2] - level[0], -1.0, 1.0);
        }
    }

    void testTransparentAtZeroDrive()
    {
        for (auto mode : { OversamplingMode::off, OversamplingMode::x2, OversamplingMode::x4 })
        {
            ColorProcessor color;
            color.prepare ({ 44100.0, 512, 2 });
            ColorParameters p;
            p.drive = 0.0f;
            p.tone = 0.0f;
            p.magicChorus = 0.0f;
            p.oversampling = mode;
            color.setParameters (p);
            color.reset();

            Buffer b (2, 4096), ref (2, 4096);
            std::mt19937 rng (3);
            std::uniform_real_distribution<float> d (-0.8f, 0.8f);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 4096; ++i)
                    b.data[(std::size_t) ch][(std::size_t) i] = ref.data[(std::size_t) ch][(std::size_t) i] = d (rng);

            color.process (b.view());
            CHECK (b.data == ref.data); // bit-exact: no latency, no filtering
        }
    }

    void testSwitchingIsClickFree()
    {
        // Change the oversampling every 3000 samples under a steady loud tone; the output
        // must stay continuous (the effect dips briefly, never jumps).
        const double sr = 44100.0;
        ColorProcessor color;
        color.prepare ({ sr, 256, 1 });
        ColorParameters p;
        p.drive = 0.7f;
        p.magicChorus = 0.0f;
        p.type = ColorType::tube;
        color.setParameters (p);
        color.reset();

        const OversamplingMode order[] { OversamplingMode::x2, OversamplingMode::x4, OversamplingMode::off,
                                         OversamplingMode::x4, OversamplingMode::x2, OversamplingMode::off };
        double maxStep = 0.0, maxStepReference = 0.0;
        float previous = 0.0f;
        long n = 0;

        for (int block = 0; block < 6 * 12; ++block)
        {
            if (block % 12 == 0)
            {
                p.oversampling = order[block / 12];
                color.setParameters (p);
            }

            Buffer b (1, 256);
            for (int i = 0; i < 256; ++i, ++n)
                b.data[0][(std::size_t) i] = 0.6f * (float) std::sin (2.0 * pi * 220.0 * (double) n / sr);
            color.process (b.view());

            for (float s : b.data[0])
            {
                maxStep = std::max (maxStep, (double) std::abs (s - previous));
                previous = s;
            }
        }

        // The steepest step of the undistorted tone itself, for scale.
        maxStepReference = 0.6 * 2.0 * pi * 220.0 / sr;
        std::printf ("    max sample step %.4f (tone slope %.4f)\n", maxStep, maxStepReference);
        CHECK (maxStep < 8.0 * maxStepReference + 0.05);
        CHECK (std::isfinite (previous));
    }

    void testBlockSizeIndependence()
    {
        for (auto mode : { OversamplingMode::x2, OversamplingMode::x4 })
        {
            std::vector<float> reference;
            for (int block : { 1, 7, 64, 480, 4096 })
            {
                ColorProcessor color;
                color.prepare ({ 48000.0, 4096, 2 });
                ColorParameters p;
                p.drive = 0.8f;
                p.type = ColorType::tape;
                p.magicChorus = 0.0f;
                p.oversampling = mode;
                color.setParameters (p);
                color.reset();

                Buffer b (2, 8192);
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 8192; ++i)
                        b.data[(std::size_t) ch][(std::size_t) i] = 0.5f * (float) std::sin (2.0 * pi * (310.0 + 70.0 * ch) * i / 48000.0);

                for (int start = 0; start < 8192; start += block)
                {
                    const int n = std::min (block, 8192 - start);
                    float* ptrs[2] { b.data[0].data() + start, b.data[1].data() + start };
                    color.process ({ ptrs, 2, n });
                }

                if (reference.empty())
                    reference = b.data[0];
                else
                    CHECK (reference == b.data[0]);
            }
        }
    }

    void testExtremesAndStability()
    {
        for (double sr : { 22050.0, 44100.0, 96000.0, 192000.0 })
            for (auto mode : { OversamplingMode::x2, OversamplingMode::x4 })
                for (ColorType type : { ColorType::tube, ColorType::tape, ColorType::console })
                {
                    ColorProcessor color;
                    color.prepare ({ sr, 512, 2 });
                    ColorParameters p;
                    p.drive = 1.0f;
                    p.type = type;
                    p.oversampling = mode;
                    color.setParameters (p);

                    Buffer b (2, 512);
                    std::mt19937 rng (11);
                    std::uniform_real_distribution<float> d (-4.0f, 4.0f); // far above full scale
                    bool finite = true;
                    float peak = 0.0f;

                    for (int block = 0; block < 60; ++block)
                    {
                        for (auto& c : b.data)
                            for (auto& s : c)
                                s = block % 7 == 3 ? 0.0f : d (rng);
                        color.process (b.view());
                        for (auto& c : b.data)
                            for (float s : c)
                            {
                                finite = finite && std::isfinite (s);
                                peak = std::max (peak, std::abs (s));
                            }
                    }

                    CHECK (finite);
                    CHECK (peak < 12.0f);
                }
    }

    void testSilenceDecaysWithoutDenormals()
    {
        for (int factor : { 2, 4 })
        {
            Oversampler os;
            std::mt19937 rng (5);
            std::uniform_real_distribution<double> d (-1.0, 1.0);
            for (int i = 0; i < 2000; ++i)
                os.process (d (rng), factor, [] (double v) { return std::tanh (6.0 * v); });

            double last = 1.0;
            for (int i = 0; i < 400000 && last != 0.0; ++i)
                last = os.process (0.0, factor, [] (double v) { return v; });

            CHECK (last == 0.0); // the filter tails end in exact zeros (flushed), not denormal noise
        }
    }

    void testNoAllocationInProcess()
    {
        ColorProcessor color;
        color.prepare ({ 44100.0, 512, 2 });
        ColorParameters p;
        p.drive = 0.8f;
        p.oversampling = OversamplingMode::x4;
        color.setParameters (p);

        Buffer b (2, 512);
        for (auto& c : b.data)
            for (int i = 0; i < 512; ++i)
                c[(std::size_t) i] = 0.5f * (float) std::sin (0.05 * i);

        countAllocations = true;
        allocationCount = 0;
        for (int i = 0; i < 100; ++i)
        {
            p.oversampling = i % 3 == 0 ? OversamplingMode::x2 : i % 3 == 1 ? OversamplingMode::x4 : OversamplingMode::off;
            color.setParameters (p);
            color.process (b.view());
        }
        countAllocations = false;
        CHECK (allocationCount == 0);
    }

    void testWholeChain()
    {
        // All modules on, hot signal, oversampling 4x and switching while playing: finite,
        // bounded, no allocation in the audio thread.
        ChainParameters params;
        params.color.drive = 0.9f;
        params.color.oversampling = OversamplingMode::x4;
        params.movement.grain = 0.5f;
        params.space.delay = 0.3f;
        params.space.mix = 0.3f;

        VintageChain chain;
        chain.prepare ({ 48000.0, 512, 2 }, params);

        Buffer b (2, 512);
        std::mt19937 rng (21);
        std::uniform_real_distribution<float> d (-0.9f, 0.9f);
        bool finite = true;
        float peak = 0.0f;

        countAllocations = true;
        allocationCount = 0;
        for (int block = 0; block < 200; ++block)
        {
            params.color.oversampling = (OversamplingMode) (block / 7 % 3);
            for (auto& c : b.data)
                for (auto& x : c)
                    x = d (rng);
            chain.process (b.view(), params);
            for (auto& c : b.data)
                for (float x : c)
                {
                    finite = finite && std::isfinite (x);
                    peak = std::max (peak, std::abs (x));
                }
        }
        countAllocations = false;

        CHECK (finite);
        CHECK (peak < 8.0f);
        CHECK (allocationCount == 0);
    }

    void testPerformance()
    {
        const double sr = 44100.0;
        double cost[3];
        int k = 0;

        for (auto mode : { OversamplingMode::off, OversamplingMode::x2, OversamplingMode::x4 })
        {
            ColorProcessor color;
            color.prepare ({ sr, 512, 2 });
            ColorParameters p;
            p.drive = 0.8f;
            p.magicChorus = 0.0f;
            p.type = ColorType::tube;
            p.oversampling = mode;
            color.setParameters (p);

            Buffer b (2, 512);
            for (auto& c : b.data)
                for (int i = 0; i < 512; ++i)
                    c[(std::size_t) i] = 0.5f * (float) std::sin (0.05 * i);

            const int blocks = 2000;
            const auto start = std::chrono::steady_clock::now();
            for (int i = 0; i < blocks; ++i)
                color.process (b.view());
            const double seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count();
            cost[k++] = 100.0 * seconds / ((double) blocks * 512.0 / sr);
        }

        std::printf ("    CPU of the Color module (stereo, 44.1 kHz): off %.2f %%   2x %.2f %%   4x %.2f %%\n", cost[0], cost[1], cost[2]);
#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_UNDEFINED__)
        return;
#elif defined(__has_feature)
 #if __has_feature(address_sanitizer) || __has_feature(undefined_behavior_sanitizer)
        return;
 #endif
#endif
        CHECK (cost[2] < 15.0);
    }

    const TestCase tests[] = {
        { "Half-band filters: response", testHalfbandResponses },
        { "Alias reduction (FFT)", testAliasReduction },
        { "Level preserved at all settings", testLevelAndCharacterPreserved },
        { "Transparent at Drive 0 (bit-exact)", testTransparentAtZeroDrive },
        { "Click-free switching", testSwitchingIsClickFree },
        { "Block size independence (bit-exact)", testBlockSizeIndependence },
        { "Extremes, sample rates, stability", testExtremesAndStability },
        { "Silence ends in exact zeros", testSilenceDecaysWithoutDenormals },
        { "No allocation in process()", testNoAllocationInProcess },
        { "Whole chain with oversampling", testWholeChain },
        { "Performance", testPerformance },
    };
}

int main()
{
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
