// Vintage Maker — Module 0 Core tests. Console program, no JUCE, no framework.
// Exit code 0 = all checks passed.

#include "Core/MasterProcessor.h"
#include "DSP/Test/TestProcessor.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <limits>
#include <memory>
#include <new>
#include <random>
#include <vector>

// ---------------------------------------------------------------------------
// Allocation counter: every global new is counted while `countAllocations` is
// set, so we can prove process() never allocates.
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Minimal test harness
// ---------------------------------------------------------------------------
static int failures = 0;
static int checks = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        ++checks;                                                                \
        if (! (cond)) {                                                          \
            ++failures;                                                          \
            std::printf ("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);       \
        }                                                                        \
    } while (false)

#define CHECK_NEAR(a, b, tol)                                                    \
    do {                                                                         \
        ++checks;                                                                \
        const double va_ = (a), vb_ = (b);                                       \
        if (! (std::abs (va_ - vb_) <= (tol))) {                                 \
            ++failures;                                                          \
            std::printf ("  FAIL %s:%d  %s = %.9g, expected %.9g (tol %g)\n",    \
                         __FILE__, __LINE__, #a, va_, vb_, (double) (tol));      \
        }                                                                        \
    } while (false)

using namespace vm;
using Slot = AudioEngine::Slot;

namespace
{
    constexpr Slot allSlots[] = { Slot::texture, Slot::color, Slot::movement, Slot::space };

    // Planar multichannel buffer for tests.
    struct Buffer
    {
        Buffer (int channels, int samples)
            : data ((size_t) channels, std::vector<float> ((size_t) samples, 0.0f))
        {
            rebuildPointers();
        }

        Buffer (const Buffer& other) : data (other.data) { rebuildPointers(); }

        Buffer& operator= (const Buffer& other)
        {
            data = other.data;
            rebuildPointers();
            return *this;
        }

        void rebuildPointers()
        {
            pointers.clear();
            for (auto& c : data)
                pointers.push_back (c.data());
        }

        int channels() const { return (int) data.size(); }
        int samples() const { return data.empty() ? 0 : (int) data[0].size(); }
        AudioBufferView view() { return { pointers.data(), channels(), samples() }; }
        float& at (int ch, int i) { return data[(size_t) ch][(size_t) i]; }
        float at (int ch, int i) const { return data[(size_t) ch][(size_t) i]; }

        std::vector<std::vector<float>> data;
        std::vector<float*> pointers;
    };

    void fillSine (Buffer& b, double freq, double sampleRate, float amp = 0.5f)
    {
        for (int ch = 0; ch < b.channels(); ++ch)
            for (int i = 0; i < b.samples(); ++i)
                b.at (ch, i) = amp * (float) std::sin (2.0 * 3.14159265358979 * freq * i / sampleRate + ch * 0.5);
    }

    void fillNoise (Buffer& b, unsigned seed, float amp = 0.5f)
    {
        std::mt19937 rng (seed);
        std::uniform_real_distribution<float> dist (-amp, amp);

        for (auto& c : b.data)
            for (auto& s : c)
                s = dist (rng);
    }

    bool allFinite (const Buffer& b)
    {
        for (const auto& c : b.data)
            for (float s : c)
                if (! std::isfinite (s))
                    return false;
        return true;
    }

    double maxAbsDiff (const Buffer& a, const Buffer& b)
    {
        double m = 0.0;
        for (int ch = 0; ch < a.channels(); ++ch)
            for (int i = 0; i < a.samples(); ++i)
                m = std::max (m, (double) std::abs (a.at (ch, i) - b.at (ch, i)));
        return m;
    }

    double maxStep (const Buffer& b)
    {
        double m = 0.0;
        for (int ch = 0; ch < b.channels(); ++ch)
            for (int i = 1; i < b.samples(); ++i)
                m = std::max (m, (double) std::abs (b.at (ch, i) - b.at (ch, i - 1)));
        return m;
    }

    bool bitIdentical (const Buffer& a, const Buffer& b)
    {
        return a.data == b.data;
    }

    struct Rig
    {
        explicit Rig (double sampleRate = 48000.0, int maxBlock = 512, int channels = 2)
        {
            for (auto s : allSlots)
                master.getEngine().setModule (s, std::make_unique<TestProcessor>());

            spec.sampleRate = sampleRate;
            spec.maxBlockSize = maxBlock;
            spec.numChannels = channels;
            master.prepare (spec);
        }

        TestProcessor& test (Slot s) { return *static_cast<TestProcessor*> (master.getEngine().getModule (s)); }

        // Processes the whole buffer in blocks of `blockSize`, calling
        // `perBlock` (block index) before each block to change parameters.
        void run (Buffer& b, int blockSize, const std::function<void (int)>& perBlock = {})
        {
            int index = 0;
            for (int start = 0; start < b.samples(); start += blockSize, ++index)
            {
                if (perBlock)
                    perBlock (index);

                master.setParameters (params);
                master.process (b.view().getSubBlock (start, std::min (blockSize, b.samples() - start)));
            }
        }

        MasterProcessor master;
        MasterParameters params;
        ProcessSpec spec;
    };

    // Module that records the order in which slots run (for chain-order test).
    struct OrderProbe final : ModuleProcessor
    {
        OrderProbe (int idIn, std::vector<int>& logIn) : id (idIn), log (logIn) {}
        void prepare (const ProcessSpec&) override {}
        void reset() noexcept override { ++resets; }
        void process (AudioBufferView) noexcept override { if (log.size() < log.capacity()) log.push_back (id); }
        const char* getName() const noexcept override { return "Probe"; }

        int id;
        std::vector<int>& log;
        int resets = 0;
    };

    // Module with fixed latency, to check latency reporting.
    struct LatencyProbe final : ModuleProcessor
    {
        explicit LatencyProbe (int l) : latency (l) {}
        void prepare (const ProcessSpec&) override {}
        void reset() noexcept override {}
        void process (AudioBufferView) noexcept override {}
        int getLatencySamples() const noexcept override { return latency; }
        const char* getName() const noexcept override { return "Latency"; }
        int latency;
    };

    // Module that adds a DC offset of `value` (to test chain order with non-commuting operations).
    struct AddProbe final : ModuleProcessor
    {
        explicit AddProbe (float v) : value (v) {}
        void prepare (const ProcessSpec&) override {}
        void reset() noexcept override {}
        void process (AudioBufferView b) noexcept override
        {
            for (int ch = 0; ch < b.getNumChannels(); ++ch)
                for (int i = 0; i < b.getNumSamples(); ++i)
                    b.getChannel (ch)[i] = b.getChannel (ch)[i] * 2.0f + value;
        }
        const char* getName() const noexcept override { return "Add"; }
        float value;
    };

    // ------------------------------------------------------------------ tests

    void testSmoothedValue()
    {
        LinearSmoothedValue v;
        v.prepare (1000.0, 0.01); // 10 samples
        v.setCurrentAndTarget (0.0f);
        v.setTarget (1.0f);
        CHECK (v.isSmoothing());

        float last = 0.0f;
        for (int i = 0; i < 10; ++i)
        {
            const float x = v.getNext();
            CHECK (x > last);
            last = x;
        }
        CHECK (last == 1.0f);
        CHECK (! v.isSmoothing());

        v.setTarget (std::nanf (""));
        CHECK (v.getTarget() == 1.0f); // non-finite targets are ignored

        v.setTarget (0.0f);
        v.skip (4);
        CHECK_NEAR (v.getCurrent(), 0.6, 1e-6);
        v.skip (100);
        CHECK (v.getCurrent() == 0.0f && ! v.isSmoothing());
    }

    void testSilence()
    {
        Rig rig;
        rig.params.inputGainDb = 24.0f;
        rig.params.outputGainDb = 24.0f;
        rig.params.mix = 0.5f;

        Buffer b (2, 48000);
        rig.run (b, 256);

        bool silent = true;
        for (auto& c : b.data)
            for (float s : c)
                silent = silent && s == 0.0f;
        CHECK (silent);
    }

    void testDefaultIsBitTransparent()
    {
        for (int channels : { 1, 2 })
        {
            Rig rig (48000.0, 512, channels);
            Buffer in (channels, 48000), out (channels, 48000);
            fillNoise (in, 1);
            out = in;

            rig.run (out, 333);
            CHECK (bitIdentical (in, out));
        }
    }

    void testGains()
    {
        Rig rig;
        rig.params.inputGainDb = 6.0f;
        rig.params.outputGainDb = -12.0f;

        Buffer in (2, 48000), out (2, 48000);
        fillSine (in, 440.0, 48000.0);
        out = in;
        rig.run (out, 512);

        // Parameters are applied from the first block: after prepare() the
        // smoothers sit at defaults, so the first 20 ms ramp. Check after that.
        const double expected = decibelsToGain (6.0f) * decibelsToGain (-12.0f);
        for (int i = 2000; i < 48000; i += 997)
            CHECK_NEAR (out.at (0, i), in.at (0, i) * expected, 1e-5);

        CHECK (maxStep (out) < 0.05); // ramp, not a jump
    }

    void testChainOrder()
    {
        std::vector<int> log;
        log.reserve (64);

        MasterProcessor master;
        auto& engine = master.getEngine();
        for (int i = 0; i < 4; ++i)
            engine.setModule (allSlots[i], std::make_unique<OrderProbe> (i, log));

        master.prepare ({ 48000.0, 64, 2 });
        Buffer b (2, 64);
        master.process (b.view());

        CHECK ((log == std::vector<int> { 0, 1, 2, 3 }));

        // Non-commuting operations (y = 2x + v) in slot order: texture first.
        MasterProcessor m2;
        m2.getEngine().setModule (Slot::texture, std::make_unique<AddProbe> (1.0f));
        m2.getEngine().setModule (Slot::space, std::make_unique<AddProbe> (10.0f));
        m2.prepare ({ 48000.0, 64, 1 });
        Buffer one (1, 8);
        m2.process (one.view());
        CHECK_NEAR (one.at (0, 4), (0.0 * 2 + 1) * 2 + 10, 1e-6); // = 12, reversed order would give 21
    }

    void testMasterMix()
    {
        Rig rig;
        rig.test (Slot::color).setParameters ({ 6.0f, true }); // wet = -2x (polarity + gain)
        rig.params.mix = 0.0f;

        Buffer in (2, 24000), out (2, 24000);
        fillSine (in, 220.0, 48000.0);
        out = in;
        rig.master.reset(); // start already at the targets
        rig.run (out, 128);

        // mix 0: smoothing starts from mix=1 after prepare, so check the tail.
        for (int i = 4000; i < 24000; i += 501)
            CHECK_NEAR (out.at (1, i), in.at (1, i), 1e-6);

        // mix 0.5: y = x + 0.5 * (wet - x)
        rig.params.mix = 0.5f;
        out = in;
        rig.run (out, 128);
        const double wetGain = -decibelsToGain (6.0f);
        for (int i = 4000; i < 24000; i += 501)
            CHECK_NEAR (out.at (0, i), in.at (0, i) * (1.0 + 0.5 * (wetGain - 1.0)), 1e-5);
    }

    void testMasterBypass()
    {
        Rig rig;
        rig.test (Slot::texture).setParameters ({ 12.0f, false });
        rig.params.outputGainDb = 6.0f;

        Buffer in (2, 96000), out (2, 96000);
        fillSine (in, 100.0, 48000.0);
        out = in;

        // Toggle bypass on at 0.5 s and off at 1.5 s.
        rig.run (out, 480, [&] (int block)
        {
            rig.params.bypass = (block >= 50 && block < 150);
        });

        CHECK (allFinite (out));

        // Fully bypassed region is the input, bit for bit.
        bool exact = true;
        for (int i = 30000; i < 70000; ++i)
            exact = exact && out.at (0, i) == in.at (0, i) && out.at (1, i) == in.at (1, i);
        CHECK (exact);

        // Click check: largest step must stay close to the processed signal's
        // own largest step (gain ~ +18 dB on a 0.5 amp, 100 Hz sine).
        const double processedSlope = 0.5 * decibelsToGain (18.0f) * 2.0 * 3.14159265 * 100.0 / 48000.0;
        CHECK (maxStep (out) < processedSlope * 1.5);
    }

    void testSlotBypassAndMix()
    {
        Rig rig;
        auto& slot = rig.master.getEngine().getSlot (Slot::movement);
        rig.test (Slot::movement).setParameters ({ 0.0f, true }); // polarity flip

        Buffer in (2, 48000), out (2, 48000);
        fillSine (in, 300.0, 48000.0);

        // Slot mix 50 % of a polarity flip = silence.
        slot.setMix (0.5f);
        rig.master.reset();
        out = in;
        rig.run (out, 256);
        for (int i = 0; i < 48000; i += 777)
            CHECK_NEAR (out.at (0, i), 0.0, 1e-6);

        // Slot bypass -> input passes untouched (after the fade).
        slot.setMix (1.0f);
        slot.setEnabled (false);
        out = in;
        rig.run (out, 256);
        bool exact = true;
        for (int i = 4800; i < 48000; ++i)
            exact = exact && out.at (0, i) == in.at (0, i);
        CHECK (exact);
        CHECK (maxStep (out) < 0.5 * 2.0 * 3.14159265 * 300.0 / 48000.0 * 1.5);

        // Re-enabling resets the module once.
        std::vector<int> log;
        log.reserve (16);
        MasterProcessor m;
        m.getEngine().setModule (Slot::texture, std::make_unique<OrderProbe> (0, log));
        m.prepare ({ 48000.0, 512, 2 });
        auto* probe = static_cast<OrderProbe*> (m.getEngine().getModule (Slot::texture));
        const int resetsBefore = probe->resets;
        Buffer b (2, 512);
        m.getEngine().getSlot (Slot::texture).setEnabled (false);
        for (int k = 0; k < 10; ++k) m.process (b.view());
        m.getEngine().getSlot (Slot::texture).setEnabled (true);
        m.process (b.view());
        m.process (b.view());
        CHECK (probe->resets == resetsBefore + 1);
    }

    void testBlockSizeIndependence()
    {
        Buffer in (2, 20000);
        fillNoise (in, 7);

        auto render = [&] (int blockSize)
        {
            Rig rig (48000.0, 512);
            rig.test (Slot::color).setParameters ({ -3.0f, false });
            rig.params = { 4.0f, -2.0f, 0.7f, false };
            Buffer out = in;
            rig.run (out, blockSize);
            return out;
        };

        const Buffer reference = render (64);

        // 4096 > maxBlockSize (512): exercises internal chunking.
        for (int bs : { 1, 7, 32, 100, 512, 513, 4096 })
        {
            const Buffer out = render (bs);
            CHECK (maxAbsDiff (reference, out) < 1e-6);
        }
    }

    void testSampleRates()
    {
        for (double sr : { 22050.0, 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
        {
            Rig rig (sr, 1024);
            rig.params.outputGainDb = -6.0f;

            const int n = (int) sr; // 1 s
            Buffer b (2, n);
            fillSine (b, 1000.0, sr);
            Buffer in = b;
            rig.run (b, 1024);

            CHECK (allFinite (b));

            // Ramp must last ~20 ms at every rate.
            const int ramp = (int) std::lround (sr * 0.02);
            const int probe = ramp + 64;
            CHECK_NEAR (b.at (0, probe), in.at (0, probe) * decibelsToGain (-6.0f), 1e-5);
        }
    }

    void testExtremesAndAutomation()
    {
        Rig rig;
        for (auto s : allSlots)
            rig.test (s).setParameters ({ 24.0f, false });

        Buffer b (2, 96000);
        fillNoise (b, 3, 1.0f);

        std::mt19937 rng (11);
        std::uniform_real_distribution<float> u (0.0f, 1.0f);

        // Every block: random parameters, including out-of-range values.
        rig.run (b, 64, [&] (int)
        {
            rig.params.inputGainDb = -100.0f + 200.0f * u (rng);
            rig.params.outputGainDb = -100.0f + 200.0f * u (rng);
            rig.params.mix = -1.0f + 3.0f * u (rng);
            rig.params.bypass = u (rng) < 0.2f;
            rig.master.getEngine().getSlot (Slot::color).setEnabled (u (rng) > 0.3f);
            rig.master.getEngine().getSlot (Slot::space).setMix (u (rng));
        });

        CHECK (allFinite (b));

        const auto p = rig.master.getParameters();
        CHECK (p.inputGainDb >= -24.0f && p.inputGainDb <= 24.0f);
        CHECK (p.mix >= 0.0f && p.mix <= 1.0f);

        // NaN parameters must not reach the audio.
        Rig nanRig;
        nanRig.params = { std::nanf (""), std::numeric_limits<float>::infinity(), std::nanf (""), false };
        Buffer c (2, 4096);
        fillSine (c, 500.0, 48000.0);
        nanRig.run (c, 256);
        CHECK (allFinite (c));
    }

    void testEdgeCases()
    {
        MasterProcessor unprepared;
        Buffer b (2, 16);
        fillNoise (b, 5);
        Buffer copy = b;
        unprepared.process (b.view()); // must be a no-op
        CHECK (bitIdentical (b, copy));

        Rig rig;
        AudioBufferView empty;
        rig.master.process (empty);
        rig.master.process (b.view().getSubBlock (0, 0));
        CHECK (true);

        // Engine with empty slots (no module) is transparent.
        MasterProcessor bare;
        bare.prepare ({ 44100.0, 128, 2 });
        Buffer x (2, 1000);
        fillNoise (x, 9);
        Buffer y = x;
        bare.process (y.view());
        CHECK (bitIdentical (x, y));

        // Re-prepare with a different format (host changes sample rate / block size).
        rig.master.prepare ({ 96000.0, 2048, 1 });
        Buffer mono (1, 5000);
        fillSine (mono, 50.0, 96000.0);
        rig.master.process (mono.view());
        CHECK (allFinite (mono));
    }

    void testLatencyReporting()
    {
        MasterProcessor m;
        m.getEngine().setModule (Slot::color, std::make_unique<LatencyProbe> (32));
        m.getEngine().setModule (Slot::space, std::make_unique<LatencyProbe> (100));
        m.prepare ({ 48000.0, 256, 2 });
        CHECK (m.getLatencySamples() == 132);
    }

    void testNoAllocationInProcess()
    {
        Rig rig;
        Buffer b (2, 48000);
        fillNoise (b, 2);

        allocationCount = 0;
        countAllocations = true;

        std::mt19937 rng (4);
        std::uniform_real_distribution<float> u (0.0f, 1.0f);
        int index = 0;
        for (int start = 0; start + 2000 <= b.samples(); start += 2000, ++index)
        {
            rig.params = { 12.0f * u (rng), -6.0f * u (rng), u (rng), index % 5 == 0 };
            rig.master.getEngine().getSlot (Slot::movement).setEnabled (index % 3 != 0);
            rig.master.setParameters (rig.params);
            rig.master.process (b.view().getSubBlock (start, 2000)); // > maxBlock: chunked
        }

        countAllocations = false;
        CHECK (allocationCount.load() == 0);
    }

    void testPerformance()
    {
        Rig rig (48000.0, 512);
        rig.params = { 3.0f, -3.0f, 0.6f, false };
        rig.master.getEngine().getSlot (Slot::color).setMix (0.5f);

        const int seconds = 60;
        Buffer b (2, 512);
        fillNoise (b, 8);

        const auto t0 = std::chrono::steady_clock::now();
        for (int k = 0; k < seconds * 48000 / 512; ++k)
        {
            rig.master.setParameters (rig.params);
            rig.master.process (b.view());
        }
        const auto t1 = std::chrono::steady_clock::now();

        const double elapsed = std::chrono::duration<double> (t1 - t0).count();
        std::printf ("  perf: %d s of stereo 48 kHz in %.4f s (%.0fx real time, %.4f %% of one core)\n",
                     seconds, elapsed, seconds / elapsed, 100.0 * elapsed / seconds);
        CHECK (allFinite (b));
        CHECK (elapsed < seconds * 0.05); // Core overhead must stay below 5 % of one core
    }

    struct TestCase
    {
        const char* name;
        void (*fn)();
    };
}

int main()
{
    const TestCase tests[] = {
        { "SmoothedValue",            testSmoothedValue },
        { "Silence in -> silence out", testSilence },
        { "Defaults bit-transparent (mono + stereo)", testDefaultIsBitTransparent },
        { "Input / output gain",       testGains },
        { "Chain order",               testChainOrder },
        { "Master dry/wet",            testMasterMix },
        { "Master bypass (exact, click-free)", testMasterBypass },
        { "Slot bypass and dry/wet",   testSlotBypassAndMix },
        { "Block size independence",   testBlockSizeIndependence },
        { "Sample rates 22.05k-192k",  testSampleRates },
        { "Extremes, automation, NaN/Inf", testExtremesAndAutomation },
        { "Edge cases",                testEdgeCases },
        { "Latency reporting",         testLatencyReporting },
        { "No allocation in process()", testNoAllocationInProcess },
        { "Performance",               testPerformance },
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
