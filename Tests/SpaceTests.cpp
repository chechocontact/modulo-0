// Vintage Maker — Module 4 (SPACE) tests. Console program, no JUCE.
// Exit code 0 = all checks passed.

#include "DSP/Space/SpaceProcessor.h"
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

    SpaceParameters allOff()
    {
        SpaceParameters p;
        p.delay = 0.0f;
        p.mix = 0.0f;
        return p;
    }

    struct Rig
    {
        explicit Rig (const SpaceParameters& p, double sampleRate = 48000.0, int channels = 2, double bpm = 120.0)
        {
            processor.prepare ({ sampleRate, 512, channels });
            processor.setTempo (bpm);
            processor.setParameters (p);
            processor.reset();
        }

        void run (Buffer& b, int blockSize)
        {
            for (int start = 0; start < b.samples(); start += blockSize)
                processor.process (b.view().getSubBlock (start, std::min (blockSize, b.samples() - start)));
        }

        SpaceProcessor processor;
    };

    SpaceParameters delayOnly (float time, float feedback, float width, bool sync = false)
    {
        SpaceParameters p;
        p.delay = 1.0f; p.time = time; p.feedback = feedback; p.width = width; p.sync = sync; p.mix = 0.0f;
        return p;
    }

    SpaceParameters reverbOnly (float mix, float decay, ReverbType type, float width = 1.0f)
    {
        SpaceParameters p;
        p.delay = 0.0f; p.mix = mix; p.decay = decay; p.type = type; p.width = width;
        return p;
    }

    Buffer impulse (int length, int channels = 2, float amp = 1.0f)
    {
        Buffer b (channels, length);
        for (auto& c : b.data) c[0] = amp;
        return b;
    }

    // Index of the largest |sample| in [from, to).
    int peakIndex (const std::vector<float>& v, int from, int to)
    {
        int best = from;
        for (int i = from; i < to; ++i)
            if (std::abs (v[(std::size_t) i]) > std::abs (v[(std::size_t) best]))
                best = i;
        return best;
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

    void testDelayTimeFree()
    {
        // The first echo appears at the requested time (20 ms ... 1.5 s).
        for (float t : { 0.0f, 0.3f, 0.6f, 1.0f })
        {
            const double expected = space::freeTimeSeconds (t);
            Rig rig (delayOnly (t, 0.0f, 0.0f));
            Buffer b = impulse (2 * 48000);
            rig.run (b, 512);
            CHECK (allFinite (b));

            const int echo = peakIndex (b.data[0], 100, b.samples());
            CHECK_RANGE ((double) echo / 48000.0, expected - 0.0005, expected + 0.0005);
            CHECK_RANGE (std::abs (b.data[0][(std::size_t) echo]), 0.9, 1.1);
        }
    }

    void testDelaySync()
    {
        // 120 BPM: 1/4 = 500 ms, 1/8 = 250 ms, 1/8. = 375 ms, 1/8T = 166.7 ms.
        struct Case { int index; double beats; };
        for (int index : { 1, 2, 4, 5, 7 })
        {
            const float knob = (float) index / 7.0f;
            Rig rig (delayOnly (knob, 0.0f, 0.0f, true), 48000.0, 2, 120.0);
            Buffer b = impulse (3 * 48000);
            rig.run (b, 512);
            const double expected = (double) space::divisionBeats[(std::size_t) index] * 0.5; // 60/120
            const int echo = peakIndex (b.data[0], 100, b.samples());
            CHECK_RANGE ((double) echo / 48000.0, expected - 0.0005, expected + 0.0005);
        }

        // The tempo changes the time: 1/4 at 90 BPM = 666.7 ms.
        Rig rig (delayOnly (5.0f / 7.0f, 0.0f, 0.0f, true), 48000.0, 2, 90.0);
        Buffer b = impulse (2 * 48000);
        rig.run (b, 512);
        CHECK_RANGE ((double) peakIndex (b.data[0], 100, b.samples()) / 48000.0, 0.6662, 0.6672);

        // Sync off ignores the tempo.
        Rig free (delayOnly (0.5f, 0.0f, 0.0f, false), 48000.0, 2, 90.0);
        Buffer c = impulse (2 * 48000);
        free.run (c, 512);
        CHECK_RANGE ((double) peakIndex (c.data[0], 100, c.samples()) / 48000.0,
                     space::freeTimeSeconds (0.5f) - 0.0005, space::freeTimeSeconds (0.5f) + 0.0005);

        // Slow tempo: 1/2 at 40 BPM would be 3 s; it is limited to the longest delay line.
        Rig slow (delayOnly (1.0f, 0.0f, 0.0f, true), 48000.0, 2, 40.0);
        CHECK (slow.processor.getDelayTimeSeconds() <= space::maxDelaySeconds);
        Buffer d = impulse (4 * 48000);
        slow.run (d, 512);
        CHECK (allFinite (d));
    }

    void testTempoChangeWhileSyncing()
    {
        Rig rig (delayOnly (5.0f / 7.0f, 0.5f, 0.5f, true), 48000.0, 2, 120.0);
        Buffer b (2, 6 * 48000);
        fillSine (b, 300.0, 48000.0, 0.4f);
        for (int start = 0, k = 0; start < b.samples(); start += 4800, ++k)
        {
            rig.processor.setTempo (60.0 + 90.0 * (k % 7) / 6.0);
            rig.processor.process (b.view().getSubBlock (start, std::min (4800, b.samples() - start)));
        }
        CHECK (allFinite (b));
        CHECK (maxStep (b.data[0], 0, b.samples()) < 2.0);

        // Invalid tempos are ignored.
        rig.processor.setTempo (std::numeric_limits<double>::quiet_NaN());
        rig.processor.setTempo (-5.0);
        rig.processor.setTempo (0.0);
        CHECK (std::isfinite (rig.processor.getDelayTimeSeconds()) && rig.processor.getDelayTimeSeconds() > 0.0);
    }

    void testFeedbackRepeats()
    {
        // Echo n has amplitude ~ fb^n (the loop filters reduce it a little).
        const float t = 0.4f; // ~ 134 ms
        const int period = (int) std::lround (space::freeTimeSeconds (t) * 48000.0);

        auto echoes = [&] (float feedback)
        {
            Rig rig (delayOnly (t, feedback, 0.0f));
            Buffer b = impulse (2 * 48000);
            rig.run (b, 512);
            CHECK (allFinite (b));
            std::vector<double> e;
            for (int n = 1; n <= 4; ++n)
                e.push_back (std::abs (b.data[0][(std::size_t) peakIndex (b.data[0], n * period - 40, n * period + 40)]));
            return e;
        };

        const auto none = echoes (0.0f);
        const auto half = echoes (0.5f);
        const auto full = echoes (1.0f);

        CHECK (none[0] > 0.5 && none[1] < 0.01);          // no feedback: one echo only
        CHECK (half[1] > 0.1 && half[1] < half[0]);        // second echo is quieter
        CHECK (half[2] < half[1] && half[3] < half[2]);    // and so on: it decays
        CHECK (full[1] > half[1]);
        CHECK (full[3] < full[0]);                         // even at maximum, it decays
    }

    void testPingPongWidth()
    {
        // Mono impulse. Width 0: identical channels. Width 1: echoes alternate L / R.
        Rig mono (delayOnly (0.4f, 0.7f, 0.0f));
        Buffer a = impulse (2 * 48000);
        mono.run (a, 512);
        CHECK (a.data[0] == a.data[1]);

        const int period = (int) std::lround (space::freeTimeSeconds (0.4f) * 48000.0);
        Rig wide (delayOnly (0.4f, 0.7f, 1.0f));
        Buffer b = impulse (2 * 48000);
        wide.run (b, 512);

        auto level = [&] (int ch, int n)
        {
            return std::abs (b.data[(std::size_t) ch][(std::size_t) peakIndex (b.data[(std::size_t) ch], n * period - 40, n * period + 40)]);
        };
        CHECK (level (0, 1) > 0.5 && level (1, 1) < 0.01);  // first echo left only
        CHECK (level (1, 2) > 0.1 && level (0, 2) < 0.01);  // second echo right only
        CHECK (level (0, 3) > 0.05 && level (1, 3) < 0.01); // third echo left again
    }

    void testDelayFilteringOfRepeats()
    {
        // Repeats lose highs: a bright click is duller after several passes.
        Rig rig (delayOnly (0.3f, 0.9f, 0.0f));
        Buffer b = impulse (3 * 48000);
        rig.run (b, 512);
        const int period = (int) std::lround (space::freeTimeSeconds (0.3f) * 48000.0);

        auto roughness = [&] (int n)
        {
            double sum = 0.0, energy = 0.0;
            for (int i = n * period - 60; i < n * period + 60; ++i)
            {
                const double d = (double) b.data[0][(std::size_t) i] - b.data[0][(std::size_t) i - 1];
                sum += d * d;
                energy += (double) b.data[0][(std::size_t) i] * b.data[0][(std::size_t) i];
            }
            return sum / std::max (energy, 1.0e-20);
        };
        CHECK (roughness (4) < roughness (1));
    }

    void testDelayTimeChangeIsClickFree()
    {
        Rig rig (delayOnly (0.3f, 0.5f, 0.5f));
        Buffer b (2, 4 * 48000);
        fillSine (b, 220.0, 48000.0, 0.4f);

        std::mt19937 rng (5);
        std::uniform_real_distribution<float> u (0.0f, 1.0f);
        for (int start = 0; start < b.samples(); start += 4000)
        {
            rig.processor.setParameters (delayOnly (u (rng), 0.5f, 0.5f, u (rng) < 0.3f));
            rig.processor.process (b.view().getSubBlock (start, std::min (4000, b.samples() - start)));
        }
        CHECK (allFinite (b));
        CHECK (maxStep (b.data[0], 0, b.samples()) < 1.0);
    }

    // RT60 from the Schroeder backward integral (T20, extrapolated to 60 dB).
    double measureRt60 (const std::vector<float>& ir, double sr)
    {
        std::vector<double> edc (ir.size());
        double sum = 0.0;
        for (std::size_t i = ir.size(); i-- > 0;)
        {
            sum += (double) ir[i] * ir[i];
            edc[i] = sum;
        }

        auto dbAt = [&] (std::size_t i) { return 10.0 * std::log10 (std::max (edc[i] / edc[0], 1.0e-30)); };
        std::size_t a = 0, b = 0;
        for (std::size_t i = 0; i < ir.size(); ++i)
        {
            if (a == 0 && dbAt (i) < -5.0) a = i;
            if (b == 0 && dbAt (i) < -25.0) { b = i; break; }
        }
        if (a == 0 || b == 0 || b <= a)
            return -1.0;

        return 3.0 * (double) (b - a) / sr; // 20 dB span -> 60 dB
    }

    void testReverbDecayTime()
    {
        const double sr = 48000.0;
        struct Case { ReverbType type; float decay; };
        for (auto c : { Case { ReverbType::room, 0.5f }, Case { ReverbType::plate, 0.5f }, Case { ReverbType::hall, 0.3f },
                        Case { ReverbType::hall, 0.7f }, Case { ReverbType::plate, 0.9f } })
        {
            const auto cfg = space::typeConfig (c.type);
            const double expected = cfg.rt60Min * std::pow ((double) cfg.rt60Max / cfg.rt60Min, c.decay);

            Rig rig (reverbOnly (1.0f, c.decay, c.type));
            Buffer b = impulse ((int) (std::min (20.0, expected * 2.5 + 1.0) * sr));
            rig.run (b, 512);
            CHECK (allFinite (b));

            const double measured = measureRt60 (b.data[0], sr);
            std::printf ("  type %d decay %.1f: RT60 expected %.2f s, measured %.2f s\n", (int) c.type, c.decay, expected, measured);
            CHECK_RANGE (measured / expected, 0.6, 1.5);
        }

        // Longer Decay = longer tail, per type.
        for (auto type : { ReverbType::room, ReverbType::plate, ReverbType::hall })
        {
            auto rt = [&] (float decay)
            {
                Rig rig (reverbOnly (1.0f, decay, type));
                Buffer b = impulse (30 * 48000);
                rig.run (b, 512);
                return measureRt60 (b.data[0], sr);
            };
            CHECK (rt (0.8f) > rt (0.4f) && rt (0.4f) > rt (0.1f));
        }

        // Hall decays longer than Room at the same knob position.
        auto at = [&] (ReverbType type)
        {
            Rig rig (reverbOnly (1.0f, 0.5f, type));
            Buffer b = impulse (20 * 48000);
            rig.run (b, 512);
            return measureRt60 (b.data[0], sr);
        };
        CHECK (at (ReverbType::hall) > at (ReverbType::plate) && at (ReverbType::plate) > at (ReverbType::room));
    }

    void testReverbTypesDiffer()
    {
        Buffer r[3] = { impulse (48000), impulse (48000), impulse (48000) };
        int k = 0;
        for (auto type : { ReverbType::room, ReverbType::plate, ReverbType::hall })
        {
            Rig rig (reverbOnly (1.0f, 0.5f, type));
            rig.run (r[k++], 512);
        }
        CHECK (r[0].data != r[1].data && r[1].data != r[2].data);

        // Pre-delay differs: Hall starts later than Room.
        auto onset = [&] (const Buffer& b)
        {
            for (int i = 1; i < b.samples(); ++i)
                if (std::abs (b.data[0][(std::size_t) i]) > 0.01f)
                    return i;
            return -1;
        };
        CHECK (onset (r[2]) > onset (r[0]));
    }

    void testReverbLevelAndMix()
    {
        Buffer in (2, 6 * 48000);
        fillNoise (in, 17, 0.4f);
        const double inLevel = rms (in.data[0], 48000, 6 * 48000);

        for (auto type : { ReverbType::room, ReverbType::plate, ReverbType::hall })
            for (float decay : { 0.1f, 0.5f, 1.0f })
            {
                Buffer b = in;
                Rig rig (reverbOnly (1.0f, decay, type));
                rig.run (b, 512);
                CHECK (allFinite (b));

                // Output = dry + reverb. The reverb alone sits within a few dB of the input.
                double added = 0.0;
                for (int i = 48000; i < b.samples(); ++i)
                {
                    const double d = (double) b.data[0][(std::size_t) i] - in.data[0][(std::size_t) i];
                    added += d * d;
                }
                const double ratio = std::sqrt (added / (b.samples() - 48000)) / inLevel;
                std::printf ("  type %d decay %.1f: reverb level x%.2f of the input\n", (int) type, decay, ratio);
                CHECK_RANGE (ratio, 0.25, 2.0);
            }

        // Mix scales it.
        auto added = [&] (float mix)
        {
            Buffer b = in;
            Rig rig (reverbOnly (mix, 0.5f, ReverbType::plate));
            rig.run (b, 512);
            double sum = 0.0;
            for (int i = 48000; i < b.samples(); ++i)
            {
                const double d = (double) b.data[0][(std::size_t) i] - in.data[0][(std::size_t) i];
                sum += d * d;
            }
            return std::sqrt (sum / (b.samples() - 48000));
        };
        CHECK (added (0.25f) < added (0.5f) && added (0.5f) < added (1.0f));
        CHECK_RANGE (added (0.5f) / added (1.0f), 0.4, 0.6);
    }

    void testReverbWidth()
    {
        auto sideLevel = [&] (float width)
        {
            Buffer b (2, 2 * 48000);
            fillNoise (b, 3, 0.4f);
            Rig rig (reverbOnly (1.0f, 0.5f, ReverbType::hall, width));
            // Same signal in both channels: any difference is the reverb's own stereo image.
            b.data[1] = b.data[0];
            rig.run (b, 512);
            double diff = 0.0;
            for (int i = 24000; i < b.samples(); ++i)
            {
                const double d = (double) b.data[0][(std::size_t) i] - b.data[1][(std::size_t) i];
                diff += d * d;
            }
            return std::sqrt (diff / (b.samples() - 24000));
        };

        CHECK (sideLevel (0.0f) < 1.0e-5);
        CHECK (sideLevel (1.0f) > 0.05);
        CHECK (sideLevel (0.5f) < sideLevel (1.0f) && sideLevel (0.5f) > sideLevel (0.0f));
    }

    void testReverbTypeSwitchClickFree()
    {
        Buffer b (2, 4 * 48000);
        fillSine (b, 330.0, 48000.0, 0.4f);
        Rig rig (reverbOnly (1.0f, 0.5f, ReverbType::room));

        for (int start = 0, k = 0; start < b.samples(); start += 1200, ++k) // faster than the 50 ms fade
        {
            auto p = reverbOnly (1.0f, 0.5f, (ReverbType) (k % 3));
            rig.processor.setParameters (p);
            rig.processor.process (b.view().getSubBlock (start, std::min (1200, b.samples() - start)));
        }
        CHECK (allFinite (b));
        CHECK (maxStep (b.data[0], 0, b.samples()) < 0.6);
    }

    void testStabilityAtExtremes()
    {
        // Longest decay and full feedback on loud noise for a minute: must stay bounded.
        SpaceParameters p;
        p.delay = 1.0f; p.feedback = 1.0f; p.time = 0.2f; p.mix = 1.0f; p.decay = 1.0f; p.width = 1.0f;

        for (auto type : { ReverbType::room, ReverbType::plate, ReverbType::hall })
        {
            p.type = type;
            Rig rig (p);
            Buffer b (2, 48000);
            float peak = 0.0f;
            bool finite = true;
            for (int s = 0; s < 60; ++s)
            {
                fillNoise (b, 100u + (unsigned) s, 1.0f);
                rig.run (b, 512);
                finite = finite && allFinite (b);
                for (auto& c : b.data)
                    for (float x : c)
                        peak = std::max (peak, std::abs (x));
            }
            CHECK (finite);
            CHECK (peak < 12.0f);

            // And it dies away once the input stops.
            Buffer silence (2, 48000);
            Rig tail (p);
            Buffer burst (2, 48000);
            fillNoise (burst, 5, 1.0f);
            tail.run (burst, 512);
            for (int s = 0; s < 60; ++s)
            {
                for (auto& c : silence.data) std::fill (c.begin(), c.end(), 0.0f);
                tail.run (silence, 512);
            }
            CHECK (rms (silence.data[0], 0, 48000) < 0.05);
        }
    }

    void testBlockSizeIndependence()
    {
        Buffer in (2, 48000);
        fillNoise (in, 55, 0.4f);
        SpaceParameters p;
        p.delay = 0.6f; p.feedback = 0.5f; p.time = 0.4f; p.mix = 0.5f; p.decay = 0.4f; p.width = 0.7f;

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
            SpaceParameters p;
            p.delay = 0.7f; p.feedback = 0.6f; p.time = 0.5f; p.mix = 0.6f; p.decay = 0.6f;

            Buffer b = impulse ((int) (3 * sr));
            Rig rig (p, sr);
            rig.run (b, 256);
            CHECK (allFinite (b));
            CHECK (rms (b.data[0], (int) (0.5 * sr), (int) (3 * sr)) > 1.0e-5);

            // The delay time is the same in seconds at every rate.
            Rig d (delayOnly (0.5f, 0.0f, 0.0f), sr);
            Buffer c = impulse ((int) (2 * sr));
            d.run (c, 256);
            CHECK_RANGE ((double) peakIndex (c.data[0], 100, c.samples()) / sr,
                         space::freeTimeSeconds (0.5f) - 0.001, space::freeTimeSeconds (0.5f) + 0.001);
        }
    }

    void testExtremesAndInvalidValues()
    {
        const float inf = std::numeric_limits<float>::infinity();
        const float nan = std::numeric_limits<float>::quiet_NaN();

        SpaceParameters bad;
        bad.delay = nan; bad.time = inf; bad.feedback = -inf; bad.mix = nan; bad.decay = inf; bad.width = nan;
        bad.type = (ReverbType) 99;
        Rig rig (SpaceParameters {});
        rig.processor.setParameters (bad);
        const auto q = rig.processor.getParameters();
        CHECK (std::isfinite (q.delay) && std::isfinite (q.time) && std::isfinite (q.feedback)
               && std::isfinite (q.mix) && std::isfinite (q.decay) && std::isfinite (q.width));
        CHECK (q.type == ReverbType::hall);

        SpaceParameters out;
        out.delay = 5.0f; out.time = -3.0f; out.feedback = 9.0f; out.mix = -1.0f; out.decay = 7.0f; out.width = 4.0f;
        rig.processor.setParameters (out);
        const auto r = rig.processor.getParameters();
        CHECK (r.delay == 1.0f && r.time == 0.0f && r.feedback == 1.0f && r.mix == 0.0f && r.decay == 1.0f && r.width == 1.0f);

        Buffer b (2, 48000);
        fillNoise (b, 15, 1.0f);
        rig.run (b, 512);
        CHECK (allFinite (b));

        // Random automation of every knob.
        std::mt19937 rng (4);
        std::uniform_real_distribution<float> u (0.0f, 1.0f);
        Buffer d (2, 6 * 48000);
        fillNoise (d, 16, 0.8f);
        Rig rig2 (allOff());
        for (int start = 0; start < d.samples(); start += 128)
        {
            SpaceParameters p;
            p.delay = u (rng); p.time = u (rng); p.feedback = u (rng); p.sync = u (rng) < 0.5f;
            p.mix = u (rng); p.decay = u (rng); p.type = (ReverbType) (int) (u (rng) * 2.99f); p.width = u (rng);
            rig2.processor.setParameters (p);
            rig2.processor.process (d.view().getSubBlock (start, std::min (128, d.samples() - start)));
        }
        CHECK (allFinite (d));
    }

    void testMonoAndSilence()
    {
        Buffer mono (1, 3 * 48000);
        fillSine (mono, 330.0, 48000.0, 0.5f);
        SpaceParameters p;
        p.delay = 0.8f; p.mix = 0.6f; p.feedback = 0.5f; p.width = 1.0f;
        Rig rig (p, 48000.0, 1);
        rig.run (mono, 256);
        CHECK (allFinite (mono));
        CHECK (rms (mono.data[0], 48000, 3 * 48000) > 0.05);

        Buffer silence (2, 2 * 48000);
        Rig s (p);
        s.run (silence, 512);
        bool allZero = true;
        for (auto& c : silence.data)
            for (float x : c)
                allZero = allZero && x == 0.0f;
        CHECK (allZero);
    }

    void testResetAndStaleTail()
    {
        Buffer in (2, 48000);
        fillNoise (in, 90, 0.4f);
        SpaceParameters p;
        p.delay = 0.7f; p.feedback = 0.6f; p.mix = 0.5f; p.decay = 0.5f;
        Rig rig (p);

        Buffer first = in;
        rig.run (first, 512);
        rig.processor.reset();
        Buffer second = in;
        rig.run (second, 512);
        CHECK (first.data == second.data);

        // Reverb off, then on again: the old tail must not come back.
        SpaceParameters on = reverbOnly (1.0f, 0.8f, ReverbType::hall);
        Rig tail (on);
        Buffer burst (2, 24000);
        fillNoise (burst, 3, 0.8f);
        tail.run (burst, 512);

        tail.processor.setParameters (reverbOnly (0.0f, 0.8f, ReverbType::hall));
        Buffer gap (2, 4 * 48000);
        tail.run (gap, 512);

        tail.processor.setParameters (on);
        Buffer silence (2, 24000);
        tail.run (silence, 512);
        CHECK (rms (silence.data[0], 0, 24000) < 1.0e-6);
    }

    void testNoAllocationsAndPerformance()
    {
        SpaceParameters p;
        p.delay = 0.7f; p.feedback = 0.6f; p.time = 0.5f; p.mix = 0.6f; p.decay = 0.6f; p.width = 0.8f;
        Rig rig (p);
        Buffer b (2, 512);
        fillNoise (b, 1, 0.5f);

        for (int i = 0; i < 20; ++i)
            rig.processor.process (b.view());

        allocationCount = 0;
        countAllocations = true;
        for (int i = 0; i < 200; ++i)
        {
            p.type = (ReverbType) (i % 3);
            p.time = (float) (i % 50) / 50.0f;
            p.sync = (i % 2) == 0;
            p.mix = (i % 40 == 0) ? 0.0f : 0.6f;
            rig.processor.setTempo (100.0 + i);
            rig.processor.setParameters (p);
            rig.processor.process (b.view());
        }
        countAllocations = false;
        CHECK (allocationCount.load() == 0);

        // Worst case: all three reverb engines running (a type crossfade) is brief; the steady cost matters.
        Rig steady (p);
        const int seconds = 60;
        Buffer big (2, 48000);
        fillNoise (big, 2, 0.5f);
        const auto t0 = std::chrono::steady_clock::now();
        for (int s = 0; s < seconds; ++s)
            steady.run (big, 512);
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
        { "Delay: free time accuracy",                testDelayTimeFree },
        { "Delay: tempo sync",                        testDelaySync },
        { "Delay: tempo changes while synced",        testTempoChangeWhileSyncing },
        { "Delay: feedback repeats and decays",       testFeedbackRepeats },
        { "Delay: width = ping-pong",                 testPingPongWidth },
        { "Delay: repeats get darker",                testDelayFilteringOfRepeats },
        { "Delay: time changes are click-free",       testDelayTimeChangeIsClickFree },
        { "Reverb: measured RT60",                    testReverbDecayTime },
        { "Reverb: types differ",                     testReverbTypesDiffer },
        { "Reverb: level and mix",                    testReverbLevelAndMix },
        { "Reverb: width",                            testReverbWidth },
        { "Reverb: type switch is click-free",        testReverbTypeSwitchClickFree },
        { "Stability at extremes (60 s)",             testStabilityAtExtremes },
        { "Block-size independence",                  testBlockSizeIndependence },
        { "Sample rates 22.05-192 kHz",               testSampleRates },
        { "Extremes, NaN/Inf, random automation",     testExtremesAndInvalidValues },
        { "Mono and silence",                         testMonoAndSilence },
        { "Reset and stale reverb tail",              testResetAndStaleTail },
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
