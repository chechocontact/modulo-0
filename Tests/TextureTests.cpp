// Vintage Maker — Module 1 (TEXTURE) tests. Console program, no JUCE.
// Exit code 0 = all checks passed.

#include "DSP/Texture/TextureProcessor.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <random>
#include <vector>

// ---- allocation counter (proves process() never allocates) ----------------
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

// ---- harness ---------------------------------------------------------------
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

    // A parameter set with every effect off: the processor must be a no-op.
    TextureParameters allOff()
    {
        TextureParameters p;
        p.wowDepth = p.flutterDepth = p.vinylDrop = p.degrade = 0.0f;
        return p;
    }

    // Prepares, sets parameters and snaps smoothers (so tests start at the targets).
    struct Rig
    {
        explicit Rig (const TextureParameters& p, double sampleRate = 48000.0, int channels = 2, int maxBlock = 512)
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

        TextureProcessor processor;
    };

    // Relative deviation of the instantaneous frequency of a sine, measured
    // from the spacing of upward zero crossings. Returns {min, max} of f/f0 - 1.
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

    // RMS-based amplitude of a sine at `freq` through the processor.
    double gainAt (const TextureParameters& p, double freq, double sr = 48000.0, float amp = 0.1f)
    {
        Rig rig (p, sr);
        Buffer b (2, (int) sr);
        fillSine (b, freq, sr, amp);
        rig.run (b, 512);
        return rms (b.data[0], (int) (sr * 0.5), (int) sr) * std::sqrt (2.0) / amp;
    }

    // ------------------------------------------------------------------ tests

    void testDelayLine()
    {
        DelayLine line;
        line.prepare (64);

        std::vector<float> in (200);
        std::mt19937 rng (3);
        std::uniform_real_distribution<float> d (-1.0f, 1.0f);
        for (auto& s : in) s = d (rng);

        // Integer delays are exact; zero delay returns the sample just pushed.
        for (int delay : { 0, 1, 5, 40 })
        {
            line.reset();
            bool exact = true;
            for (int i = 0; i < 200; ++i)
            {
                line.push (in[(std::size_t) i]);
                const float expected = i - delay >= 0 ? in[(std::size_t) (i - delay)] : 0.0f;
                exact = exact && line.read ((float) delay) == expected;
            }
            CHECK (exact);
        }

        // Fractional delay on a slow sine is accurate.
        line.reset();
        double maxError = 0.0;
        for (int i = 0; i < 2000; ++i)
        {
            line.push ((float) std::sin (2.0 * pi * 0.01 * i));
            if (i > 100)
                maxError = std::max (maxError, std::abs (line.read (10.37f) - std::sin (2.0 * pi * 0.01 * (i - 10.37))));
        }
        CHECK (maxError < 1.0e-4);

        // Out-of-range delays are clamped, not read out of bounds.
        line.push (1.0f);
        CHECK (std::isfinite (line.read (-5.0f)) && std::isfinite (line.read (1.0e9f)));
    }

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

    void testSilenceStaysSilentWhenOff()
    {
        Rig rig (allOff());
        Buffer b (2, 48000);
        rig.run (b, 256);
        CHECK (b.data[0] == std::vector<float> (48000, 0.0f));
    }

    void testWowPitch()
    {
        TextureParameters p = allOff();
        p.wowDepth = 1.0f;
        p.wowRate = 1.0f;

        Rig rig (p);
        Buffer b (2, 5 * 48000);
        fillSine (b, 1000.0, 48000.0, 0.5f);
        rig.run (b, 512);

        // Expected: +-2*pi*rate*excursion = +-5.03 %
        const auto d = measureDeviation (b.data[0], 48000.0, 1000.0, 24000, 5 * 48000);
        CHECK_RANGE (d.high, 0.0455, 0.0545);
        CHECK_RANGE (d.low, -0.0545, -0.0455);

        // Half the depth, half the deviation.
        p.wowDepth = 0.5f;
        Rig half (p);
        Buffer c (2, 5 * 48000);
        fillSine (c, 1000.0, 48000.0, 0.5f);
        half.run (c, 512);
        const auto h = measureDeviation (c.data[0], 48000.0, 1000.0, 24000, 5 * 48000);
        CHECK_RANGE (h.high, 0.0228, 0.0272);

        // The two stereo channels move almost together (tape wow is correlated).
        const auto d1 = measureDeviation (b.data[1], 48000.0, 1000.0, 24000, 5 * 48000);
        CHECK_RANGE (d1.high, 0.0455, 0.0545);
    }

    void testFlutterPitch()
    {
        TextureParameters p = allOff();
        p.flutterDepth = 1.0f;
        p.flutterRate = 10.0f;

        Rig rig (p);
        Buffer b (2, 3 * 48000);
        fillSine (b, 1000.0, 48000.0, 0.5f);
        rig.run (b, 512);
        CHECK (allFinite (b));

        // Nominal +-2*pi*10*0.6ms = +-3.77 %; the fixed 30 % randomness adds some spread.
        const auto d = measureDeviation (b.data[0], 48000.0, 1000.0, 24000, 3 * 48000);
        CHECK_RANGE (d.high, 0.030, 0.050);
        CHECK_RANGE (d.low, -0.050, -0.030);

        // Half depth, about half the deviation.
        p.flutterDepth = 0.5f;
        Rig half (p);
        Buffer c (2, 3 * 48000);
        fillSine (c, 1000.0, 48000.0, 0.5f);
        half.run (c, 512);
        const auto h = measureDeviation (c.data[0], 48000.0, 1000.0, 24000, 3 * 48000);
        CHECK_RANGE (h.high, 0.015, 0.026);
    }

    void testVinylDrop()
    {
        TextureParameters p = allOff();
        p.vinylDrop = 1.0f;

        Rig rig (p);
        rig.processor.triggerDrop (0.4);

        Buffer b (2, 2 * 48000);
        fillSine (b, 1000.0, 48000.0, 0.5f);
        rig.run (b, 512);
        CHECK (allFinite (b));

        // Mid-drop (t = 0.2 s) the level dips ~35 %; before and after it does not.
        const double mid = rms (b.data[0], (int) (0.19 * 48000), (int) (0.21 * 48000)) / (0.5 / std::sqrt (2.0));
        const double after = rms (b.data[0], (int) (0.8 * 48000), (int) (1.0 * 48000)) / (0.5 / std::sqrt (2.0));
        CHECK_RANGE (mid, 0.58, 0.72);
        CHECK_RANGE (after, 0.97, 1.03);

        // Pitch dips and recovers during the drop.
        const auto d = measureDeviation (b.data[0], 48000.0, 1000.0, 400, (int) (0.45 * 48000));
        CHECK (d.low < -0.003);
        CHECK (d.high > 0.003);

        // Drop = 0 never triggers on its own (60 s at 48 kHz, silent input, no noise).
        TextureParameters q = allOff();
        Rig none (q);
        Buffer c (2, 60 * 48000);
        fillSine (c, 1000.0, 48000.0, 0.5f);
        Buffer ref = c;
        none.run (c, 4096);
        CHECK (c.data == ref.data);
    }

    void testDegradeKnob()
    {
        TextureParameters p = allOff();

        // Amount 0 is a true bypass.
        CHECK (std::abs (gainAt (p, 10000.0) - 1.0) < 1.0e-6);

        // Amount 1: band narrows to ~300 Hz .. 1.9 kHz.
        p.degrade = 1.0f;
        const double low = gainAt (p, 100.0), mid = gainAt (p, 800.0), high = gainAt (p, 10000.0);
        std::printf ("  degrade 1.0: 100 Hz %.1f dB, 800 Hz %.1f dB, 10 kHz %.1f dB\n",
                     toDb (low), toDb (mid), toDb (high));
        CHECK (toDb (high) < -25.0);
        CHECK (toDb (low) < -12.0);
        CHECK (toDb (mid) > -6.0);

        // More knob = more degraded, monotonically (checked on the highs).
        double previous = 2.0;
        for (float d : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
        {
            p.degrade = d;
            const double g = gainAt (p, 8000.0);
            CHECK (g < previous + 1.0e-6);
            previous = g;
        }
    }

    void testDegradeSmoothing()
    {
        // Sparse clicks over a quiet bed: the knob must flatten the peaks
        // (partly by band-limiting, partly by transient softening).
        auto clickPeak = [] (float degrade)
        {
            TextureParameters p = allOff();
            p.degrade = degrade;

            Rig rig (p);
            Buffer b (2, 48000);
            fillNoise (b, 5, 0.02f);
            for (int i = 4800; i < 48000; i += 6000)
                b.data[0][(std::size_t) i] += 0.8f;

            rig.run (b, 512);
            float peak = 0.0f;
            for (int i = 4800; i < 48000; ++i)
                peak = std::max (peak, std::abs (b.data[0][(std::size_t) i]));
            return (double) peak;
        };

        const double clean = clickPeak (0.0f), half = clickPeak (0.5f), full = clickPeak (1.0f);
        std::printf ("  click peak: degrade 0 = %.3f, 0.5 = %.3f, 1 = %.3f\n", clean, half, full);
        CHECK (half < clean);
        CHECK (full < half);
        CHECK (full < clean * 0.5);

        // Steady tones in the passband keep most of their level.
        TextureParameters p = allOff();
        p.degrade = 1.0f;
        CHECK (gainAt (p, 800.0) > 0.5);
    }

    void testBlockSizeIndependence()
    {
        TextureParameters p;
        p.wowDepth = 0.8f; p.wowRate = 1.3f; p.flutterDepth = 0.7f; p.flutterRate = 9.0f;
        p.vinylDrop = 0.8f; p.degrade = 0.7f;

        Buffer in (2, 30000);
        fillNoise (in, 21, 0.5f);

        auto render = [&] (int blockSize)
        {
            Rig rig (p);
            rig.processor.triggerDrop (0.3);
            Buffer out = in;
            rig.run (out, blockSize);
            return out;
        };

        const Buffer reference = render (64);
        CHECK (allFinite (reference));

        for (int bs : { 1, 7, 100, 512, 4096, 30000 })
            CHECK (render (bs).data == reference.data); // bit-identical
    }

    void testSampleRates()
    {
        TextureParameters p = allOff();
        p.wowDepth = 1.0f;
        p.wowRate = 1.0f;
        p.degrade = 0.5f;

        for (double sr : { 22050.0, 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
        {
            Rig rig (p, sr, 2, 1024);
            Buffer b (2, (int) (4 * sr));
            fillSine (b, 1000.0, sr, 0.5f);
            rig.run (b, 1024);
            CHECK (allFinite (b));

            // Wow depth in percent is independent of the sample rate.
            const auto d = measureDeviation (b.data[0], sr, 1000.0, (int) (0.5 * sr), (int) (4 * sr));
            CHECK_RANGE (d.high, 0.0400, 0.0570);
        }
    }

    void testExtremesAndAutomation()
    {
        Rig rig (TextureParameters {});
        Buffer b (2, 96000);
        fillNoise (b, 3, 1.0f);

        std::mt19937 rng (17);
        std::uniform_real_distribution<float> u (0.0f, 1.0f);

        for (int start = 0; start < b.samples(); start += 64)
        {
            TextureParameters p;
            p.wowDepth = -1.0f + 3.0f * u (rng);          // out of range on purpose
            p.wowRate = -5.0f + 20.0f * u (rng);
            p.flutterDepth = -1.0f + 3.0f * u (rng);
            p.flutterRate = u (rng) * 40.0f;
            p.vinylDrop = u (rng);
            p.degrade = u (rng) < 0.3f ? 0.0f : u (rng);
            rig.processor.setParameters (p);
            rig.processor.process (b.view().getSubBlock (start, std::min (64, b.samples() - start)));
        }
        CHECK (allFinite (b));

        float peak = 0.0f;
        for (float s : b.data[0]) peak = std::max (peak, std::abs (s));
        CHECK (peak < 4.0f);

        // NaN / Inf parameters keep the previous value.
        TextureParameters bad;
        bad.wowDepth = std::numeric_limits<float>::quiet_NaN();
        bad.flutterRate = std::numeric_limits<float>::infinity();
        bad.degrade = -std::numeric_limits<float>::infinity();
        Rig nanRig (TextureParameters {});
        nanRig.processor.setParameters (bad);
        Buffer c (2, 8192);
        fillSine (c, 500.0, 48000.0, 0.5f);
        nanRig.run (c, 256);
        CHECK (allFinite (c));
        CHECK (std::isfinite (nanRig.processor.getParameters().wowDepth));
    }

    void testMonoAndEdgeCases()
    {
        TextureParameters p;
        p.vinylDrop = 1.0f;

        Rig mono (p, 48000.0, 1);
        Buffer m (1, 48000);
        fillSine (m, 300.0, 48000.0, 0.5f);
        mono.run (m, 256);
        CHECK (allFinite (m));

        // Block with more channels than prepared: extra channel left untouched.
        Rig prepMono (p, 48000.0, 1);
        Buffer two (2, 1000);
        fillNoise (two, 8, 0.5f);
        const auto ch1 = two.data[1];
        prepMono.run (two, 256);
        CHECK (two.data[1] == ch1);

        // Empty block, single-sample blocks.
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
        TextureParameters p;
        p.vinylDrop = 1.0f; p.degrade = 1.0f;

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
        TextureParameters p;
        p.vinylDrop = 1.0f; p.degrade = 0.8f;
        Rig rig (p);
        rig.processor.triggerDrop (0.2);

        Buffer b (2, 48000);
        fillNoise (b, 4, 0.5f);

        std::mt19937 rng (9);
        std::uniform_real_distribution<float> u (0.0f, 1.0f);

        allocationCount = 0;
        countAllocations = true;

        for (int start = 0; start + 1000 <= b.samples(); start += 1000)
        {
            TextureParameters q;
            q.wowDepth = u (rng); q.flutterDepth = u (rng); q.vinylDrop = u (rng);
            q.degrade = u (rng);
            rig.processor.setParameters (q);
            rig.processor.process (b.view().getSubBlock (start, 1000));
        }
        rig.processor.reset();

        countAllocations = false;
        CHECK (allocationCount.load() == 0);
    }

    void testPerformance()
    {
        TextureParameters p;
        p.wowDepth = 0.6f; p.flutterDepth = 0.5f; p.vinylDrop = 0.5f; p.degrade = 0.7f;

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
        { "DelayLine",                          testDelayLine },
        { "Off = bit-transparent (mono+stereo)", testBitTransparentWhenOff },
        { "Off + silence = silence",            testSilenceStaysSilentWhenOff },
        { "Wow: measured pitch deviation",      testWowPitch },
        { "Flutter: measured pitch deviation",  testFlutterPitch },
        { "Vinyl: drop",                        testVinylDrop },
        { "Degrade: one-knob band-limiting",        testDegradeKnob },
        { "Degrade: softened peaks",                testDegradeSmoothing },
        { "Block size independence (bit-exact)", testBlockSizeIndependence },
        { "Sample rates 22.05k-192k",           testSampleRates },
        { "Extremes, automation, NaN/Inf",      testExtremesAndAutomation },
        { "Mono and edge cases",                testMonoAndEdgeCases },
        { "reset() is repeatable",              testResetIsRepeatable },
        { "No allocation in process()",         testNoAllocationInProcess },
        { "Performance",                        testPerformance },
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
