#include "../test_config.h"

#include "MayaFlux/Kinesis/Scalar.hpp"
#include "MayaFlux/Kinesis/Tendency/TimeMap.hpp"

namespace MayaFlux::Test {

TEST(DampAsymmetricTest, RiseAndFallUseTheirOwnTimeConstants)
{
    EXPECT_DOUBLE_EQ(Kinesis::damp_asymmetric(0.0, 1.0, 0.5, 2.0, 0.1), Kinesis::damp(0.0, 1.0, 0.5, 0.1));
    EXPECT_DOUBLE_EQ(Kinesis::damp_asymmetric(1.0, 0.0, 0.5, 2.0, 0.1), Kinesis::damp(1.0, 0.0, 2.0, 0.1));
    EXPECT_DOUBLE_EQ(Kinesis::damp_asymmetric(0.7, 0.7, 0.5, 2.0, 0.1), 0.7);
}

TEST(DampAsymmetricTest, SlowRiseAndFastFallMakeAnAsymmetricSwell)
{
    double level = 0.0;
    for (int n = 0; n < 10; ++n) {
        level = Kinesis::damp_asymmetric(level, 1.0, 2.0, 0.05, 0.01);
    }
    const double risen = level;

    for (int n = 0; n < 10; ++n) {
        level = Kinesis::damp_asymmetric(level, 0.0, 2.0, 0.05, 0.01);
    }

    EXPECT_LT(risen, 0.1);
    EXPECT_LT(level, 0.2 * risen);
}

TEST(DampAsymmetricTest, ArrayFormMovesEachLevelByItsOwnNumbers)
{
    std::vector<double> levels { 0.0, 1.0, 0.5 };
    const std::vector<double> targets { 1.0, 0.0, 0.5 };
    const std::vector<double> rise { 0.5, 0.5, 0.5 };
    const std::vector<double> fall { 2.0, 2.0, 2.0 };

    Kinesis::damp_asymmetric(levels, targets, rise, fall, 0.1);

    EXPECT_DOUBLE_EQ(levels.at(0), Kinesis::damp_asymmetric(0.0, 1.0, 0.5, 2.0, 0.1));
    EXPECT_DOUBLE_EQ(levels.at(1), Kinesis::damp_asymmetric(1.0, 0.0, 0.5, 2.0, 0.1));
    EXPECT_DOUBLE_EQ(levels.at(2), 0.5);
}

TEST(DampAsymmetricTest, ArrayFormStopsAtTheShortestSpan)
{
    std::vector<double> levels { 0.0, 0.0, 0.0 };
    const std::vector<double> targets { 1.0, 1.0 };
    const std::vector<double> rise { 0.5, 0.5, 0.5 };
    const std::vector<double> fall { 2.0, 2.0, 2.0 };

    Kinesis::damp_asymmetric(levels, targets, rise, fall, 0.1);

    EXPECT_GT(levels.at(1), 0.0);
    EXPECT_DOUBLE_EQ(levels.at(2), 0.0);
}

TEST(SampleTableTest, InterpolatesEvenlySpacedEntriesAndClamps)
{
    const std::vector<double> table { 0.0, 10.0, 20.0 };

    EXPECT_DOUBLE_EQ(Kinesis::sample_table(table, 0.0), 0.0);
    EXPECT_DOUBLE_EQ(Kinesis::sample_table(table, 0.25), 5.0);
    EXPECT_DOUBLE_EQ(Kinesis::sample_table(table, 0.5), 10.0);
    EXPECT_DOUBLE_EQ(Kinesis::sample_table(table, 1.0), 20.0);
    EXPECT_DOUBLE_EQ(Kinesis::sample_table(table, -3.0), 0.0);
    EXPECT_DOUBLE_EQ(Kinesis::sample_table(table, 7.0), 20.0);
    EXPECT_DOUBLE_EQ(Kinesis::sample_table(std::vector<double> { 4.0 }, 0.5), 4.0);
    EXPECT_DOUBLE_EQ(Kinesis::sample_table(std::vector<double> {}, 0.5), 0.0);
}

TEST(PiecewiseLinearTimeMapTest, ReadsItsPointsAcrossTheDurationAndHoldsTheLast)
{
    const auto path = Kinesis::TimeMaps::piecewise_linear({ 0.0, 10.0, 20.0 }, 2.0);

    EXPECT_DOUBLE_EQ(path(0.0), 0.0);
    EXPECT_DOUBLE_EQ(path(0.5), 5.0);
    EXPECT_DOUBLE_EQ(path(1.0), 10.0);
    EXPECT_DOUBLE_EQ(path(1.5), 15.0);
    EXPECT_DOUBLE_EQ(path(9.0), 20.0);
    EXPECT_DOUBLE_EQ(path(-1.0), 0.0);

    EXPECT_DOUBLE_EQ(Kinesis::TimeMaps::piecewise_linear({}, 2.0)(1.0), 0.0);
    EXPECT_DOUBLE_EQ(Kinesis::TimeMaps::piecewise_linear({ 7.0 }, 2.0)(1.0), 7.0);
    EXPECT_DOUBLE_EQ(Kinesis::TimeMaps::piecewise_linear({ 1.0, 5.0 }, 0.0)(1.0), 5.0);
}

TEST(SoftClipTest, IdentityForSmallValuesBoundedForLargeOnes)
{
    EXPECT_NEAR(Kinesis::soft_clip(0.01), 0.01, 1e-6);
    EXPECT_NEAR(Kinesis::soft_clip(100.0), 1.0, 1e-12);
    EXPECT_NEAR(Kinesis::soft_clip(100.0, 2.0), 2.0, 1e-12);
    EXPECT_DOUBLE_EQ(Kinesis::soft_clip(-0.4), -Kinesis::soft_clip(0.4));
    EXPECT_DOUBLE_EQ(Kinesis::soft_clip(3.0, 0.0), 0.0);
}

}
