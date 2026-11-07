// Vintage Maker — Module 5 (VINTAGE ENGINE) tests. Console program, no JUCE.
// Exit code 0 = all checks passed.

#include "DSP/Engine/VintageEngine.h"
#include "DSP/Movement/MovementProcessor.h"
#include "DSP/Space/SpaceProcessor.h"
#include "DSP/Texture/TextureProcessor.h"

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

    [[maybe_unused]] void fillSine (Buffer& b, double freq, double sr, float amp)
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

    [[maybe_unused]] double rms (const std::vector<float>& v, int from, int to)
    {
        double sum = 0.0;
        for (int i = from; i < to; ++i)
            sum += (double) v[(std::size_t) i] * v[(std::size_t) i];
        return std::sqrt (sum / std::max (1, to - from));
    }

    [[maybe_unused]] double maxStep (const std::vector<float>& v, int from, int to)
    {
        double m = 0.0;
        for (int i = std::max (from, 1); i < to; ++i)
            m = std::max (m, (double) std::abs (v[(std::size_t) i] - v[(std::size_t) i - 1]));
        return m;
    }

    using Offsets = VintageEngine::Offsets;

    // Offsets normalised by each destination's range (so every one is in [-1, 1] * amount).
    std::array<float, 9> normalised (const Offsets& o)
    {
        return { o.wowDepth / engine::wowDepthRange, o.wowRate / engine::wowRateRange,
                 o.flutterDepth / engine::flutterDepthRange, o.flutterRate / engine::flutterRateRange,
                 o.vinylAmount / engine::vinylAmountRange, o.drive / engine::driveRange,
                 o.grain / engine::grainRange, o.tremoloRate / engine::tremoloRateRange,
                 o.width / engine::widthRange };
    }

    VintageEngineParameters make (float flux, float rate, float depth, EngineCharacter c)
    {
        VintageEngineParameters p;
        p.flux = flux; p.rate = rate; p.depth = depth; p.character = c;
        return p;
    }

    struct Run
    {
        std::vector<std::array<float, 9>> values;  // one per block
        double blockSeconds = 0.0;
    };

    Run run (const VintageEngineParameters& p, double seconds, double sr = 48000.0, int block = 512)
    {
        VintageEngine e;
        e.prepare (sr);
        e.setParameters (p);

        Run r;
        r.blockSeconds = (double) block / sr;
        const int blocks = (int) (seconds * sr / block);
        r.values.reserve ((std::size_t) blocks);
        for (int i = 0; i < blocks; ++i)
        {
            e.advance (block);
            r.values.push_back (normalised (e.getOffsets()));
        }
        return r;
    }

    double stdev (const Run& r, std::size_t d, std::size_t skip = 0)
    {
        double sum = 0.0, sq = 0.0; std::size_t n = 0;
        for (std::size_t i = skip; i < r.values.size(); ++i)
        {
            const double v = r.values[i][d];
            sum += v; sq += v * v; ++n;
        }
        const double mean = sum / (double) n;
        return std::sqrt (std::max (0.0, sq / (double) n - mean * mean));
    }

    // Average speed of change per second (normalised units).
    double speed (const Run& r, std::size_t d)
    {
        double total = 0.0;
        for (std::size_t i = 1; i < r.values.size(); ++i)
            total += std::abs ((double) r.values[i][d] - r.values[i - 1][d]);
        return total / ((double) (r.values.size() - 1) * r.blockSeconds);
    }

    double maxStepPerBlock (const Run& r)
    {
        double m = 0.0;
        for (std::size_t i = 1; i < r.values.size(); ++i)
            for (std::size_t d = 0; d < 9; ++d)
                m = std::max (m, std::abs ((double) r.values[i][d] - r.values[i - 1][d]));
        return m;
    }

    // ------------------------------------------------------------------ tests

    void testFluxZeroIsTransparent()
    {
        VintageEngine e;
        e.prepare (48000.0);
        e.setParameters (make (0.0f, 1.0f, 1.0f, EngineCharacter::random));

        for (int i = 0; i < 2000; ++i)
            e.advance (512);

        const auto o = e.getOffsets();
        CHECK (o.wowDepth == 0.0f && o.wowRate == 0.0f && o.flutterDepth == 0.0f && o.flutterRate == 0.0f
               && o.vinylAmount == 0.0f && o.drive == 0.0f && o.grain == 0.0f && o.tremoloRate == 0.0f && o.width == 0.0f);

        TextureParameters t; ColorParameters c; MovementParameters m; SpaceParameters s;
        const auto t0 = t; const auto c0 = c; const auto m0 = m; const auto s0 = s;
        e.apply (t); e.apply (c); e.apply (m); e.apply (s);
        CHECK (t == t0 && c == c0 && m == m0 && s == s0);
        CHECK (e.applyToVinylAmount (0.37f) == 0.37f);

        // Depth 0 is off too.
        VintageEngine d;
        d.prepare (48000.0);
        d.setParameters (make (1.0f, 1.0f, 0.0f, EngineCharacter::organic));
        for (int i = 0; i < 500; ++i) d.advance (512);
        CHECK (d.getOffsets().drive == 0.0f && d.getOffsets().wowDepth == 0.0f);

        // Raised and lowered again: it returns exactly to zero.
        VintageEngine f;
        f.prepare (48000.0);
        f.setParameters (make (1.0f, 0.5f, 1.0f, EngineCharacter::organic));
        for (int i = 0; i < 500; ++i) f.advance (512);
        CHECK (f.getOffsets().drive != 0.0f);
        f.setParameters (make (0.0f, 0.5f, 1.0f, EngineCharacter::organic));
        for (int i = 0; i < 500; ++i) f.advance (512);
        CHECK (f.getOffsets().drive == 0.0f && f.getOffsets().width == 0.0f);
    }

    void testBoundedAndMoving()
    {
        for (auto c : { EngineCharacter::slow, EngineCharacter::organic, EngineCharacter::random })
        {
            const auto r = run (make (1.0f, 0.6f, 1.0f, c), 600.0);
            for (std::size_t d = 0; d < 9; ++d)
            {
                float lo = 10.0f, hi = -10.0f;
                for (const auto& v : r.values) { lo = std::min (lo, v[d]); hi = std::max (hi, v[d]); }
                CHECK (lo >= -1.001f && hi <= 1.001f);
                CHECK (hi - lo > 0.6f);   // it really moves, in both directions
                CHECK (stdev (r, d) > 0.1);
            }
        }

        // Half the Flux * Depth = half the excursion, exactly (everything is linear).
        const auto full = run (make (1.0f, 0.4f, 1.0f, EngineCharacter::organic), 120.0);
        const auto half = run (make (0.5f, 0.4f, 1.0f, EngineCharacter::organic), 120.0);
        const auto halfDepth = run (make (1.0f, 0.4f, 0.5f, EngineCharacter::organic), 120.0);
        for (std::size_t d = 0; d < 9; ++d)
        {
            CHECK_RANGE (stdev (half, d) / stdev (full, d), 0.499, 0.501);
            CHECK_RANGE (stdev (halfDepth, d) / stdev (full, d), 0.499, 0.501);
        }
    }

    void testSmoothness()
    {
        // Largest change between two consecutive blocks (of 512 samples at 48 kHz = 10.7 ms), as a
        // fraction of the full range: no jumps, even for the fastest character at the highest Rate.
        for (auto c : { EngineCharacter::slow, EngineCharacter::organic, EngineCharacter::random })
        {
            const auto r = run (make (1.0f, 1.0f, 1.0f, c), 300.0);
            const double step = maxStepPerBlock (r);
            std::printf ("  character %d: largest step per 10.7 ms block = %.4f of the range\n", (int) c, step);
            CHECK (step < 0.06);
        }

        // Switching Character, Rate and Flux abruptly: still no jump.
        VintageEngine e;
        e.prepare (48000.0);
        e.setParameters (make (1.0f, 0.5f, 1.0f, EngineCharacter::slow));
        std::array<float, 9> prev {};
        double worst = 0.0;
        for (int i = 0; i < 6000; ++i)
        {
            if (i % 300 == 0)
                e.setParameters (make ((i / 300) % 2 ? 1.0f : 0.2f, (float) ((i / 300) % 5) / 4.0f, 1.0f, (EngineCharacter) ((i / 300) % 3)));
            e.advance (256);
            const auto now = normalised (e.getOffsets());
            if (i > 0)
                for (std::size_t d = 0; d < 9; ++d)
                    worst = std::max (worst, (double) std::abs (now[d] - prev[d]));
            prev = now;
        }
        CHECK (worst < 0.06);
    }

    void testDeterministicAndReset()
    {
        const auto a = run (make (0.8f, 0.5f, 0.7f, EngineCharacter::organic), 30.0);
        const auto b = run (make (0.8f, 0.5f, 0.7f, EngineCharacter::organic), 30.0);
        CHECK (a.values == b.values);

        VintageEngine e;
        e.prepare (48000.0);
        e.setParameters (make (0.8f, 0.5f, 0.7f, EngineCharacter::organic));
        for (int i = 0; i < 1000; ++i) e.advance (512);
        e.reset();
        e.setParameters (make (0.8f, 0.5f, 0.7f, EngineCharacter::organic));
        std::vector<std::array<float, 9>> again;
        for (int i = 0; i < (int) a.values.size(); ++i)
        {
            e.advance (512);
            again.push_back (normalised (e.getOffsets()));
        }
        CHECK (again == a.values);
    }

    void testCharactersAndRate()
    {
        // Same Rate: Slow < Organic < Random in how fast things change.
        double s[3];
        int k = 0;
        for (auto c : { EngineCharacter::slow, EngineCharacter::organic, EngineCharacter::random })
        {
            const auto r = run (make (1.0f, 0.5f, 1.0f, c), 300.0);
            double sum = 0.0;
            for (std::size_t d = 0; d < 9; ++d) sum += speed (r, d);
            s[k++] = sum / 9.0;
        }
        std::printf ("  average speed of change: slow %.4f, organic %.4f, random %.4f (range per second)\n", s[0], s[1], s[2]);
        CHECK (s[0] < s[1] && s[1] < s[2]);

        // The Rate knob speeds everything up (about x6 from 0 to 100 %: 0.02 Hz to 0.5 Hz is x25 in base speed).
        for (auto c : { EngineCharacter::slow, EngineCharacter::organic, EngineCharacter::random })
        {
            double v[3];
            int j = 0;
            for (float rate : { 0.0f, 0.5f, 1.0f })
            {
                const auto r = run (make (1.0f, rate, 1.0f, c), 600.0);
                double sum = 0.0;
                for (std::size_t d = 0; d < 9; ++d) sum += speed (r, d);
                v[j++] = sum / 9.0;
            }
            CHECK (v[0] < v[1] && v[1] < v[2]);
            CHECK (v[2] / v[0] > 8.0);
        }
    }

    void testIndependentDestinations()
    {
        const auto r = run (make (1.0f, 0.5f, 1.0f, EngineCharacter::organic), 2000.0);
        double worst = 0.0;
        for (std::size_t a = 0; a < 9; ++a)
            for (std::size_t b = a + 1; b < 9; ++b)
            {
                double sab = 0, saa = 0, sbb = 0;
                for (const auto& v : r.values) { sab += v[a] * v[b]; saa += v[a] * v[a]; sbb += v[b] * v[b]; }
                worst = std::max (worst, std::abs (sab / std::sqrt (saa * sbb)));
            }
        std::printf ("  largest correlation between two destinations: %.3f\n", worst);
        CHECK (worst < 0.35);
    }

    void testSampleRateAndBlockSizeIndependence()
    {
        // Statistics (how big, how fast) do not depend on the sample rate or on the block size.
        const auto base = run (make (1.0f, 0.5f, 1.0f, EngineCharacter::organic), 1200.0, 48000.0, 512);
        for (double sr : { 22050.0, 44100.0, 96000.0, 192000.0 })
        {
            const auto r = run (make (1.0f, 0.5f, 1.0f, EngineCharacter::organic), 1200.0, sr, 512);
            double sb = 0, sr2 = 0;
            for (std::size_t d = 0; d < 9; ++d) { sb += stdev (base, d); sr2 += stdev (r, d); }
            CHECK_RANGE (sr2 / sb, 0.85, 1.15);

            double vb = 0, vr = 0;
            for (std::size_t d = 0; d < 9; ++d) { vb += speed (base, d); vr += speed (r, d); }
            CHECK_RANGE (vr / vb, 0.85, 1.15);
        }

        for (int block : { 1, 32, 64, 2048, 8192 })
        {
            const auto r = run (make (1.0f, 0.5f, 1.0f, EngineCharacter::organic), 1200.0, 48000.0, block);
            double sb = 0, sr2 = 0;
            for (std::size_t d = 0; d < 9; ++d) { sb += stdev (base, d); sr2 += stdev (r, d); }
            CHECK_RANGE (sr2 / sb, 0.85, 1.15);
        }
    }

    void testApplyClampsToRanges()
    {
        VintageEngine e;
        e.prepare (48000.0);
        e.setParameters (make (1.0f, 1.0f, 1.0f, EngineCharacter::random));

        std::mt19937 rng (3);
        std::uniform_real_distribution<float> u (0.0f, 1.0f);

        bool ok = true;
        for (int i = 0; i < 20000; ++i)
        {
            e.advance (256);

            // Parameters at the very edges of their ranges.
            TextureParameters t;
            t.wowDepth = u (rng) < 0.5f ? 0.0f : 1.0f;
            t.wowRate = u (rng) < 0.5f ? texture::wowRateMinHz : texture::wowRateMaxHz;
            t.flutterDepth = u (rng) < 0.5f ? 0.0f : 1.0f;
            t.flutterRate = u (rng) < 0.5f ? texture::flutterRateMinHz : texture::flutterRateMaxHz;
            ColorParameters c; c.drive = u (rng) < 0.5f ? 0.0f : 1.0f;
            MovementParameters m; m.grain = u (rng) < 0.5f ? 0.0f : 1.0f;
            m.rateHz = u (rng) < 0.5f ? movement::rateMinHz : movement::rateMaxHz;
            SpaceParameters s; s.width = u (rng) < 0.5f ? 0.0f : 1.0f;
            float vinyl = u (rng) < 0.5f ? 0.0f : 1.0f;

            e.apply (t); e.apply (c); e.apply (m); e.apply (s);
            vinyl = e.applyToVinylAmount (vinyl);

            ok = ok && t.wowDepth >= 0.0f && t.wowDepth <= 1.0f
                    && t.wowRate >= texture::wowRateMinHz && t.wowRate <= texture::wowRateMaxHz
                    && t.flutterDepth >= 0.0f && t.flutterDepth <= 1.0f
                    && t.flutterRate >= texture::flutterRateMinHz && t.flutterRate <= texture::flutterRateMaxHz
                    && c.drive >= 0.0f && c.drive <= 1.0f && m.grain >= 0.0f && m.grain <= 1.0f
                    && m.rateHz >= movement::rateMinHz && m.rateHz <= movement::rateMaxHz
                    && s.width >= 0.0f && s.width <= 1.0f && vinyl >= 0.0f && vinyl <= 1.0f;
        }
        CHECK (ok);

        // It does not touch anything it does not own.
        TextureParameters t; t.vinylDrop = 0.7f; t.degrade = 0.4f;
        MovementParameters m; m.feedback = 0.55f; m.tremolo = 0.35f; m.style = MovementStyle::frozen;
        SpaceParameters s; s.delay = 0.3f; s.time = 0.6f; s.feedback = 0.45f; s.sync = true; s.mix = 0.2f; s.decay = 0.7f; s.type = ReverbType::plate;
        const auto t0 = t; const auto m0 = m; const auto s0 = s;
        e.apply (t); e.apply (m); e.apply (s);
        CHECK (t.vinylDrop == t0.vinylDrop && t.degrade == t0.degrade);
        CHECK (m.feedback == m0.feedback && m.tremolo == m0.tremolo && m.style == m0.style);
        CHECK (s.delay == s0.delay && s.time == s0.time && s.feedback == s0.feedback && s.sync == s0.sync
               && s.mix == s0.mix && s.decay == s0.decay && s.type == s0.type);
    }

    void testInvalidValues()
    {
        const float inf = std::numeric_limits<float>::infinity();
        const float nan = std::numeric_limits<float>::quiet_NaN();

        VintageEngine e;
        e.prepare (48000.0);
        e.setParameters (make (0.5f, 0.5f, 0.5f, EngineCharacter::organic));

        VintageEngineParameters bad;
        bad.flux = nan; bad.rate = inf; bad.depth = -inf; bad.character = (EngineCharacter) 77;
        e.setParameters (bad);
        const auto q = e.getParameters();
        CHECK (q.flux == 0.5f && q.rate == 0.5f && q.depth == 0.5f); // non-finite values keep the previous value
        CHECK (q.character == EngineCharacter::organic);

        VintageEngineParameters wild = make (9.0f, -4.0f, 3.0f, EngineCharacter::random);
        e.setParameters (wild);
        const auto w = e.getParameters();
        CHECK (w.flux == 1.0f && w.rate == 0.0f && w.depth == 1.0f);

        e.advance (0);
        e.advance (-5);
        e.advance (10000000);
        const auto n = normalised (e.getOffsets());
        bool finite = true;
        for (float v : n) finite = finite && std::isfinite (v) && std::abs (v) <= 1.001f;
        CHECK (finite);
    }

    // The modules fed with modulated parameters stay finite and bounded, in every character.
    void testModulatedModulesStayStable()
    {
        for (auto c : { EngineCharacter::slow, EngineCharacter::organic, EngineCharacter::random })
        {
            VintageEngine e;
            e.prepare (48000.0);
            e.setParameters (make (1.0f, 1.0f, 1.0f, c));

            TextureProcessor texture; MovementProcessor movementModule; SpaceProcessor spaceModule;
            const ProcessSpec spec { 48000.0, 256, 2 };
            texture.prepare (spec); movementModule.prepare (spec); spaceModule.prepare (spec);

            Buffer b (2, 256);
            bool finite = true;
            float peak = 0.0f;
            for (int block = 0; block < 48000 * 30 / 256; ++block)
            {
                e.advance (256);

                TextureParameters t; t.wowDepth = 0.8f; t.flutterDepth = 0.8f;
                MovementParameters m; m.grain = 0.8f; m.feedback = 0.8f; m.tremolo = 0.5f;
                SpaceParameters s; s.width = 0.8f; s.delay = 0.5f; s.mix = 0.5f; s.feedback = 0.7f;
                e.apply (t); e.apply (m); e.apply (s);
                texture.setParameters (t); movementModule.setParameters (m); spaceModule.setParameters (s);

                fillNoise (b, (unsigned) block, 0.4f);
                texture.process (b.view());
                movementModule.process (b.view());
                spaceModule.process (b.view());

                finite = finite && allFinite (b);
                for (auto& ch : b.data)
                    for (float x : ch)
                        peak = std::max (peak, std::abs (x));
            }
            CHECK (finite);
            CHECK (peak < 8.0f);
        }
    }

    void testNoAllocationsAndPerformance()
    {
        VintageEngine e;
        e.prepare (48000.0);
        e.setParameters (make (1.0f, 0.7f, 1.0f, EngineCharacter::organic));
        for (int i = 0; i < 20; ++i) e.advance (256);

        TextureParameters t; ColorParameters c; MovementParameters m; SpaceParameters s;

        allocationCount = 0;
        countAllocations = true;
        for (int i = 0; i < 2000; ++i)
        {
            e.setParameters (make (1.0f, (float) (i % 10) / 10.0f, 1.0f, (EngineCharacter) (i % 3)));
            e.advance (256);
            e.apply (t); e.apply (c); e.apply (m); e.apply (s);
            (void) e.applyToVinylAmount (0.2f);
        }
        countAllocations = false;
        CHECK (allocationCount.load() == 0);

        const auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < 48000 * 600 / 256; ++i) // 10 minutes of audio
            e.advance (256);
        const double elapsed = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("  perf: 600 s of audio (256-sample blocks) in %.4f s (%.4f %% of one core)\n", elapsed, 100.0 * elapsed / 600.0);
        CHECK (elapsed < 600.0 * 0.01);
    }

    struct TestCase { const char* name; void (*fn)(); };
}

int main()
{
    const TestCase tests[] = {
        { "Flux 0 / Depth 0 = exactly transparent",   testFluxZeroIsTransparent },
        { "Bounded, moving, linear in Flux and Depth", testBoundedAndMoving },
        { "Smoothness (no jumps)",                    testSmoothness },
        { "Deterministic and reset",                  testDeterministicAndReset },
        { "Characters and Rate",                      testCharactersAndRate },
        { "Independent destinations",                 testIndependentDestinations },
        { "Sample-rate and block-size independence",  testSampleRateAndBlockSizeIndependence },
        { "Apply clamps to each destination's range", testApplyClampsToRanges },
        { "Invalid values",                           testInvalidValues },
        { "Modulated modules stay stable (30 s)",     testModulatedModulesStayStable },
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
