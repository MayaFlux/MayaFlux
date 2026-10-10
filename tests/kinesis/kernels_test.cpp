#include "../test_config.h"

#include "MayaFlux/Kinesis/Discrete/Coefficients.hpp"
#include "MayaFlux/Kinesis/Discrete/Kernels.hpp"
#include "MayaFlux/Nodes/Generators/Polynomial.hpp"

namespace MayaFlux::Test {

namespace Discrete = Kinesis::Discrete;

namespace {

    std::span<const double> view(const std::vector<double>& values)
    {
        return values;
    }

}

TEST(KernelTest, WeightedSumUsesTheShorterOfWindowAndWeights)
{
    EXPECT_DOUBLE_EQ(Discrete::weighted_sum(view({ 1.0, 2.0, 3.0 }), view({ 1.0, 0.5 })), 2.0);
    EXPECT_DOUBLE_EQ(Discrete::weighted_sum(view({}), view({ 1.0 })), 0.0);
}

TEST(KernelTest, WeightedSumWithDifferenceWeightsIsADerivative)
{
    const std::vector<double> squares { 4.0, 1.0, 0.0 };
    const auto weights = Discrete::backward_difference_weights(2);

    EXPECT_DOUBLE_EQ(Discrete::weighted_sum(view(squares), view(weights)), 2.0);
}

TEST(KernelTest, TappedSumReadsPairsAndInterpolatesFractionalLags)
{
    const std::vector<double> window { 10.0, 20.0, 30.0, 40.0 };
    const std::vector<double> taps { 0.0, 1.0, 2.0, 0.5, 1.5, 2.0, 9.0, 5.0, -1.0, 3.0 };

    EXPECT_DOUBLE_EQ(Discrete::tapped_sum(view(window), view(taps)), 10.0 + 15.0 + 50.0);
}

TEST(KernelTest, TableLookupInterpolatesAndClampsOverMinusOneToOne)
{
    const std::vector<double> ramp { 0.0, 1.0 };
    EXPECT_DOUBLE_EQ(Discrete::table_lookup(view({ 0.0 }), view(ramp)), 0.5);
    EXPECT_DOUBLE_EQ(Discrete::table_lookup(view({ 5.0 }), view(ramp)), 1.0);
    EXPECT_DOUBLE_EQ(Discrete::table_lookup(view({ -5.0 }), view(ramp)), 0.0);

    const std::vector<double> three { 2.0, 4.0, 6.0 };
    EXPECT_DOUBLE_EQ(Discrete::table_lookup(view({ 0.5 }), view(three)), 5.0);

    EXPECT_DOUBLE_EQ(Discrete::table_lookup(view({ 0.3 }), view({ 7.0 })), 7.0);
    EXPECT_DOUBLE_EQ(Discrete::table_lookup(view({ 0.3 }), view({})), 0.0);
}

TEST(KernelTest, TableLookupPreviousIndexesByTheOlderValue)
{
    const std::vector<double> ramp { 0.0, 1.0 };

    EXPECT_DOUBLE_EQ(Discrete::table_lookup_previous(view({ 1.0, -1.0 }), view(ramp)), 0.0);
    EXPECT_DOUBLE_EQ(Discrete::table_lookup_previous(view({ -1.0, 1.0 }), view(ramp)), 1.0);
    EXPECT_DOUBLE_EQ(Discrete::table_lookup_previous(view({ 0.7 }), view(ramp)), 0.5);
}

TEST(KernelTest, ChebyshevSeriesMatchesTheClosedForms)
{
    EXPECT_NEAR(Discrete::chebyshev_series(view({ 0.5 }), view({ 0.0, 0.0, 1.0 })), -0.5, 1e-12);
    EXPECT_NEAR(Discrete::chebyshev_series(view({ 0.5 }), view({ 0.0, 0.0, 0.0, 1.0 })), -1.0, 1e-12);
    EXPECT_NEAR(Discrete::chebyshev_series(view({ 0.5 }), view({ 1.0, 2.0, 3.0 })), 0.5, 1e-12);
    EXPECT_DOUBLE_EQ(Discrete::chebyshev_series(view({ 0.5 }), view({})), 0.0);
}

TEST(KernelTest, ChebyshevSeriesTurnsACosineIntoItsHarmonic)
{
    const std::vector<double> fifth { 0.0, 0.0, 0.0, 0.0, 0.0, 1.0 };

    for (const double t : { 0.1, 0.7, 2.0, 3.0 }) {
        const std::vector<double> window { std::cos(t) };
        EXPECT_NEAR(Discrete::chebyshev_series(view(window), view(fifth)), std::cos(5.0 * t), 1e-9) << "t=" << t;
    }
}

TEST(KernelTest, IteratedPolynomialEvaluatesThePreviousValue)
{
    constexpr double r = 3.2;
    const std::vector<double> logistic { -r, r, 0.0 };

    EXPECT_NEAR(Discrete::iterated_polynomial(view({ 0.0, 0.5 }), view(logistic)), 0.25 * r, 1e-12);
    EXPECT_DOUBLE_EQ(Discrete::horner(view({ 2.0, 3.0, 1.0 }), 2.0), 15.0);
}

TEST(KernelTest, PhasorBankAdvancesEveryPartialAndWritesItsPhaseBack)
{
    auto state = Discrete::phasor_bank_state(view({ std::numbers::pi / 2.0 }));

    const std::vector<double> none;
    EXPECT_NEAR(Discrete::phasor_bank(view(none), state), 0.5, 1e-12);
    EXPECT_NEAR(state.at(4), std::numbers::pi / 2.0, 1e-12);
    EXPECT_NEAR(Discrete::phasor_bank(view(none), state), 0.0, 1e-12);
    EXPECT_NEAR(Discrete::phasor_bank(view(none), state), -0.5, 1e-12);
    EXPECT_NEAR(Discrete::phasor_bank(view(none), state), 0.0, 1e-12);
}

TEST(KernelTest, PhasorBankPartialsWithEqualNumbersAddAndUnequalNumbersDoNot)
{
    const std::vector<double> none;
    const double step = Discrete::radians_per_sample(440.0, 48000.0);

    auto single = Discrete::phasor_bank_state(view({ step }));
    auto aligned = Discrete::phasor_bank_state(view({ step, step }));
    auto apart = Discrete::phasor_bank_state(view({ step, step }), {}, {}, view({ 0.0, std::numbers::pi }));

    for (int n = 0; n < 50; ++n) {
        const double one = Discrete::phasor_bank(view(none), single);
        EXPECT_NEAR(Discrete::phasor_bank(view(none), aligned), 2.0 * one, 1e-12) << "n=" << n;
        EXPECT_NEAR(Discrete::phasor_bank(view(none), apart), 0.0, 1e-12) << "n=" << n;
    }
}

TEST(KernelTest, PhasorBankIgnoresAnArrayShorterThanItsCount)
{
    std::vector<double> state { 3.0, 0.1, 0.1 };

    EXPECT_DOUBLE_EQ(Discrete::phasor_bank(view({}), state), 0.0);
    EXPECT_DOUBLE_EQ(state.at(1), 0.1);
}

TEST(KernelTest, BuilderFillsMissingColumnsWithNeutralNumbers)
{
    const auto state = Discrete::phasor_bank_state(view({ 0.1, 0.2 }));

    ASSERT_EQ(state.size(), 11U);
    EXPECT_DOUBLE_EQ(state.at(0), 2.0);
    EXPECT_DOUBLE_EQ(state.at(3), 1.0);
    EXPECT_DOUBLE_EQ(state.at(4), 1.0);
    EXPECT_DOUBLE_EQ(state.at(5), 0.0);
    EXPECT_DOUBLE_EQ(state.at(9), 0.0);
}

TEST(KernelTest, KernelsAreCompleteNodesInAPolynomial)
{
    using Nodes::Generator::Polynomial;
    using Nodes::Generator::PolynomialMode;

    auto fir = std::make_shared<Polynomial>(
        Discrete::weighted_sum, PolynomialMode::FEEDFORWARD, 2, std::vector<double> { 0.7, 0.3 });

    EXPECT_DOUBLE_EQ(fir->process_sample(1.0), 0.7);
    EXPECT_DOUBLE_EQ(fir->process_sample(2.0), 1.7);
    EXPECT_DOUBLE_EQ(fir->process_sample(3.0), 2.7);

    auto bank = std::make_shared<Polynomial>(
        Discrete::phasor_bank, PolynomialMode::FEEDFORWARD, 1,
        Discrete::phasor_bank_state(view({ std::numbers::pi / 2.0 })));

    EXPECT_NEAR(bank->process_sample(0.0), 0.5, 1e-12);
    EXPECT_NEAR(bank->process_sample(0.0), 0.0, 1e-12);
    EXPECT_NEAR(bank->process_sample(0.0), -0.5, 1e-12);
}

TEST(KernelTest, PolynomialLagsEvaluatesAHenonStep)
{
    constexpr double a = 1.4;
    constexpr double b = 0.3;
    const std::vector<double> henon { 3.0, 2.0, 1.0, 0.0, 0.0, 0.0, 0.0, -a, 0.0, b, 0.0 };

    EXPECT_NEAR(Discrete::polynomial_lags(view({ 0.0, 0.1, 0.2 }), view(henon)), 1.0 - a * 0.01 + b * 0.2, 1e-12);
}

TEST(KernelTest, PolynomialLagsSkipsLagsBeyondTheWindowAndShortArrays)
{
    const std::vector<double> two_lags { 2.0, 1.0, 1.0, 2.0, 5.0, 3.0 };

    EXPECT_DOUBLE_EQ(Discrete::polynomial_lags(view({ 4.0 }), view(two_lags)), 1.0 + 2.0 * 4.0);
    EXPECT_DOUBLE_EQ(Discrete::polynomial_lags(view({ 4.0, 2.0 }), view({ 2.0, 2.0, 1.0 })), 0.0);
}

TEST(KernelTest, FourierSeriesSumsCosineAndSineHarmonics)
{
    constexpr double t = 0.8;

    EXPECT_NEAR(Discrete::fourier_series(view({ t }), view({ 0.5, 1.0, 0.0, 0.0, 0.25 })),
        0.5 + std::cos(t) + 0.25 * std::sin(2.0 * t), 1e-12);
    EXPECT_NEAR(Discrete::fourier_series(view({ t }), view({ 0.0, 0.0, 0.0, 2.0 })), 2.0 * std::cos(2.0 * t), 1e-12);
    EXPECT_NEAR(Discrete::fourier_series(view({ t }), view({ 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 3.0 })),
        std::cos(3.0 * t) + 3.0 * std::sin(4.0 * t), 1e-12);
    EXPECT_DOUBLE_EQ(Discrete::fourier_series(view({ t }), view({})), 0.0);
}

TEST(KernelTest, BreakpointCurveInterpolatesAndHoldsAtTheEnds)
{
    const std::vector<double> points { 0.0, 0.0, 1.0, 10.0, 3.0, 20.0 };

    EXPECT_DOUBLE_EQ(Discrete::breakpoint_curve(view({ -1.0 }), view(points)), 0.0);
    EXPECT_DOUBLE_EQ(Discrete::breakpoint_curve(view({ 0.5 }), view(points)), 5.0);
    EXPECT_DOUBLE_EQ(Discrete::breakpoint_curve(view({ 2.0 }), view(points)), 15.0);
    EXPECT_DOUBLE_EQ(Discrete::breakpoint_curve(view({ 5.0 }), view(points)), 20.0);
    EXPECT_DOUBLE_EQ(Discrete::breakpoint_curve(view({ 0.5 }), view({})), 0.0);
}

TEST(KernelTest, PatternLookupReadsATableByTheShapeOfTheWindow)
{
    const std::vector<double> coefs { 2.0, 2.0, -1.0, 1.0, 10.0, 20.0, 30.0, 40.0 };

    EXPECT_DOUBLE_EQ(Discrete::pattern_lookup(view({ -0.5, -0.5 }), view(coefs)), 10.0);
    EXPECT_DOUBLE_EQ(Discrete::pattern_lookup(view({ 0.5, -0.5 }), view(coefs)), 20.0);
    EXPECT_DOUBLE_EQ(Discrete::pattern_lookup(view({ -0.5, 0.5 }), view(coefs)), 30.0);
    EXPECT_DOUBLE_EQ(Discrete::pattern_lookup(view({ 0.5, 0.5 }), view(coefs)), 40.0);
    EXPECT_DOUBLE_EQ(Discrete::pattern_lookup(view({ 0.5, 0.5 }), view({ 2.0, 3.0, -1.0, 1.0, 1.0, 2.0, 3.0 })), 0.0);
    EXPECT_DOUBLE_EQ(Discrete::pattern_lookup(view({ 0.5, 0.5 }), view({ 2.0, 2.0, 1.0, 1.0, 10.0, 20.0, 30.0, 40.0 })), 0.0);
}

TEST(KernelTest, PatternLookupQuantisesOverItsOwnBounds)
{
    const std::vector<double> coefs { 2.0, 2.0, 0.0, 10.0, 10.0, 20.0, 30.0, 40.0 };

    EXPECT_DOUBLE_EQ(Discrete::pattern_lookup(view({ 2.0, 2.0 }), view(coefs)), 10.0);
    EXPECT_DOUBLE_EQ(Discrete::pattern_lookup(view({ 8.0, 2.0 }), view(coefs)), 20.0);
    EXPECT_DOUBLE_EQ(Discrete::pattern_lookup(view({ 2.0, 8.0 }), view(coefs)), 30.0);
    EXPECT_DOUBLE_EQ(Discrete::pattern_lookup(view({ 50.0, 50.0 }), view(coefs)), 40.0);
}

TEST(KernelTest, DilateAndErodeTakeTheExtremeOfWindowPlusOrMinusOffsets)
{
    const std::vector<double> window { 1.0, 2.0, 3.0 };
    const std::vector<double> offsets { 0.0, 1.0, -1.0 };

    EXPECT_DOUBLE_EQ(Discrete::dilate(view(window), view(offsets)), 3.0);
    EXPECT_DOUBLE_EQ(Discrete::erode(view(window), view(offsets)), 1.0);
    EXPECT_DOUBLE_EQ(Discrete::dilate(view({}), view(offsets)), 0.0);
}

TEST(KernelTest, QuantileInterpolatesBetweenRanksWithoutDisturbingTheWindow)
{
    const std::vector<double> window { 5.0, 1.0, 3.0, 2.0, 4.0 };

    EXPECT_DOUBLE_EQ(Discrete::quantile(view(window), view({})), 3.0);
    EXPECT_DOUBLE_EQ(Discrete::quantile(view(window), view({ 0.0 })), 1.0);
    EXPECT_DOUBLE_EQ(Discrete::quantile(view(window), view({ 1.0 })), 5.0);
    EXPECT_DOUBLE_EQ(Discrete::quantile(view(window), view({ 0.25 })), 2.0);
    EXPECT_DOUBLE_EQ(Discrete::quantile(view(window), view({ 0.375 })), 2.5);
    EXPECT_EQ(window, (std::vector<double> { 5.0, 1.0, 3.0, 2.0, 4.0 }));
}

TEST(KernelTest, ZscoreMeasuresTheNewestAgainstTheOlderWindow)
{
    EXPECT_NEAR(Discrete::zscore(view({ 10.0, 1.0, 2.0, 3.0, 4.0, 5.0 }), view({})), 7.0 / std::sqrt(2.5), 1e-12);
    EXPECT_NEAR(Discrete::zscore(view({ 5.0, 1.0, 1.0, 1.0 }), view({ 1.0 })), 4.0, 1e-12);
    EXPECT_DOUBLE_EQ(Discrete::zscore(view({ 5.0, 1.0 }), view({})), 0.0);
}

TEST(KernelTest, GoertzelMeasuresTheSquaredAmplitudeAtAFrequency)
{
    std::vector<double> window(64);
    for (size_t n = 0; n < window.size(); ++n) {
        window.at(n) = 0.5 * std::sin(2.0 * std::numbers::pi * 4.0 * static_cast<double>(n) / 64.0);
    }

    EXPECT_NEAR(Discrete::goertzel(view(window), view({ 4.0 / 64.0, 1.0 })), 0.25, 1e-9);
    EXPECT_NEAR(Discrete::goertzel(view(window), view({ 4.0 / 64.0, 2.0 })), 0.5, 1e-9);
    EXPECT_NEAR(Discrete::goertzel(view(window), view({ 10.0 / 64.0, 1.0 })), 0.0, 1e-9);
}

TEST(KernelTest, ModularRecurrenceWrapsTheWeightedSum)
{
    EXPECT_DOUBLE_EQ(Discrete::modular_recurrence(view({ 7.0, 8.0 }), view({ 10.0, 1.0, 1.0 })), 5.0);
    EXPECT_DOUBLE_EQ(Discrete::modular_recurrence(view({ -3.0, 0.0 }), view({ 10.0, 1.0, 0.0 })), 7.0);
    EXPECT_DOUBLE_EQ(Discrete::modular_recurrence(view({ 7.0, 8.0 }), view({ 0.0, 1.0, 1.0 })), 15.0);
}

TEST(KernelTest, KuramotoAdvancesEachOscillatorByItsOwnIncrementWhenUncoupled)
{
    auto state = Discrete::kuramoto_state(view({ 0.1, 0.2, 0.3 }), 0.0);
    const std::vector<double> none;

    const double mix = Discrete::kuramoto(view(none), state);

    EXPECT_NEAR(mix, (std::sin(0.1) + std::sin(0.2) + std::sin(0.3)) / 3.0, 1e-12);
    EXPECT_NEAR(state.at(5), 0.1, 1e-12);
    EXPECT_NEAR(state.at(7), 0.3, 1e-12);
}

TEST(KernelTest, KuramotoCouplingPullsSpreadPhasesTogether)
{
    auto state = Discrete::kuramoto_state(view({ 0.0, 0.0, 0.0 }), 1.0, view({ 0.0, 0.5, -0.5 }));
    const std::vector<double> none;

    for (int n = 0; n < 300; ++n) {
        Discrete::kuramoto(view(none), state);
    }

    double cos_sum = 0.0;
    double sin_sum = 0.0;
    for (size_t i = 0; i < 3; ++i) {
        cos_sum += std::cos(state.at(5 + i));
        sin_sum += std::sin(state.at(5 + i));
    }
    EXPECT_GT(std::hypot(cos_sum, sin_sum) / 3.0, 0.9999);
}

TEST(KernelTest, CoupledMapLatticeKeepsUniformCellsUniformAndMapsThem)
{
    auto state = Discrete::coupled_map_lattice_state(view({ 0.3, 0.3, 0.3, 0.3 }), 3.9, 0.4);
    const std::vector<double> none;

    const double out = Discrete::coupled_map_lattice(view(none), state);
    const double mapped = 3.9 * 0.3 * 0.7;

    EXPECT_NEAR(out, mapped - 0.5, 1e-12);
    for (size_t i = 0; i < 4; ++i) {
        EXPECT_NEAR(state.at(3 + i), mapped, 1e-12);
    }
}

TEST(KernelTest, CoupledMapLatticeWithNoCouplingIteratesEachCellAlone)
{
    auto state = Discrete::coupled_map_lattice_state(view({ 0.2, 0.6 }), 3.7, 0.0);
    const std::vector<double> none;

    Discrete::coupled_map_lattice(view(none), state);

    EXPECT_NEAR(state.at(3), 3.7 * 0.2 * 0.8, 1e-12);
    EXPECT_NEAR(state.at(4), 3.7 * 0.6 * 0.4, 1e-12);
}

TEST(KernelTest, LorenzStaysBoundedAndIsSensitiveToItsStart)
{
    auto base = Discrete::lorenz_state();
    auto nudged = Discrete::lorenz_state(0.005, 10.0, 28.0, 8.0 / 3.0, 1.0 + 1e-9);
    const std::vector<double> none;

    double lo = 1e9;
    double hi = -1e9;
    double apart = 0.0;
    for (int n = 0; n < 8000; ++n) {
        const double x = Discrete::lorenz_attractor(view(none), base);
        const double y = Discrete::lorenz_attractor(view(none), nudged);
        lo = std::min(lo, x);
        hi = std::max(hi, x);
        apart = std::max(apart, std::abs(x - y));
    }

    EXPECT_LT(hi, 40.0);
    EXPECT_GT(lo, -40.0);
    EXPECT_GT(hi - lo, 10.0);
    EXPECT_GT(apart, 1.0);
}

TEST(KernelTest, StepSequenceAdvancesOnEachRisingEdgeAndWraps)
{
    auto state = Discrete::step_sequence_state(view({ 1.0, 2.0, 3.0 }));

    const auto read = [&state](double trigger) {
        return Discrete::step_sequence(view({ trigger }), state);
    };

    EXPECT_DOUBLE_EQ(read(0.0), 1.0);
    EXPECT_DOUBLE_EQ(read(1.0), 2.0);
    EXPECT_DOUBLE_EQ(read(1.0), 2.0);
    EXPECT_DOUBLE_EQ(read(0.0), 2.0);
    EXPECT_DOUBLE_EQ(read(1.0), 3.0);
    EXPECT_DOUBLE_EQ(read(0.0), 3.0);
    EXPECT_DOUBLE_EQ(read(1.0), 1.0);
}

TEST(KernelTest, StepSequenceStartsLatchedSoAHighInputDoesNotAdvanceImmediately)
{
    auto state = Discrete::step_sequence_state(view({ 1.0, 2.0 }));

    EXPECT_DOUBLE_EQ(Discrete::step_sequence(view({ 1.0 }), state), 1.0);
    EXPECT_DOUBLE_EQ(Discrete::step_sequence(view({ 1.0 }), state), 1.0);
}

TEST(KernelTest, RotorBankFollowsTheSineOfTheAccumulatedPhase)
{
    const std::vector<double> increments { 0.1, 0.37 };
    const std::vector<double> scales { 1.0, 0.5 };
    auto state = Discrete::rotor_bank_state(view(increments), view(scales));
    const std::vector<double> none;

    for (int n = 1; n <= 20000; ++n) {
        const double out = Discrete::rotor_bank(view(none), state);
        const double expected = std::sin(0.1 * n) + 0.5 * std::sin(0.37 * n);
        ASSERT_NEAR(out, expected, 1e-9) << "n=" << n;
    }
}

TEST(KernelTest, RotorBankStartsFromItsPhasesAndKeepsItsAmplitude)
{
    auto state = Discrete::rotor_bank_state(view({ 0.2 }), {}, view({ std::numbers::pi / 2.0 }));
    const std::vector<double> none;

    EXPECT_NEAR(Discrete::rotor_bank(view(none), state), std::sin(std::numbers::pi / 2.0 + 0.2), 1e-12);

    for (int n = 0; n < 200000; ++n) {
        Discrete::rotor_bank(view(none), state);
    }
    EXPECT_NEAR(state.at(4) * state.at(4) + state.at(5) * state.at(5), 1.0, 1e-9);
}

TEST(KernelTest, RotorBankEntriesWithEqualNumbersAdd)
{
    auto single = Discrete::rotor_bank_state(view({ 0.3 }));
    auto doubled = Discrete::rotor_bank_state(view({ 0.3, 0.3 }));
    const std::vector<double> none;

    for (int n = 0; n < 50; ++n) {
        EXPECT_NEAR(Discrete::rotor_bank(view(none), doubled), 2.0 * Discrete::rotor_bank(view(none), single), 1e-12);
    }
}

TEST(KernelTest, RotorBankIgnoresAnArrayShorterThanItsCount)
{
    std::vector<double> state { 4.0, 1.0, 0.0 };

    EXPECT_DOUBLE_EQ(Discrete::rotor_bank(view({}), state), 0.0);
}

}
