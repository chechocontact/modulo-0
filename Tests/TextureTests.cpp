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
void* operator new[] (std::size_t size) { return operator new (size); }
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
        p.wowDepth = p.flutterDepth = p.vinylAmount = p.vinylDrop = p.degradeAmount = 0.0f;
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
        p.wowShape = 0.0f;

        Rig rig (p);
        Buffer b (2, 5 * 48000);
        fillSine (b, 1000.0, 48000.0, 0.5f);
        rig.run (b, 512);

        // Expected: +-2*pi*rate*excursion = +-1.885 %
        const auto d = measureDeviation (b.data[0], 48000.0, 1000.0, 24000, 5 * 48000);
        CHECK_RANGE (d.high, 0.0170, 0.0205);
        CHECK_RANGE (d.low, -0.0205, -0.0170);

        // Half the depth, half the deviation.
        p.wowDepth = 0.5f;
        Rig half (p);
        Buffer c (2, 5 * 48000);
        fillSine (c, 1000.0, 48000.0, 0.5f);
        half.run (c, 512);
        const auto h = measureDeviation (c.data[0], 48000.0, 1000.0, 24000, 5 * 48000);
        CHECK_RANGE (h.high, 0.0085, 0.0103);

        // The two stereo channels move almost together (tape wow is correlated).
        const auto d1 = measureDeviation (b.data[1], 48000.0, 1000.0, 24000, 5 * 48000);
        CHECK_RANGE (d1.high, 0.0170, 0.0205);
    }

    void testWowShapes()
    {
        TextureParameters p = allOff();
        p.wowDepth = 1.0f;
        p.wowRate = 1.0f;

        double previousHigh = -1.0;
        for (float shape : { 0.0f, 0.5f, 1.0f })
        {
            p.wowShape = shape;
            Rig rig (p);
            Buffer b (2, 8 * 48000);
            fillSine (b, 1000.0, 48000.0, 0.5f);
            rig.run (b, 512);

            CHECK (allFinite (b));
            const auto d = measureDeviation (b.data[0], 48000.0, 1000.0, 24000, 8 * 48000);
            CHECK (d.cycles > 1000);
            CHECK_RANGE (d.high, 0.002, 0.04);   // drift is random: only bounded
            CHECK_RANGE (d.low, -0.04, -0.002);

            if (shape > 0.0f)
                CHECK (std::abs (d.high - previousHigh) > 1.0e-4); // shapes really differ
            previousHigh = d.high;
        }
    }

    void testFlutterPitch()
    {
        TextureParameters p = allOff();
        p.flutterDepth = 1.0f;
        p.flutterRate = 10.0f;
        p.flutterVariation = 0.0f;

        Rig rig (p);
        Buffer b (2, 3 * 48000);
        fillSine (b, 1000.0, 48000.0, 0.5f);
        rig.run (b, 512);

        // Expected: +-2*pi*10*0.2ms = +-1.257 %
        const auto d = measureDeviation (b.data[0], 48000.0, 1000.0, 24000, 3 * 48000);
        CHECK_RANGE (d.high, 0.0108, 0.0142);
        CHECK_RANGE (d.low, -0.0142, -0.0108);

        // Variation changes the pattern but stays bounded.
        p.flutterVariation = 1.0f;
        Rig varied (p);
        Buffer c (2, 3 * 48000);
        fillSine (c, 1000.0, 48000.0, 0.5f);
        varied.run (c, 512);
        CHECK (allFinite (c));
        const auto v = measureDeviation (c.data[0], 48000.0, 1000.0, 24000, 3 * 48000);
        CHECK_RANGE (v.high, 0.005, 0.03);
        CHECK (c.data[0] != b.data[0]);
    }

    void testVinylNoiseLevels()
    {
        // Silence in: only the vinyl noise comes out.
        TextureParameters p = allOff();
        p.vinylAmount = 1.0f;

        Rig rig (p);
        Buffer b (2, 10 * 48000);
        rig.run (b, 512);
        CHECK (allFinite (b));

        const double fullDb = toDb (rms (b.data[0], 4800, 10 * 48000));
        std::printf ("  vinyl amount 1.0 (normal): %.1f dBFS RMS\n", fullDb);
        CHECK_RANGE (fullDb, -56.0, -44.0);

        p.vinylAmount = 0.5f;
        Rig half (p);
        Buffer c (2, 10 * 48000);
        half.run (c, 512);
        const double halfDb = toDb (rms (c.data[0], 4800, 10 * 48000));
        CHECK (halfDb < fullDb - 3.0);

        // Stereo noise is independent between channels.
        double dot = 0.0, n0 = 0.0, n1 = 0.0;
        for (int i = 4800; i < b.samples(); ++i)
        {
            dot += (double) b.data[0][(std::size_t) i] * b.data[1][(std::size_t) i];
            n0 += (double) b.data[0][(std::size_t) i] * b.data[0][(std::size_t) i];
            n1 += (double) b.data[1][(std::size_t) i] * b.data[1][(std::size_t) i];
        }
        CHECK (std::abs (dot / std::sqrt (n0 * n1)) < 0.1);

        // The noise does not depend on the input level.
        Rig loud (p);
        Buffer d (2, 10 * 48000);
        fillSine (d, 1000.0, 48000.0, 0.8f);
        Buffer e = d;
        loud.run (e, 512);
        double noiseOnly = 0.0;
        for (int i = 4800; i < 10 * 48000; ++i)
        {
            const double diff = (double) e.data[0][(std::size_t) i] - d.data[0][(std::size_t) i];
            noiseOnly += diff * diff;
        }
        CHECK (std::abs (toDb (std::sqrt (noiseOnly / (10 * 48000 - 4800))) - halfDb) < 1.0);
    }

    // Counts crackle events in `b` as groups of samples above `threshold`.
    struct Events { double perSecond; double peak; };

    Events countEvents (const std::vector<float>& y, double sr, double threshold)
    {
        int count = 0;
        long lastIndex = -1000000;
        double peak = 0.0;
        for (std::size_t i = 4800; i < y.size(); ++i)
        {
            const double a = std::abs ((double) y[i]);
            peak = std::max (peak, a);
            if (a > threshold)
            {
                if ((long) i - lastIndex > 480) // 10 ms apart = separate event
                    ++count;
                lastIndex = (long) i;
            }
        }
        return { count / ((y.size() - 4800) / sr), peak };
    }

    void testCrackleTypes()
    {
        const double thresholdLinear = std::pow (10.0, -32.0 / 20.0); // above the hiss peaks (about -36 dBFS)
        Events ev[3];

        for (int t = 0; t < 3; ++t)
        {
            TextureParameters p = allOff();
            p.vinylAmount = 1.0f;
            p.crackleType = (CrackleType) t;
            Rig rig (p);
            Buffer b (2, 20 * 48000);
            rig.run (b, 512);
            CHECK (allFinite (b));
            ev[t] = countEvents (b.data[0], 48000.0, thresholdLinear);
            std::printf ("  crackle %d: %.1f events/s, peak %.1f dBFS\n", t, ev[t].perSecond, toDb (ev[t].peak));
        }

        CHECK (ev[0].perSecond > 0.5);           // normal is audible
        CHECK (ev[2].peak > ev[0].peak * 1.5);   // heavy pops are louder than normal ticks
        CHECK (ev[2].peak > ev[1].peak * 1.5);   // ... and than dusty
        CHECK (ev[1].peak < ev[0].peak * 1.2);   // dusty is the quietest-peaked type

        // Less Amount, fewer and softer events.
        TextureParameters p = allOff();
        p.vinylAmount = 0.25f;
        Rig rig (p);
        Buffer b (2, 20 * 48000);
        rig.run (b, 512);
        const auto quiet = countEvents (b.data[0], 48000.0, thresholdLinear);
        CHECK (quiet.perSecond < ev[0].perSecond);
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

    void testDegradeBandwidth()
    {
        TextureParameters p = allOff();
        p.degradeAmount = 1.0f;
        p.degradeSmoothing = 0.0f;

        p.degradeBandwidth = 0.0f; // narrow: 500 Hz .. 1.2 kHz
        const double narrow1k = gainAt (p, 1000.0);
        const double narrow10k = gainAt (p, 10000.0);
        const double narrow100 = gainAt (p, 100.0);
        std::printf ("  narrow: 100 Hz %.1f dB, 1 kHz %.1f dB, 10 kHz %.1f dB\n",
                     toDb (narrow100), toDb (narrow1k), toDb (narrow10k));
        CHECK (toDb (narrow10k) < -30.0);
        CHECK (toDb (narrow100) < -20.0);
        CHECK (narrow1k > 0.4);

        p.degradeBandwidth = 1.0f; // wide: 40 Hz .. 12 kHz
        const double wide10k = gainAt (p, 10000.0);
        const double wide1k = gainAt (p, 1000.0);
        const double wide100 = gainAt (p, 100.0);
        CHECK (toDb (wide10k) > -4.5);
        CHECK (std::abs (toDb (wide1k)) < 1.0);
        CHECK (toDb (wide100) > -1.0);
        CHECK (wide10k > narrow10k * 20.0);

        // Amount 0 is a true bypass at any bandwidth.
        p.degradeAmount = 0.0f;
        p.degradeBandwidth = 0.0f;
        CHECK (std::abs (gainAt (p, 10000.0) - 1.0) < 1.0e-6);

        // Half Amount sits between clean and fully degraded.
        p.degradeAmount = 0.5f;
        const double halfWay = gainAt (p, 10000.0);
        CHECK (halfWay > narrow10k && halfWay < 0.9);
    }

    void testDegradeSmoothing()
    {
        // Sparse clicks over a quiet bed: smoothing must flatten the peaks.
        auto clickPeak = [] (float smoothing)
        {
            TextureParameters p = allOff();
            p.degradeAmount = 1.0f;
            p.degradeBandwidth = 1.0f;
            p.degradeSmoothing = smoothing;

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

        const double plain = clickPeak (0.0f);
        const double smoothed = clickPeak (1.0f);
        std::printf ("  click peak: smoothing 0 = %.3f, smoothing 1 = %.3f\n", plain, smoothed);
        CHECK (smoothed < plain * 0.7);

        // Steady tones are barely touched.
        TextureParameters p = allOff();
        p.degradeAmount = 1.0f;
        p.degradeBandwidth = 1.0f;
        p.degradeSmoothing = 0.0f;
        const double without = gainAt (p, 440.0);
        p.degradeSmoothing = 1.0f;
        const double with = gainAt (p, 440.0);
        CHECK (with > without * 0.85);
    }

    void testBlockSizeIndependence()
    {
        TextureParameters p;
        p.wowDepth = 0.8f; p.wowShape = 0.7f; p.flutterDepth = 0.7f; p.flutterVariation = 0.8f;
        p.vinylAmount = 0.6f; p.crackleType = CrackleType::heavy; p.vinylDrop = 0.8f;
        p.degradeAmount = 0.7f; p.degradeBandwidth = 0.4f; p.degradeSmoothing = 0.6f;

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
        p.flutterDepth = 0.0f;
        p.vinylAmount = 0.5f;
        p.degradeAmount = 0.5f;

        for (double sr : { 22050.0, 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
        {
            Rig rig (p, sr, 2, 1024);
            Buffer b (2, (int) (4 * sr));
            fillSine (b, 1000.0, sr, 0.5f);
            rig.run (b, 1024);
            CHECK (allFinite (b));

            // Wow depth in percent is independent of the sample rate.
            const auto d = measureDeviation (b.data[0], sr, 1000.0, (int) (0.5 * sr), (int) (4 * sr));
            CHECK_RANGE (d.high, 0.0150, 0.0215);
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
            p.wowShape = u (rng);
            p.flutterDepth = -1.0f + 3.0f * u (rng);
            p.flutterRate = u (rng) * 40.0f;
            p.flutterVariation = u (rng);
            p.vinylAmount = u (rng);
            p.crackleType = (CrackleType) (int) (u (rng) * 2.99f);
            p.vinylDrop = u (rng);
            p.degradeAmount = u (rng) < 0.3f ? 0.0f : u (rng);
            p.degradeBandwidth = u (rng);
            p.degradeSmoothing = u (rng);
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
        bad.degradeAmount = -std::numeric_limits<float>::infinity();
        bad.crackleType = (CrackleType) 77;
        Rig nanRig (TextureParameters {});
        nanRig.processor.setParameters (bad);
        Buffer c (2, 8192);
        fillSine (c, 500.0, 48000.0, 0.5f);
        nanRig.run (c, 256);
        CHECK (allFinite (c));
        const auto q = nanRig.processor.getParameters();
        CHECK (std::isfinite (q.wowDepth) && q.crackleType == CrackleType::normal);
    }

    void testMonoAndEdgeCases()
    {
        TextureParameters p;
        p.vinylAmount = 0.5f;
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
        p.wowShape = 1.0f; p.flutterVariation = 1.0f;
        p.vinylAmount = 1.0f; p.crackleType = CrackleType::dusty; p.vinylDrop = 1.0f;
        p.degradeAmount = 1.0f;

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
        p.vinylAmount = 0.7f; p.vinylDrop = 1.0f; p.degradeAmount = 0.8f;
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
            q.wowDepth = u (rng); q.flutterDepth = u (rng); q.vinylAmount = u (rng);
            q.crackleType = (CrackleType) (int) (u (rng) * 2.99f);
            q.degradeAmount = u (rng); q.degradeBandwidth = u (rng);
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
        p.wowDepth = 0.6f; p.flutterDepth = 0.5f; p.vinylAmount = 0.5f; p.vinylDrop = 0.5f;
        p.degradeAmount = 0.7f; p.degradeSmoothing = 0.5f;

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
        { "Wow: shapes",                        testWowShapes },
        { "Flutter: measured pitch deviation",  testFlutterPitch },
        { "Vinyl: noise level, stereo, independent of input", testVinylNoiseLevels },
        { "Vinyl: crackle types",               testCrackleTypes },
        { "Vinyl: drop",                        testVinylDrop },
        { "Degrade: bandwidth",                 testDegradeBandwidth },
        { "Degrade: transient smoothing",       testDegradeSmoothing },
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
