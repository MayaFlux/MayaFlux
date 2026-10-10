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

}
