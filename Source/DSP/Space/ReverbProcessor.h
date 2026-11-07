#pragma once

#include "Core/AudioBufferView.h"
#include "Core/SmoothedValue.h"
#include "SpaceParameters.h"

#include <array>
#include <vector>

namespace vm
{
    // Stereo reverb: input high-pass -> pre-delay -> 4 input diffusers per channel
    // -> 8-line feedback delay network (Hadamard mixing, per-line damping and a
    // slow delay modulation against metallic ringing).
    //
    // Room, Plate and Hall are three tunings of the same network. Switching Type
    // crossfades between engines (50 ms), so it never clicks. Returns the wet
    // signal only; SpaceProcessor adds it to the dry signal.
    class ReverbProcessor
    {
    public:
        ReverbProcessor();

        void prepare (const ProcessSpec& spec);
        void reset() noexcept;

        void setParams (float decay, ReverbType type) noexcept;
        void process (float inL, float inR, float& wetL, float& wetR) noexcept;

    private:
        static constexpr int numLines = 8;
        static constexpr int numDiffusers = 4;
        static constexpr int gainUpdateInterval = 32;

        struct Ring
        {
            std::vector<float> data;
            int mask = 0;
            int write = 0;

            void prepare (int minSize);
            void clear() noexcept;
            void push (float x) noexcept { data[(std::size_t) write] = x; write = (write + 1) & mask; }
            // delay in samples >= 1
            float read (float delay) const noexcept;
            float readInt (int delay) const noexcept { return data[(std::size_t) ((write - delay) & mask)]; }
        };

        struct Engine
        {
            void prepare (double sampleRate, ReverbType type);
            void clear() noexcept;
            void updateGains (float rt60) noexcept;
            void process (float inL, float inR, float& outL, float& outR) noexcept;

            double sampleRate = 44100.0;
            ReverbType type = ReverbType::hall;
            space::TypeConfig config = space::typeConfig (ReverbType::hall);

            Ring preDelay[2];
            int preDelaySamples = 0;
            Ring diffuser[2][numDiffusers];
            int diffuserSamples[numDiffusers] {};
            float hpState[2] {};
            float hpCoef = 0.0f;

            Ring line[numLines];
            float lineSamples[numLines] {};
            float lineSeconds[numLines] {};
            float damp[numLines] {};
            float lowpass[numLines] {};
            float gain[numLines] {};
            float lfoPhase[numLines] {};
            float lfoStep[numLines] {};
            float modDepth = 0.0f;
            float dampCoef = 0.0f;
            float outputScale = 1.0f;
            float outputBase = 0.6f;
            float dampMidGain = 1.0f;
        };

        double sampleRate = 44100.0;
        std::array<Engine, 3> engines;
        std::array<float, 3> engineGain {};
        std::array<bool, 3> running {};
        int active = 2;
        float fadeStep = 0.01f;

        LinearSmoothedValue decay;
        ReverbType type = ReverbType::hall;
        int gainCountdown = 0;
    };
}
