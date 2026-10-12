#include "../test_config.h"

#include "MayaFlux/Kinesis/Discrete/Series.hpp"

namespace MayaFlux::Test {

namespace Discrete = Kinesis::Discrete;

TEST(SeriesTest, HarmonicSeriesScalesWithItsBase)
{
    EXPECT_EQ(Discrete::harmonic_series(3), (std::vector<double> { 1.0, 2.0, 3.0 }));
    EXPECT_EQ(Discrete::harmonic_series(3, 2.0), (std::vector<double> { 2.0, 4.0, 6.0 }));
    EXPECT_TRUE(Discrete::harmonic_series(0).empty());
}

TEST(SeriesTest, StretchedSeriesIsHarmonicAtZeroAndPulledOutwardOtherwise)
{
    EXPECT_EQ(Discrete::stretched_series(4, 0.0), Discrete::harmonic_series(4));

    const auto stretched = Discrete::stretched_series(4, 0.0001);
    for (size_t i = 0; i < stretched.size(); ++i) {
        const auto n = static_cast<double>(i + 1);
        EXPECT_DOUBLE_EQ(stretched.at(i), n * std::sqrt(1.0 + 0.0001 * n * n));
        EXPECT_GE(stretched.at(i), n);
    }
}

TEST(SeriesTest, FreeBarSeriesMatchesTheKnownRatios)
{
    const auto bar = Discrete::free_bar_series(5);
    const std::vector<double> known { 1.0, 2.756, 5.404, 8.933, 13.344 };

    ASSERT_EQ(bar.size(), 5U);
    EXPECT_DOUBLE_EQ(bar.front(), 1.0);
    for (size_t i = 0; i < bar.size(); ++i) {
        EXPECT_NEAR(bar.at(i), known.at(i), 1e-3) << "i=" << i;
    }
}

TEST(SeriesTest, FreeBarSeriesAscendsAndApproachesItsAsymptote)
{
    const auto bar = Discrete::free_bar_series(40, 100.0);

    for (size_t i = 1; i < bar.size(); ++i) {
        EXPECT_GT(bar.at(i), bar.at(i - 1));
    }

    const double first_root = 4.730040744862704;
    const double order = 40.0;
    const double asymptote = std::pow((2.0 * order + 1.0) * std::numbers::pi * 0.5 / first_root, 2.0);
    EXPECT_NEAR(bar.back() / 100.0, asymptote, 1e-6 * asymptote);
    EXPECT_TRUE(Discrete::free_bar_series(0).empty());
}

TEST(SeriesTest, PowerSeriesStepsByTheRatio)
{
    const double phi = (1.0 + std::sqrt(5.0)) / 2.0;
    const auto terms = Discrete::power_series(3, phi, 0.22, 432.0);

    ASSERT_EQ(terms.size(), 3U);
    EXPECT_DOUBLE_EQ(terms.at(0), 432.0 * std::pow(phi, 0.22));
    EXPECT_DOUBLE_EQ(terms.at(2), 432.0 * std::pow(phi, 0.66));
}

TEST(SeriesTest, SplitPairsGivesAnAscendingPairForEachValue)
{
    const std::vector<double> values { 100.0, 200.0 };

    const auto pairs = Discrete::split_pairs(values, 0.01);

    ASSERT_EQ(pairs.size(), 4U);
    EXPECT_DOUBLE_EQ(pairs.at(0), 99.0);
    EXPECT_DOUBLE_EQ(pairs.at(1), 101.0);
    EXPECT_DOUBLE_EQ(pairs.at(2), 198.0);
    EXPECT_DOUBLE_EQ(pairs.at(3), 202.0);
    EXPECT_TRUE(Discrete::split_pairs({}, 0.01).empty());
}

}
