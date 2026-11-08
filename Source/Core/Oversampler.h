#pragma once

#include <array>
#include <cmath>
#include <cstddef>

namespace vm
{
    // Oversampling for the non-linear processes (OFF, 2x, 4x), built from
    // two-path polyphase allpass half-band filters (elliptic design):
    //
    //   H(z) = 0.5 * ( A0(z^2) + z^-1 * A1(z^2) )
    //
    // Each branch is a cascade of first-order allpass sections, so the filters are
    // cheap, have no reported latency and keep exact unity gain at DC. Their phase is
    // not linear (a group delay of a couple of samples), which is irrelevant for a
    // saturator whose output replaces the signal.
    //
    //   sharp stage   (1x <-> 2x): 6 sections, passband to 0.42 fs, >= 94 dB rejection
    //                  above 0.58 fs
    //   relaxed stage (2x <-> 4x): 3 sections, passband to 0.56 fs(1x), >= 81 dB rejection
    //                  above 1.44 fs(1x)
    //
    // Coefficients come from the elliptic prototype whose poles lie on the imaginary
    // axis (the half-band condition); see Tests/OversamplingTests.cpp for the measured
    // responses.

    struct SharpHalfbandCoefficients
    {
        static constexpr std::array<double, 3> branch0 { 0.0453621646108123, 0.3371496893925452, 0.7080641376803771 };
        static constexpr std::array<double, 3> branch1 { 0.16808748209753996, 0.5223778559113387, 0.8974455917308244 };
    };

    struct RelaxedHalfbandCoefficients
    {
        static constexpr std::array<double, 2> branch0 { 0.07638269358725608, 0.69878248872245 };
        static constexpr std::array<double, 1> branch1 { 0.30208200381369493 };
    };

    template <typename Coefficients>
    class HalfbandStage
    {
    public:
        void reset() noexcept
        {
            state0 = {};
            state1 = {};
            previousOdd = 0.0;
        }

        // One input sample at the low rate -> two samples at the double rate (even, odd).
        void upsample (double x, double& even, double& odd) noexcept
        {
            even = runBranch (Coefficients::branch0, state0, x);
            odd = runBranch (Coefficients::branch1, state1, x);
        }

        // Two samples at the high rate (even phase first) -> one sample at the low rate.
        double downsample (double even, double odd) noexcept
        {
            const double y = 0.5 * (runBranch (Coefficients::branch0, state0, even)
                                    + runBranch (Coefficients::branch1, state1, previousOdd));
            previousOdd = odd;
            return y;
        }

    private:
        template <std::size_t N>
        static double runBranch (const std::array<double, N>& coefficients, std::array<double, N>& state, double x) noexcept
        {
            // Transposed first-order allpass: y = a*x + s;  s = x - a*y   =>  (a + z^-1) / (1 + a z^-1)
            for (std::size_t i = 0; i < N; ++i)
            {
                const double a = coefficients[i];
                const double y = a * x + state[i];
                double s = x - a * y;

                // No denormals in the feedback state (silence after loud input).
                if (std::abs (s) < 1.0e-30)
                    s = 0.0;

                state[i] = s;
                x = y;
            }

            return x;
        }

        std::array<double, Coefficients::branch0.size()> state0 {};
        std::array<double, Coefficients::branch1.size()> state1 {};
        double previousOdd = 0.0;
    };

    // One channel. factor: 1 (off), 2 or 4. The non-linear function is applied at the
    // oversampled rate between the interpolation and the decimation filters.
    class Oversampler
    {
    public:
        void reset() noexcept
        {
            up1.reset();
            down1.reset();
            up2.reset();
            down2.reset();
        }

        template <typename Function>
        double process (double input, int factor, Function&& nonlinear) noexcept
        {
            if (factor < 2)
                return nonlinear (input);

            double a0, a1;
            up1.upsample (input, a0, a1);

            if (factor < 4)
            {
                const double y0 = nonlinear (a0);
                const double y1 = nonlinear (a1);
                return down1.downsample (y0, y1);
            }

            double b0, b1, b2, b3;
            up2.upsample (a0, b0, b1);
            up2.upsample (a1, b2, b3);

            const double c0 = nonlinear (b0);
            const double c1 = nonlinear (b1);
            const double c2 = nonlinear (b2);
            const double c3 = nonlinear (b3);

            const double d0 = down2.downsample (c0, c1);
            const double d1 = down2.downsample (c2, c3);
            return down1.downsample (d0, d1);
        }

    private:
        HalfbandStage<SharpHalfbandCoefficients> up1, down1;
        HalfbandStage<RelaxedHalfbandCoefficients> up2, down2;
    };
}
