// Vintage Maker — Module 6 (INTEGRATION: the five modules together) tests. Console program, no JUCE.
// Exit code 0 = all checks passed.

#include "Chain/VintageChain.h"
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

    // A tonal, music-like signal (harmonics of 110 Hz with a falling spectrum), identical in
    // every channel. White noise is a poor stand-in for level tests: Degrade, by design,
    // removes most of its energy.
    void fillTonal (Buffer& b, double sr, float amp, int seed = 0)
    {
        std::mt19937 rng ((unsigned) seed + 1u);
        std::uniform_real_distribution<double> phase (0.0, 2.0 * pi);
        const double freqs[] = { 110.0, 220.0, 330.0, 440.0, 660.0, 880.0, 1320.0, 1760.0, 2640.0 };
        const double gains[] = { 1.0, 0.8, 0.6, 0.6, 0.4, 0.4, 0.25, 0.2, 0.1 };
        double ph[9];
        for (double& p : ph) p = phase (rng);

        for (int i = 0; i < b.samples(); ++i)
        {
            double x = 0.0;
            for (int k = 0; k < 9; ++k)
                x += gains[k] * std::sin (2.0 * pi * freqs[k] * i / sr + ph[k]);
            for (int ch = 0; ch < b.channels(); ++ch)
                b.data[(std::size_t) ch][(std::size_t) i] = amp * (float) (x * 0.25);
        }
    }

    // Every amount at zero, Vintage Engine off, no vinyl: the whole plugin must be a wire.
    ChainParameters neutral()
    {
        ChainParameters c;
        c.master.vinylAmount = 0.0f;
        c.texture.wowDepth = c.texture.flutterDepth = c.texture.vinylDrop = c.texture.degrade = 0.0f;
        c.color.drive = c.color.tone = c.color.magicChorus = 0.0f;
        c.movement.grain = c.movement.feedback = c.movement.tremolo = 0.0f;
        c.space.delay = c.space.mix = 0.0f;
        c.engine.flux = 0.0f;
        return c;
    }

    // Every effect working hard, Vintage Engine at full strength.
    ChainParameters loaded()
    {
        ChainParameters c;
        c.master.vinylAmount = 0.6f;
        c.texture.wowDepth = 0.8f; c.texture.flutterDepth = 0.8f; c.texture.vinylDrop = 0.5f; c.texture.degrade = 0.6f;
        c.color.drive = 0.7f; c.color.type = ColorType::tape; c.color.tone = 0.4f; c.color.magicChorus = 0.8f;
        c.movement.grain = 0.8f; c.movement.feedback = 0.6f; c.movement.tremolo = 0.5f;
        c.movement.rateHz = 5.0f; c.movement.style = MovementStyle::cloud;
        c.space.delay = 0.6f; c.space.time = 0.4f; c.space.feedback = 0.5f; c.space.mix = 0.5f;
        c.space.decay = 0.6f; c.space.type = ReverbType::plate; c.space.width = 0.8f;
        c.engine.flux = 1.0f; c.engine.rate = 0.8f; c.engine.depth = 1.0f; c.engine.character = EngineCharacter::random;
        c.tempoBpm = 120.0;
        return c;
    }

    struct Rig
    {
        explicit Rig (const ChainParameters& p, double sampleRate = 48000.0, int channels = 2, int maxBlock = 512)
            : params (p)
        {
            chain.prepare ({ sampleRate, maxBlock, channels }, p);
        }

        void run (Buffer& b, int blockSize)
        {
            for (int start = 0; start < b.samples(); start += blockSize)
                chain.process (b.view().getSubBlock (start, std::min (blockSize, b.samples() - start)), params);
        }

        VintageChain chain;
        ChainParameters params;
    };

    double differenceRms (const Buffer& a, const Buffer& b, int from)
    {
        double sum = 0.0;
        for (int i = from; i < a.samples(); ++i)
        {
            const double d = (double) a.data[0][(std::size_t) i] - b.data[0][(std::size_t) i];
            sum += d * d;
        }
        return std::sqrt (sum / std::max (1, a.samples() - from));
    }

    float peakOf (const Buffer& b)
    {
        float p = 0.0f;
        for (const auto& c : b.data)
            for (float x : c)
                p = std::max (p, std::abs (x));
        return p;
    }

    // ------------------------------------------------------------------ tests

    void testNeutralIsAWire()
    {
        for (int channels : { 1, 2 })
        {
            Buffer in (channels, 48000);
            fillNoise (in, 3, 0.5f);
            Buffer out = in;
            Rig rig (neutral(), 48000.0, channels);
            rig.run (out, 256);
            CHECK (out.data == in.data);
            CHECK (rig.chain.getLatencySamples() == 0);
        }

        // Master bypass and Master Dry/Wet = 0 are wires too, whatever the effects are doing.
        auto bypassed = loaded();
        bypassed.master.bypass = true;
        auto dry = loaded();
        dry.master.mix = 0.0f;
        dry.master.vinylAmount = 0.0f;
        for (const auto& p : { bypassed, dry })
        {
            Buffer in (2, 48000);
            fillNoise (in, 4, 0.5f);
            Buffer out = in;
            Rig rig (p);
            rig.run (out, 512);
            CHECK (out.data == in.data);
        }
    }

    void testMagnitudeZeroIsAWire()
    {
        // Every effect at full strength, Vintage Engine included: Magnitude 0 removes it all.
        auto p = loaded();
        p.magnitude = 0.0f;

        for (int channels : { 1, 2 })
        {
            Buffer in (channels, 2 * 48000);
            fillNoise (in, 11, 0.5f);
            Buffer out = in;
            Rig rig (p, 48000.0, channels);
            rig.run (out, 512);
            CHECK (out.data == in.data);
        }
    }

    void testMagnitudeScalesAmountsOnly()
    {
        // Magnitude 0.5 must equal setting every AMOUNT to half by hand (and nothing else).
        // The Vintage Engine is off here: its offsets are added before Magnitude, by design.
        auto p = loaded();
        p.engine.flux = 0.0f;
        p.magnitude = 0.5f;

        auto q = p;
        q.magnitude = 1.0f;
        q.master.vinylAmount *= 0.5f;
        q.texture.wowDepth *= 0.5f; q.texture.flutterDepth *= 0.5f; q.texture.vinylDrop *= 0.5f; q.texture.degrade *= 0.5f;
        q.color.drive *= 0.5f; q.color.tone *= 0.5f; q.color.magicChorus *= 0.5f;
        q.movement.grain *= 0.5f; q.movement.feedback *= 0.5f; q.movement.tremolo *= 0.5f;
        q.space.delay *= 0.5f; q.space.mix *= 0.5f;

        Buffer in (2, 3 * 48000);
        fillNoise (in, 12, 0.4f);
        Buffer a = in, b = in;
        { Rig rig (p); rig.run (a, 512); }
        { Rig rig (q); rig.run (b, 512); }
        CHECK (a.data == b.data); // rates, types, Time, Decay, Width, Sync, Dry/Wet, gains: untouched

        // The caller's parameters are never modified (they are the host's values).
        const auto before = p;
        { Rig rig (p); Buffer c = in; rig.run (c, 512); }
        CHECK (p.texture == before.texture && p.color == before.color && p.movement == before.movement
               && p.space == before.space && p.master == before.master && p.magnitude == before.magnitude);

        // NaN or out-of-range Magnitude: treated as 100 % / clamped.
        auto full = loaded(); full.engine.flux = 0.0f;
        auto nanP = full; nanP.magnitude = std::numeric_limits<float>::quiet_NaN();
        auto hugeP = full; hugeP.magnitude = 7.0f;
        Buffer x = in, y = in, z = in;
        { Rig r (full); r.run (x, 512); }
        { Rig r (nanP); r.run (y, 512); }
        { Rig r (hugeP); r.run (z, 512); }
        CHECK (x.data == y.data && x.data == z.data);
    }

    void testMagnitudeIsMonotonic()
    {
        // One effect at a time, so the measurement is not swamped by time shifts: more Magnitude
        // means a bigger difference from the input, and 0 means none at all.
        struct Case { const char* name; ChainParameters p; };
        std::vector<Case> cases;
        { auto p = neutral(); p.color.drive = 1.0f; p.color.tone = 0.8f;                cases.push_back ({ "color", p }); }
        { auto p = neutral(); p.movement.tremolo = 1.0f;                                cases.push_back ({ "tremolo", p }); }
        { auto p = neutral(); p.space.delay = 1.0f; p.space.feedback = 0.4f;            cases.push_back ({ "delay", p }); }
        { auto p = neutral(); p.space.mix = 1.0f; p.space.decay = 0.5f;                 cases.push_back ({ "reverb", p }); }
        { auto p = neutral(); p.texture.degrade = 1.0f;                                 cases.push_back ({ "degrade", p }); }
        { auto p = neutral(); p.master.vinylAmount = 1.0f;                              cases.push_back ({ "vinyl", p }); }
        { auto p = neutral(); p.movement.grain = 1.0f;                                  cases.push_back ({ "grain", p }); }

        Buffer in (2, 4 * 48000);
        fillTonal (in, 48000.0, 0.8f);

        for (auto& c : cases)
        {
            auto effect = [&] (float magnitude)
            {
                auto p = c.p;
                p.magnitude = magnitude;
                Buffer b = in;
                Rig rig (p);
                rig.run (b, 512);
                CHECK (allFinite (b));
                return differenceRms (b, in, 24000);
            };

            const double m0 = effect (0.0f), m25 = effect (0.25f), m50 = effect (0.5f), m100 = effect (1.0f);
            std::printf ("  %-8s effect size: 0%% %.4f, 25%% %.4f, 50%% %.4f, 100%% %.4f\n", c.name, m0, m25, m50, m100);
            CHECK (m0 == 0.0);
            CHECK (m0 < m25 && m25 < m50 && m50 < m100);
        }
    }

    void testMagnitudeAutomationIsClickFree()
    {
        // Only Drive and Tone: a 200 Hz sine, Magnitude jumping around every 64 samples.
        auto p = neutral();
        p.color.drive = 1.0f; p.color.tone = 0.6f;
        Buffer b (2, 2 * 48000);
        fillSine (b, 200.0, 48000.0, 0.4f);
        Rig rig (p);

        std::mt19937 rng (21);
        std::uniform_real_distribution<float> u (0.0f, 1.0f);
        for (int start = 0; start < b.samples(); start += 64)
        {
            rig.params.magnitude = u (rng);
            rig.chain.process (b.view().getSubBlock (start, std::min (64, b.samples() - start)), rig.params);
        }
        CHECK (allFinite (b));
        CHECK (maxStep (b.data[0], 0, b.samples()) < 0.3);

        // And with everything on: stays finite and bounded under the same abuse.
        auto q = loaded();
        Buffer c (2, 4 * 48000);
        fillNoise (c, 22, 0.4f);
        Rig rig2 (q);
        for (int start = 0; start < c.samples(); start += 64)
        {
            rig2.params.magnitude = u (rng);
            rig2.chain.process (c.view().getSubBlock (start, std::min (64, c.samples() - start)), rig2.params);
        }
        CHECK (allFinite (c));
        CHECK (peakOf (c) < 10.0f);
    }

    void testVintageEngineNeverSwitchesEffectsOn()
    {
        // Engine at full strength, Vinyl Amount and every amount at 0: the plugin stays a wire.
        auto p = neutral();
        p.engine.flux = 1.0f; p.engine.depth = 1.0f; p.engine.rate = 1.0f; p.engine.character = EngineCharacter::random;

        Buffer in (2, 3 * 48000);
        fillNoise (in, 14, 0.5f);
        Buffer out = in;
        Rig rig (p);
        rig.run (out, 512);
        CHECK (out.data == in.data);
    }

    void testEverySlotCombination()
    {
        for (int mask = 0; mask < 16; ++mask)
        {
            auto p = loaded();
            p.textureSlot.enabled = (mask & 1) != 0;
            p.colorSlot.enabled = (mask & 2) != 0;
            p.movementSlot.enabled = (mask & 4) != 0;
            p.spaceSlot.enabled = (mask & 8) != 0;

            Buffer in (2, 4 * 48000);
            fillTonal (in, 48000.0, 0.6f, mask);
            Buffer out = in;
            Rig rig (p);
            rig.run (out, 512);

            CHECK (allFinite (out));
            CHECK (peakOf (out) < 10.0f);

            // Level stays in a sane window around the input (noise input rms ~0.23).
            const double ratio = rms (out.data[0], 48000, 4 * 48000) / rms (in.data[0], 48000, 4 * 48000);
            CHECK_RANGE (ratio, 0.3, 4.0);

            // All four off: only the vinyl noise (and nothing else) is added to the input.
            if (mask == 0)
            {
                auto noVinyl = p;
                noVinyl.master.vinylAmount = 0.0f;
                noVinyl.engine.flux = 0.0f;
                Buffer c = in;
                Rig r2 (noVinyl);
                r2.run (c, 512);
                CHECK (c.data == in.data);
            }
        }

        // Switching slots on and off while playing: no clicks beyond the signal's own steps.
        auto p = loaded();
        p.engine.flux = 0.0f; p.master.vinylAmount = 0.0f;
        p.texture.degrade = 0.0f;
        Buffer s (2, 3 * 48000);
        fillSine (s, 220.0, 48000.0, 0.4f);
        Rig rig (p);
        std::mt19937 rng (8);
        for (int start = 0; start < s.samples(); start += 2400)
        {
            const auto bits = (unsigned) rng();
            rig.params.textureSlot.enabled = (bits & 1) != 0;
            rig.params.colorSlot.enabled = (bits & 2) != 0;
            rig.params.movementSlot.enabled = (bits & 4) != 0;
            rig.params.spaceSlot.enabled = (bits & 8) != 0;
            rig.chain.process (s.view().getSubBlock (start, std::min (2400, s.samples() - start)), rig.params);
        }
        CHECK (allFinite (s));
        CHECK (maxStep (s.data[0], 0, s.samples()) < 1.5);
    }

    void testBlockSizeIndependence()
    {
        // With the Vintage Engine off (it updates once per host block, by design) the whole
        // plugin gives bit-identical output for any block size, including blocks larger than
        // the one prepared (the Core splits them).
        auto p = loaded();
        p.engine.flux = 0.0f;

        Buffer in (2, 48000);
        fillNoise (in, 55, 0.4f);

        Buffer reference = in;
        { Rig rig (p); rig.run (reference, 48000); }

        for (int blockSize : { 1, 7, 64, 333, 512, 4096 })
        {
            Buffer b = in;
            Rig rig (p);
            rig.run (b, blockSize);
            CHECK (b.data == reference.data);
        }

        // With the engine on the result depends only slightly on the block size.
        auto q = loaded();
        double levels[3];
        int k = 0;
        for (int blockSize : { 64, 512, 4096 })
        {
            Buffer b (2, 5 * 48000);
            fillNoise (b, 56, 0.4f);
            Rig rig (q);
            rig.run (b, blockSize);
            CHECK (allFinite (b));
            levels[k++] = rms (b.data[0], 48000, 5 * 48000);
        }
        CHECK_RANGE (levels[1] / levels[0], 0.7, 1.4);
        CHECK_RANGE (levels[2] / levels[0], 0.7, 1.4);
    }

    void testSampleRatesAndChannels()
    {
        for (double sr : { 22050.0, 44100.0, 48000.0, 96000.0, 192000.0 })
            for (int channels : { 1, 2 })
            {
                Buffer b (channels, (int) (3 * sr));
                fillTonal (b, sr, 0.6f);
                Buffer in = b;
                Rig rig (loaded(), sr, channels);
                rig.run (b, 256);
                CHECK (allFinite (b));
                CHECK (peakOf (b) < 10.0f);
                const double ratio = rms (b.data[0], (int) sr, (int) (3 * sr)) / rms (in.data[0], (int) sr, (int) (3 * sr));
                CHECK_RANGE (ratio, 0.3, 4.0);
            }
    }

    void testLevelAndPhaseAtDefaults()
    {
        // Default settings (no changes by the user), a mono source (identical L and R).
        ChainParameters p;
        p.tempoBpm = 120.0;

        Buffer b (2, 8 * 48000);
        fillTonal (b, 48000.0, 0.6f);
        const Buffer in = b;
        Rig rig (p);
        rig.run (b, 512);
        CHECK (allFinite (b));

        const int from = 2 * 48000, to = 8 * 48000;
        const double inLevel = rms (in.data[0], from, to);
        const double outLevel = 0.5 * (rms (b.data[0], from, to) + rms (b.data[1], from, to));
        std::printf ("  defaults: output level %.2f dB relative to the input\n", 20.0 * std::log10 (outLevel / inLevel));
        CHECK_RANGE (outLevel / inLevel, 0.5, 2.0); // within +-6 dB

        // Mono compatibility: summing L + R must not cancel the signal.
        double sumSq = 0.0, lSq = 0.0, rSq = 0.0, lr = 0.0;
        for (int i = from; i < to; ++i)
        {
            const double l = b.data[0][(std::size_t) i], r = b.data[1][(std::size_t) i];
            sumSq += (l + r) * (l + r); lSq += l * l; rSq += r * r; lr += l * r;
        }
        const double monoRatio = std::sqrt (sumSq) / (std::sqrt (lSq) + std::sqrt (rSq));
        const double correlation = lr / std::sqrt (lSq * rSq);
        std::printf ("  defaults: mono sum keeps %.2f of the stereo level, L/R correlation %.2f\n", monoRatio, correlation);
        CHECK (monoRatio > 0.6); // the stereo effects are wide, but L + R never cancels
    }

    void testMeters()
    {
        // Input and output meters, with a -6 dB Output Gain.
        auto p = neutral();
        p.master.outputGainDb = -6.0206f;
        Rig rig (p);

        Buffer b (2, 3 * 48000);
        for (int i = 0; i < b.samples(); ++i)
        {
            b.data[0][(std::size_t) i] = 0.5f * (float) std::sin (2.0 * pi * 440.0 * i / 48000.0);
            b.data[1][(std::size_t) i] = 0.25f * (float) std::sin (2.0 * pi * 440.0 * i / 48000.0);
        }
        rig.run (b, 512);

        const auto& in = rig.chain.getInputMeter();
        const auto& out = rig.chain.getOutputMeter();
        CHECK_RANGE (in.getPeak (0), 0.48, 0.5);
        CHECK_RANGE (in.getPeak (1), 0.24, 0.25);
        CHECK_RANGE (in.getRms (0), 0.34, 0.36);   // 0.5 / sqrt 2
        CHECK_RANGE (in.getRms (1), 0.17, 0.18);
        CHECK_RANGE (out.getPeak (0), 0.24, 0.25); // after -6 dB
        CHECK_RANGE (out.getRms (0), 0.17, 0.18);
        CHECK_RANGE (LevelMeter::toDecibels (in.getPeak (0)), -6.5, -6.0);
        CHECK (LevelMeter::toDecibels (0.0f) == -100.0f);

        // The result does not depend on the block size.
        Buffer c (2, 3 * 48000);
        for (int i = 0; i < c.samples(); ++i)
        {
            c.data[0][(std::size_t) i] = 0.5f * (float) std::sin (2.0 * pi * 440.0 * i / 48000.0);
            c.data[1][(std::size_t) i] = 0.25f * (float) std::sin (2.0 * pi * 440.0 * i / 48000.0);
        }
        Rig other (p);
        other.run (c, 7);
        CHECK (other.chain.getInputMeter().getPeak (0) == in.getPeak (0));
        CHECK (other.chain.getInputMeter().getRms (1) == in.getRms (1));
        CHECK (other.chain.getOutputMeter().getRms (0) == out.getRms (0));

        // Silence: the peak falls away, the RMS too.
        Buffer silence (2, 8 * 48000);
        rig.run (silence, 512);
        CHECK (in.getPeak (0) < 1.0e-3f && in.getRms (0) < 1.0e-3f);

        // Mono, and a NaN in the input never freezes the meter.
        Buffer mono (1, 48000);
        fillSine (mono, 440.0, 48000.0, 0.5f);
        mono.data[0][100] = std::numeric_limits<float>::quiet_NaN();
        LevelMeter meter;
        meter.prepare (48000.0);
        meter.process (mono.view());
        CHECK (std::isfinite (meter.getPeak (0)) && std::isfinite (meter.getRms (0)));
        CHECK (meter.getPeak (0) > 0.4f);
        CHECK (meter.getPeak (7) == meter.getPeak (1)); // out-of-range channel is clamped, not a crash
    }

    void testRandomAutomationOfEverything()
    {
        std::mt19937 rng (77);
        std::uniform_real_distribution<float> u (0.0f, 1.0f);

        Rig rig (neutral());
        Buffer b (2, 512);
        bool finite = true;
        float peak = 0.0f;

        for (int block = 0; block < 48000 * 40 / 512; ++block)
        {
            auto& p = rig.params;
            p.master.inputGainDb = -12.0f + 18.0f * u (rng);
            p.master.outputGainDb = -24.0f + 30.0f * u (rng);
            p.master.mix = u (rng);
            p.master.vinylAmount = u (rng);
            p.master.bypass = u (rng) < 0.1f;
            p.magnitude = u (rng);
            p.textureSlot = { u (rng) < 0.8f, u (rng) };
            p.colorSlot = { u (rng) < 0.8f, u (rng) };
            p.movementSlot = { u (rng) < 0.8f, u (rng) };
            p.spaceSlot = { u (rng) < 0.8f, u (rng) };
            p.texture = { u (rng), 0.1f + 1.9f * u (rng), u (rng), 4.0f + 11.0f * u (rng), u (rng), u (rng) };
            p.color = { u (rng), (ColorType) (int) (u (rng) * 2.99f), -1.0f + 2.0f * u (rng), u (rng) };
            p.movement = { u (rng), u (rng), u (rng), 0.1f + 14.9f * u (rng), (MovementStyle) (int) (u (rng) * 2.99f) };
            p.space = { u (rng), u (rng), u (rng), u (rng) < 0.5f, u (rng), u (rng), (ReverbType) (int) (u (rng) * 2.99f), u (rng) };
            p.engine = { u (rng) < 0.8f, u (rng), u (rng), u (rng), (EngineCharacter) (int) (u (rng) * 2.99f) };
            p.tempoBpm = u (rng) < 0.2f ? 0.0 : 40.0 + 200.0 * u (rng);

            fillNoise (b, 1000u + (unsigned) block, 0.5f);
            rig.chain.process (b.view(), p);
            finite = finite && allFinite (b);
            peak = std::max (peak, peakOf (b));
        }
        CHECK (finite);
        CHECK (peak < 40.0f); // +24 dB of output gain on top of loud effects is the worst case
    }

    void testLongRunStability()
    {
        // Two minutes of loud noise with the harshest settings: no growth, no NaN.
        auto p = loaded();
        p.space.feedback = 1.0f; p.space.decay = 1.0f; p.space.mix = 1.0f; p.space.delay = 1.0f;
        p.movement.feedback = 1.0f; p.movement.grain = 1.0f;
        p.master.inputGainDb = 6.0f;

        Rig rig (p);
        Buffer b (2, 48000);
        float lateFirst = 0.0f, lateLast = 0.0f;
        bool finite = true;
        for (int s = 0; s < 120; ++s)
        {
            fillNoise (b, 5000u + (unsigned) s, 0.8f);
            rig.run (b, 512);
            finite = finite && allFinite (b);
            const float pk = peakOf (b);
            if (s >= 20 && s < 30) lateFirst = std::max (lateFirst, pk);
            if (s >= 110) lateLast = std::max (lateLast, pk);
        }
        CHECK (finite);
        std::printf ("  peak in seconds 20-30: %.2f, in seconds 110-120: %.2f\n", lateFirst, lateLast);
        CHECK (lateLast < 1.5f * lateFirst + 0.5f); // levels settle; they do not keep growing
        CHECK (lateLast < 30.0f);
    }

    void testResetIsRepeatable()
    {
        auto p = loaded();
        p.engine.flux = 0.0f;
        Buffer in (2, 48000);
        fillNoise (in, 90, 0.4f);

        Rig rig (p);
        Buffer first = in;
        rig.run (first, 512);
        rig.chain.reset();
        Buffer second = in;
        rig.run (second, 512);
        CHECK (first.data == second.data);

        // Engine on too: the drifts restart from the same place after reset(). The modules start
        // from their last parameter values, so the two runs are alike but not bit-identical.
        auto q = loaded();
        Rig rig2 (q);
        Buffer a = in;
        rig2.run (a, 512);
        rig2.chain.reset();
        Buffer b = in;
        rig2.run (b, 512);
        CHECK (allFinite (a) && allFinite (b));
        CHECK_RANGE (rms (b.data[0], 24000, 48000) / rms (a.data[0], 24000, 48000), 0.7, 1.4);
    }

    void testNoAllocationsAndPerformance()
    {
        Rig rig (loaded());
        Buffer b (2, 512);
        fillNoise (b, 1, 0.5f);
        for (int i = 0; i < 20; ++i)
            rig.chain.process (b.view(), rig.params);

        std::mt19937 rng (6);
        std::uniform_real_distribution<float> u (0.0f, 1.0f);

        allocationCount = 0;
        countAllocations = true;
        for (int i = 0; i < 400; ++i)
        {
            rig.params.magnitude = u (rng);
            rig.params.space.type = (ReverbType) (i % 3);
            rig.params.color.type = (ColorType) (i % 3);
            rig.params.movement.style = (MovementStyle) (i % 3);
            rig.params.space.sync = (i % 2) == 0;
            rig.params.tempoBpm = 80.0 + i % 100;
            rig.params.textureSlot.enabled = (i % 7) != 0;
            rig.chain.process (b.view(), rig.params);
        }
        countAllocations = false;
        CHECK (allocationCount.load() == 0);

        Rig steady (loaded());
        const int seconds = 60;
        Buffer big (2, 48000);
        fillNoise (big, 2, 0.5f);
        const auto t0 = std::chrono::steady_clock::now();
        for (int s = 0; s < seconds; ++s)
            steady.run (big, 512);
        const double elapsed = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("  perf: all five modules, %d s of stereo 48 kHz in %.3f s (%.0fx real time, %.2f %% of one core)\n",
                     seconds, elapsed, seconds / elapsed, 100.0 * elapsed / seconds);
        CHECK (elapsed < seconds * 0.25);
    }

    struct TestCase { const char* name; void (*fn)(); };
}

int main()
{
    const TestCase tests[] = {
        { "Neutral settings / bypass / dry = a wire",       testNeutralIsAWire },
        { "Magnitude 0 = a wire (engine at full strength)", testMagnitudeZeroIsAWire },
        { "Magnitude scales the amounts and nothing else",  testMagnitudeScalesAmountsOnly },
        { "Magnitude is monotonic",                         testMagnitudeIsMonotonic },
        { "Magnitude automation is click-free",             testMagnitudeAutomationIsClickFree },
        { "Vintage Engine never switches effects on",       testVintageEngineNeverSwitchesEffectsOn },
        { "All 16 slot combinations",                       testEverySlotCombination },
        { "Block-size independence",                        testBlockSizeIndependence },
        { "Sample rates 22.05-192 kHz, mono and stereo",    testSampleRatesAndChannels },
        { "Level and phase at default settings",            testLevelAndPhaseAtDefaults },
        { "Level meters",                                   testMeters },
        { "Random automation of every parameter (40 s)",    testRandomAutomationOfEverything },
        { "Long run, harshest settings (120 s)",            testLongRunStability },
        { "Reset is repeatable",                            testResetIsRepeatable },
        { "No allocations + CPU",                           testNoAllocationsAndPerformance },
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
