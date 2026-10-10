#include "../test_config.h"

#include "MayaFlux/Kinesis/Discrete/Dynamics.hpp"

namespace MayaFlux::Test {

namespace Discrete = Kinesis::Discrete;

TEST(PulseResponseTest, RisesDuringThePulseAndDecaysAfterIt)
{
    constexpr double onset = 1.0;
    constexpr double duration = 2.0;
    constexpr double rise = 0.5;
    constexpr double fall = 1.0;
    const double peak = 1.0 - std::exp(-duration / rise);

    EXPECT_DOUBLE_EQ(Discrete::pulse_response(0.5, onset, duration, rise, fall), 0.0);
    EXPECT_DOUBLE_EQ(Discrete::pulse_response(1.0, onset, duration, rise, fall), 0.0);
    EXPECT_NEAR(Discrete::pulse_response(2.0, onset, duration, rise, fall), 1.0 - std::exp(-1.0 / rise), 1e-12);
    EXPECT_NEAR(Discrete::pulse_response(3.0, onset, duration, rise, fall), peak, 1e-12);
    EXPECT_NEAR(Discrete::pulse_response(4.0, onset, duration, rise, fall), peak * std::exp(-1.0), 1e-12);
}

TEST(PulseResponseTest, NonPositiveConstantsAreInstant)
{
    EXPECT_DOUBLE_EQ(Discrete::pulse_response(1.0, 1.0, 2.0, 0.0, 1.0), 1.0);
    EXPECT_DOUBLE_EQ(Discrete::pulse_response(3.5, 1.0, 2.0, 0.5, 0.0), 0.0);
}

TEST(PulseResponseTest, ARepeatStartsFromRestEveryPeriod)
{
    const double first = Discrete::pulse_response(0.5, 0.0, 1.0, 0.5, 0.5, 4.0);

    EXPECT_GT(first, 0.0);
    EXPECT_NEAR(Discrete::pulse_response(4.5, 0.0, 1.0, 0.5, 0.5, 4.0), first, 1e-12);
    EXPECT_NEAR(Discrete::pulse_response(8.5, 0.0, 1.0, 0.5, 0.5, 4.0), first, 1e-12);
    EXPECT_DOUBLE_EQ(Discrete::pulse_response(-1.0, 0.0, 1.0, 0.5, 0.5, 4.0), 0.0);
}

TEST(PulseResponseTest, DurationIsHeldToThePeriod)
{
    EXPECT_DOUBLE_EQ(
        Discrete::pulse_response(3.9, 0.0, 10.0, 0.5, 0.5, 4.0),
        Discrete::pulse_response(3.9, 0.0, 4.0, 0.5, 0.5, 4.0));
}

TEST(PulseResponseTest, ArrayEntriesAreIndependentAndAlikeNumbersGiveAlikeLevels)
{
    const std::vector<double> onset { 0.0, 0.0, 1.0 };
    const std::vector<double> duration { 1.0, 1.0, 1.0 };
    const std::vector<double> rise { 0.5, 0.5, 0.5 };
    const std::vector<double> fall { 0.5, 0.5, 0.5 };
    const std::vector<double> period { 0.0, 0.0, 0.0 };
    std::vector<double> levels(3, -1.0);

    Discrete::pulse_response(levels, 0.5, onset, duration, rise, fall, period);

    EXPECT_DOUBLE_EQ(levels.at(0), levels.at(1));
    EXPECT_GT(levels.at(0), 0.0);
    EXPECT_DOUBLE_EQ(levels.at(2), 0.0);
}

TEST(PulseResponseTest, ArrayFormStopsAtTheShortestSpan)
{
    const std::vector<double> one { 0.0 };
    const std::vector<double> two { 0.0, 0.0 };
    std::vector<double> levels(2, -1.0);

    Discrete::pulse_response(levels, 0.5, one, two, two, two, two);

    EXPECT_GE(levels.at(0), 0.0);
    EXPECT_DOUBLE_EQ(levels.at(1), -1.0);
}

TEST(LotkaVolterraTest, OneLevelGrowsToItsCarryingCapacity)
{
    std::vector<double> levels { 0.1 };
    const std::vector<double> growth { 1.0 };
    const std::vector<double> interaction { 0.5 };

    for (int n = 0; n < 5000; ++n) {
        Discrete::lotka_volterra_step(levels, growth, interaction, 0.01);
    }

    EXPECT_NEAR(levels.at(0), 2.0, 1e-3);
}

TEST(LotkaVolterraTest, StrongCrossSuppressionLetsTheLargerLevelWin)
{
    std::vector<double> levels { 0.5, 0.2 };
    const std::vector<double> growth { 1.0, 1.0 };
    const std::vector<double> interaction { 1.0, 2.0, 2.0, 1.0 };

    for (int n = 0; n < 20000; ++n) {
        Discrete::lotka_volterra_step(levels, growth, interaction, 0.01);
    }

    EXPECT_GT(levels.at(0), 0.99);
    EXPECT_LT(levels.at(1), 0.01);
}

TEST(LotkaVolterraTest, EqualNumbersStayEqual)
{
    std::vector<double> levels { 0.3, 0.3, 0.3 };
    const std::vector<double> growth { 1.0, 1.0, 1.0 };
    const std::vector<double> interaction { 1.0, 2.0, 2.0, 2.0, 1.0, 2.0, 2.0, 2.0, 1.0 };

    for (int n = 0; n < 50; ++n) {
        Discrete::lotka_volterra_step(levels, growth, interaction, 0.01);
    }

    EXPECT_DOUBLE_EQ(levels.at(0), levels.at(1));
    EXPECT_DOUBLE_EQ(levels.at(1), levels.at(2));
}

TEST(LotkaVolterraTest, ZeroStaysZeroAndALevelNeverGoesNegative)
{
    std::vector<double> levels { 0.0, 5.0 };
    const std::vector<double> growth { 1.0, 1.0 };
    const std::vector<double> interaction { 1.0, 0.0, 0.0, 10.0 };

    Discrete::lotka_volterra_step(levels, growth, interaction, 1.0);

    EXPECT_DOUBLE_EQ(levels.at(0), 0.0);
    EXPECT_GE(levels.at(1), 0.0);
}

TEST(LotkaVolterraTest, AMatrixSmallerThanNByNLeavesTheLevelsAlone)
{
    std::vector<double> levels { 0.4, 0.6 };
    const std::vector<double> growth { 1.0, 1.0 };
    const std::vector<double> interaction { 1.0, 0.0, 0.0 };

    Discrete::lotka_volterra_step(levels, growth, interaction, 0.1);

    EXPECT_DOUBLE_EQ(levels.at(0), 0.4);
    EXPECT_DOUBLE_EQ(levels.at(1), 0.6);
}

}
