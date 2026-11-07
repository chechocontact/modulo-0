// Vintage Maker — Module 3 (MOVEMENT) tests. Console program, no JUCE.
// Exit code 0 = all checks passed.

#include "DSP/Movement/MovementProcessor.h"
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

    double maxStep (const std::vector<float>& v, int from, int to)
    {
        double m = 0.0;
        for (int i = std::max (from, 1); i < to; ++i)
            m = std::max (m, (double) std::abs (v[(std::size_t) i] - v[(std::size_t) i - 1]));
        return m;
    }

    MovementParameters allOff()
    {
        MovementParameters p;
        p.grain = 0.0f;
        p.tremolo = 0.0f;
        p.feedback = 0.0f;
        return p;
    }

    struct Rig
    {
        explicit Rig (const MovementParameters& p, double sampleRate = 48000.0, int channels = 2, int maxBlock = 512)
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

        MovementProcessor processor;
    };

    MovementParameters make (float grain, float feedback, float tremolo, float rate, MovementStyle style)
    {
        MovementParameters p;
        p.grain = grain; p.feedback = feedback; p.tremolo = tremolo; p.rateHz = rate; p.style = style;
        return p;
    }

    // ------------------------------------------------------------------ tests

    void testBitTransparentWhenOff()
    {
        for (int channels : { 1, 2 })
        {
            Buffer in (channels, 48000);
            fillNoise (in, 3, 0.5f);

            Buffer out = in;
            Rig rig (allOff(), 48000.0, channels);
            rig.run (out, 256);
            CHECK (out.data == in.data);
        }
    }

    void testGrainKnob()
    {
        Buffer in (2, 2 * 48000);
        fillNoise (in, 5, 0.4f);

        auto difference = [&] (float grain, MovementStyle style)
        {
            // Capture a second of sound first, so that Frozen has something to hold.
            Rig rig (make (grain, 0.0f, 0.0f, 4.0f, MovementStyle::cloud));
            Buffer warm = in;
            rig.run (warm, 512);
            rig.processor.setParameters (make (grain, 0.0f, 0.0f, 4.0f, style));

            Buffer b = in;
            rig.run (b, 512);
            CHECK (allFinite (b));

            double sum = 0.0;
            for (int i = 24000; i < b.samples(); ++i)
            {
                const double d = (double) b.data[0][(std::size_t) i] - in.data[0][(std::size_t) i];
                sum += d * d;
            }
            return std::sqrt (sum / (b.samples() - 24000));
        };

        for (auto style : { MovementStyle::frozen, MovementStyle::drift, MovementStyle::cloud })
        {
            const double low = difference (0.25f, style), high = difference (1.0f, style);
            CHECK (low > 0.02);
            CHECK (high > low);
        }
    }

    void testLevelStaysSensible()
    {
        // Overlapping grains are power-normalised: the wet level stays near the input level.
        // Capture 3 s of noise in Cloud first, so Frozen has something to hold.
        for (auto style : { MovementStyle::frozen, MovementStyle::drift, MovementStyle::cloud })
        {
            Rig rig (make (1.0f, 0.0f, 0.0f, 4.0f, MovementStyle::cloud));
            Buffer warm (2, 3 * 48000);
            fillNoise (warm, 8, 0.5f);
            rig.run (warm, 512);

            rig.processor.setParameters (make (1.0f, 0.0f, 0.0f, 4.0f, style));
            Buffer b (2, 3 * 48000);
            fillNoise (b, 9, 0.5f);
            const double inLevel = rms (b.data[0], 48000, 3 * 48000);
            rig.run (b, 512);
            CHECK_RANGE (rms (b.data[0], 48000, 3 * 48000) / inLevel, 0.6, 1.5);
        }
    }

    void testStyles()
    {
        Buffer in (2, 3 * 48000);
        fillNoise (in, 21, 0.4f);

        Buffer r[3] = { in, in, in };
        int k = 0;
        for (auto style : { MovementStyle::frozen, MovementStyle::drift, MovementStyle::cloud })
        {
            Rig rig (make (1.0f, 0.0f, 0.0f, 4.0f, style));
            rig.run (r[k++], 512);
        }
        CHECK (r[0].data != r[1].data && r[1].data != r[2].data && r[0].data != r[2].data);

        // Cloud is wide: identical L/R input comes out decorrelated.
        Buffer mono (2, 2 * 48000);
        fillSine (mono, 440.0, 48000.0, 0.5f);
        Rig rig (make (1.0f, 0.0f, 0.0f, 4.0f, MovementStyle::cloud));
        rig.run (mono, 512);
        double diff = 0.0;
        for (int i = 24000; i < mono.samples(); ++i)
        {
            const double d = (double) mono.data[0][(std::size_t) i] - mono.data[1][(std::size_t) i];
            diff += d * d;
        }
        CHECK (std::sqrt (diff / (mono.samples() - 24000)) > 0.02);
    }

    void testFrozenHoldsAfterInputStops()
    {
        // Play a tone, switch to Frozen, then feed silence: grains keep replaying the capture.
        const double sr = 48000.0;
        Rig rig (make (1.0f, 0.0f, 0.0f, 4.0f, MovementStyle::cloud));

        Buffer tone (2, (int) sr);
        fillSine (tone, 500.0, sr, 0.5f);
        rig.run (tone, 512);

        rig.processor.setParameters (make (1.0f, 0.0f, 0.0f, 4.0f, MovementStyle::frozen));
        Buffer silence (2, 3 * (int) sr);
        rig.run (silence, 512);
        CHECK (allFinite (silence));
        CHECK (rms (silence.data[0], 2 * (int) sr, 3 * (int) sr) > 0.05);

        // Cloud, by contrast, runs out once the 2 s capture is empty.
        Rig cloud (make (1.0f, 0.0f, 0.0f, 4.0f, MovementStyle::cloud));
        Buffer t2 (2, (int) sr);
        fillSine (t2, 500.0, sr, 0.5f);
        cloud.run (t2, 512);
        Buffer s2 (2, 4 * (int) sr);
        cloud.run (s2, 512);
        CHECK (rms (s2.data[0], 3 * (int) sr + 4800, 4 * (int) sr) < 1.0e-4);
    }

    void testFeedback()
    {
        // A short burst followed by silence: feedback makes the tail last longer.
        auto tailEnergy = [&] (float feedback)
        {
            Rig rig (make (1.0f, feedback, 0.0f, 4.0f, MovementStyle::cloud));
            Buffer b (2, 8 * 48000);
            for (int i = 0; i < 4800; ++i)
                b.data[0][(std::size_t) i] = b.data[1][(std::size_t) i] = 0.4f * (float) std::sin (2.0 * pi * 400.0 * i / 48000.0);
            rig.run (b, 512);
            CHECK (allFinite (b));
            return rms (b.data[0], 3 * 48000, 6 * 48000);
        };

        const double none = tailEnergy (0.0f);
        const double some = tailEnergy (0.8f);
        const double lots = tailEnergy (1.0f);
        CHECK (none < 1.0e-4);
        CHECK (some > 1.0e-3);
        CHECK (lots > some);
    }

    void testFeedbackStability()
    {
        // Maximum feedback with loud continuous input must stay bounded.
        for (auto style : { MovementStyle::drift, MovementStyle::cloud })
        {
            Rig rig (make (1.0f, 1.0f, 0.0f, 4.0f, style));
            Buffer b (2, 20 * 48000);
            fillNoise (b, 31, 1.0f);
            rig.run (b, 512);
            CHECK (allFinite (b));

            float peak = 0.0f;
            for (auto& c : b.data)
                for (float s : c)
                    peak = std::max (peak, std::abs (s));
            CHECK (peak < 4.0f);
        }
    }

    void testTremolo()
    {
        const double sr = 48000.0;
        Buffer b (2, 2 * 48000);
        for (auto& c : b.data) std::fill (c.begin(), c.end(), 1.0f);

        Rig rig (make (0.0f, 0.0f, 1.0f, 5.0f, MovementStyle::cloud), sr);
        rig.run (b, 512);
        CHECK (allFinite (b));

        float lo = 10.0f, hi = -10.0f;
        for (int i = 4800; i < b.samples(); ++i)
        {
            lo = std::min (lo, b.data[0][(std::size_t) i]);
            hi = std::max (hi, b.data[0][(std::size_t) i]);
        }
        CHECK_RANGE (lo, -0.001, 0.01);  // depth 1 reaches silence
        CHECK_RANGE (hi, 0.99, 1.001);

        // Rate: count the minima over one second.
        int minima = 0;
        for (int i = 48001; i < 2 * 48000 - 1; ++i)
        {
            const float a = b.data[0][(std::size_t) i - 1], c = b.data[0][(std::size_t) i], d = b.data[0][(std::size_t) i + 1];
            if (c < a && c <= d && c < 0.05f)
                ++minima;
        }
        CHECK_RANGE (minima, 4, 6); // 5 Hz

        // Right channel runs a quarter cycle behind.
        int phaseAtMin = -1;
        for (int i = 48000; i < 2 * 48000; ++i)
            if (b.data[0][(std::size_t) i] < 0.002f && phaseAtMin < 0) phaseAtMin = i;
        CHECK (phaseAtMin >= 0);
        CHECK (std::abs (b.data[1][(std::size_t) phaseAtMin] - b.data[0][(std::size_t) phaseAtMin]) > 0.3f);

        // Depth scales the swing.
        Buffer h (2, 2 * 48000);
        for (auto& c : h.data) std::fill (c.begin(), c.end(), 1.0f);
        Rig half (make (0.0f, 0.0f, 0.5f, 5.0f, MovementStyle::cloud));
        half.run (h, 512);
        float lo2 = 10.0f;
        for (int i = 4800; i < h.samples(); ++i) lo2 = std::min (lo2, h.data[0][(std::size_t) i]);
        CHECK_RANGE (lo2, 0.49, 0.52);
    }

    void testClickFreeParameterChanges()
    {
        // Sine in, parameter jumps every 4000 samples: no sample-to-sample jump beyond what the
        // signal itself (plus the grains' random content) can produce.
        Buffer b (2, 3 * 48000);
        fillSine (b, 200.0, 48000.0, 0.4f);
        Rig rig (make (0.5f, 0.3f, 0.3f, 3.0f, MovementStyle::cloud));

        std::mt19937 rng (77);
        std::uniform_real_distribution<float> u (0.0f, 1.0f);
        for (int start = 0; start < b.samples(); start += 4000)
        {
            rig.processor.setParameters (make (u (rng), u (rng), u (rng), 0.1f + 14.0f * u (rng),
                                               (MovementStyle) (int) (u (rng) * 2.99f)));
            rig.processor.process (b.view().getSubBlock (start, std::min (4000, b.samples() - start)));
        }
        CHECK (allFinite (b));
        CHECK (maxStep (b.data[0], 0, b.samples()) < 0.5);

        // Grain 1 -> 0 and back, with the tremolo off: smooth.
        Buffer c (2, 48000);
        fillSine (c, 300.0, 48000.0, 0.4f);
        Rig r2 (make (1.0f, 0.0f, 0.0f, 4.0f, MovementStyle::cloud));
        for (int start = 0, k = 0; start < c.samples(); start += 2400, ++k)
        {
            r2.processor.setParameters (make (k % 2 == 0 ? 1.0f : 0.0f, 0.0f, 0.0f, 4.0f, MovementStyle::cloud));
            r2.processor.process (c.view().getSubBlock (start, std::min (2400, c.samples() - start)));
        }
        CHECK (maxStep (c.data[0], 0, c.samples()) < 0.4);
    }

    void testBlockSizeIndependence()
    {
        Buffer in (2, 48000);
        fillNoise (in, 55, 0.4f);
        const auto p = make (0.8f, 0.5f, 0.4f, 5.0f, MovementStyle::cloud);

        Buffer reference = in;
        { Rig rig (p); rig.run (reference, 480000); }

        for (int blockSize : { 1, 7, 64, 333, 512 })
        {
            Buffer b = in;
            Rig rig (p);
            rig.run (b, blockSize);
            CHECK (b.data == reference.data);
        }
    }

    void testSampleRates()
    {
        for (double sr : { 22050.0, 44100.0, 48000.0, 96000.0, 192000.0 })
        {
            Buffer b (2, (int) (3 * sr));
            fillNoise (b, 9, 0.5f);
            Rig rig (make (1.0f, 0.7f, 0.5f, 6.0f, MovementStyle::cloud), sr);
            rig.run (b, 256);
            CHECK (allFinite (b));
            CHECK (rms (b.data[0], (int) sr, (int) (3 * sr)) > 0.01);
        }
    }

    void testExtremesAndInvalidValues()
    {
        const float inf = std::numeric_limits<float>::infinity();
        const float nan = std::numeric_limits<float>::quiet_NaN();

        Buffer b (2, 48000);
        fillNoise (b, 14, 1.0f);
        Rig rig (make (1.0f, 1.0f, 1.0f, 15.0f, MovementStyle::cloud));
        rig.run (b, 100);
        CHECK (allFinite (b));

        MovementParameters bad;
        bad.grain = nan; bad.feedback = inf; bad.tremolo = -inf; bad.rateHz = nan;
        bad.style = (MovementStyle) 99;
        rig.processor.setParameters (bad);
        const auto q = rig.processor.getParameters();
        CHECK (std::isfinite (q.grain) && std::isfinite (q.feedback) && std::isfinite (q.tremolo) && std::isfinite (q.rateHz));
        CHECK (q.style == MovementStyle::cloud);

        MovementParameters out;
        out.grain = 5.0f; out.feedback = -3.0f; out.tremolo = 9.0f; out.rateHz = 1000.0f;
        rig.processor.setParameters (out);
        const auto r = rig.processor.getParameters();
        CHECK (r.grain == 1.0f && r.feedback == 0.0f && r.tremolo == 1.0f && r.rateHz == movement::rateMaxHz);

        Buffer c (2, 48000);
        fillNoise (c, 15, 1.0f);
        rig.run (c, 512);
        CHECK (allFinite (c));

        // Random automation of every knob.
        std::mt19937 rng (4);
        std::uniform_real_distribution<float> u (0.0f, 1.0f);
        Buffer d (2, 5 * 48000);
        fillNoise (d, 16, 0.8f);
        Rig rig2 (allOff());
        for (int start = 0; start < d.samples(); start += 128)
        {
            rig2.processor.setParameters (make (u (rng), u (rng), u (rng), 0.1f + 14.9f * u (rng),
                                                (MovementStyle) (int) (u (rng) * 2.99f)));
            rig2.processor.process (d.view().getSubBlock (start, std::min (128, d.samples() - start)));
        }
        CHECK (allFinite (d));
    }

    void testMonoAndSilence()
    {
        Buffer mono (1, 3 * 48000);
        fillSine (mono, 330.0, 48000.0, 0.5f);
        Rig rig (make (1.0f, 0.5f, 0.5f, 5.0f, MovementStyle::drift), 48000.0, 1);
        rig.run (mono, 256);
        CHECK (allFinite (mono));
        CHECK (rms (mono.data[0], 48000, 3 * 48000) > 0.05);

        Buffer silence (2, 48000);
        Rig s (make (1.0f, 1.0f, 1.0f, 5.0f, MovementStyle::cloud));
        s.run (silence, 512);
        bool allZero = true;
        for (auto& c : silence.data)
            for (float x : c)
                allZero = allZero && x == 0.0f;
        CHECK (allZero);
    }

    void testResetRepeatable()
    {
        Buffer in (2, 48000);
        fillNoise (in, 90, 0.4f);
        const auto p = make (0.9f, 0.6f, 0.3f, 4.0f, MovementStyle::cloud);
        Rig rig (p);

        Buffer first = in;
        rig.run (first, 512);

        rig.processor.reset();
        Buffer second = in;
        rig.run (second, 512);
        CHECK (first.data == second.data);
    }

    void testVoiceLimit()
    {
        Rig rig (make (1.0f, 0.0f, 0.0f, 4.0f, MovementStyle::cloud));
        Buffer b (2, 48000);
        fillNoise (b, 2, 0.5f);

        int maxVoices = 0;
        for (int start = 0; start < b.samples(); start += 64)
        {
            rig.processor.process (b.view().getSubBlock (start, 64));
            maxVoices = std::max (maxVoices, rig.processor.getActiveVoices());
        }
        CHECK (maxVoices > 0 && maxVoices <= movement::maxVoices);
    }

    void testNoAllocationsAndPerformance()
    {
        Rig rig (make (1.0f, 0.7f, 0.5f, 5.0f, MovementStyle::cloud));
        Buffer b (2, 512);
        fillNoise (b, 1, 0.5f);

        for (int i = 0; i < 20; ++i)
            rig.processor.process (b.view());

        allocationCount = 0;
        countAllocations = true;
        for (int i = 0; i < 200; ++i)
        {
            rig.processor.setParameters (make (0.2f + 0.004f * (float) i, 0.5f, 0.4f, 5.0f, (MovementStyle) (i % 3)));
            rig.processor.process (b.view());
        }
        countAllocations = false;
        CHECK (allocationCount.load() == 0);

        const int seconds = 60;
        Buffer big (2, 48000);
        fillNoise (big, 2, 0.5f);
        const auto t0 = std::chrono::steady_clock::now();
        for (int s = 0; s < seconds; ++s)
            rig.run (big, 512);
        const auto t1 = std::chrono::steady_clock::now();
        const double elapsed = std::chrono::duration<double> (t1 - t0).count();
        std::printf ("  perf: %d s of stereo 48 kHz in %.3f s (%.0fx real time, %.2f %% of one core)\n",
                     seconds, elapsed, seconds / elapsed, 100.0 * elapsed / seconds);
        CHECK (elapsed < seconds * 0.25);
    }

    struct TestCase { const char* name; void (*fn)(); };
}

int main()
{
    const TestCase tests[] = {
        { "Off = bit-transparent (mono+stereo)",      testBitTransparentWhenOff },
        { "Grain knob: more granular signal",         testGrainKnob },
        { "Grain level stays sensible",               testLevelStaysSensible },
        { "Styles differ; Cloud is wide",             testStyles },
        { "Frozen holds after the input stops",       testFrozenHoldsAfterInputStops },
        { "Feedback sustains the tail",               testFeedback },
        { "Feedback stays bounded",                   testFeedbackStability },
        { "Tremolo: depth, rate, stereo phase",       testTremolo },
        { "Click-free parameter changes",             testClickFreeParameterChanges },
        { "Block-size independence",                  testBlockSizeIndependence },
        { "Sample rates 22.05-192 kHz",               testSampleRates },
        { "Extremes, NaN/Inf, random automation",     testExtremesAndInvalidValues },
        { "Mono and silence",                         testMonoAndSilence },
        { "Reset is repeatable",                      testResetRepeatable },
        { "Voice limit",                              testVoiceLimit },
        { "No allocations + CPU",                     testNoAllocationsAndPerformance },
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
