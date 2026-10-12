#include "../test_config.h"

#include "MayaFlux/Kinesis/Discrete/Coefficients.hpp"

namespace MayaFlux::Test {

namespace Discrete = Kinesis::Discrete;

namespace {

    constexpr double sample_rate = 48000.0;

}

TEST(BiquadByValueTest, MatchesTheVectorForm)
{
    for (const auto& [frequency, q] : std::vector<std::pair<double, double>> { { 800.0, 16.0 }, { 2500.0, 55.0 }, { 270.0, 7.0 }, { 1000.0, 0.7 } }) {
        std::vector<double> a;
        std::vector<double> b;
        Discrete::biquad_bandpass(frequency, q, sample_rate, a, b);
        const auto biquad = Discrete::biquad_bandpass(frequency, q, sample_rate);

        EXPECT_DOUBLE_EQ(biquad.a1, a.at(1)) << "f=" << frequency;
        EXPECT_DOUBLE_EQ(biquad.a2, a.at(2)) << "f=" << frequency;
        EXPECT_DOUBLE_EQ(biquad.b0, b.at(0)) << "f=" << frequency;
        EXPECT_DOUBLE_EQ(biquad.b1, b.at(1)) << "f=" << frequency;
        EXPECT_DOUBLE_EQ(biquad.b2, b.at(2)) << "f=" << frequency;
    }
}

TEST(DecayTest, DecayPerSampleIsTheBareExponentialWithNoFloorOrCap)
{
    EXPECT_DOUBLE_EQ(Discrete::decay_per_sample(0.5, sample_rate), std::exp(-1.0 / (0.5 * sample_rate)));
    EXPECT_DOUBLE_EQ(Discrete::decay_per_sample(0.0, sample_rate), 0.0);
    EXPECT_DOUBLE_EQ(Discrete::decay_per_sample(1e12, sample_rate), 1.0);
    EXPECT_DOUBLE_EQ(Discrete::pole_radius_from_decay(0.5, sample_rate), Discrete::decay_per_sample(0.5, sample_rate));
}

TEST(DecayTest, PoleRadiusAndDecayAreInverse)
{
    for (const double seconds : { 0.001, 0.05, 1.0, 30.0 }) {
        const double radius = Discrete::pole_radius_from_decay(seconds, sample_rate);
        EXPECT_LT(radius, 1.0);
        EXPECT_NEAR(Discrete::decay_from_pole_radius(radius, sample_rate), seconds, 1e-6 * seconds);
    }
}

TEST(DecayTest, RadiusStaysInsideTheUnitCircleForAnyRingTime)
{
    EXPECT_LT(Discrete::pole_radius_from_decay(1e12, sample_rate), 1.0);
    EXPECT_GT(Discrete::pole_radius_from_decay(0.0, sample_rate), 0.0);
    EXPECT_DOUBLE_EQ(Discrete::decay_from_pole_radius(0.0, sample_rate), 0.0);
}

TEST(DecayTest, QFromDecayGivesASectionWithThatPoleRadius)
{
    constexpr double frequency = 1000.0;
    for (const double seconds : { 0.01, 0.2, 2.0 }) {
        const double q = Discrete::q_from_decay(frequency, seconds, sample_rate);

        std::vector<double> a;
        std::vector<double> b;
        Discrete::biquad_bandpass(frequency, q, sample_rate, a, b);

        EXPECT_NEAR(Discrete::max_pole_magnitude(a), Discrete::pole_radius_from_decay(seconds, sample_rate), 1e-9) << "seconds=" << seconds;
    }
}

TEST(DecayTest, QAndDecayRoundTripAndLongRingsExceedTheOldQCeiling)
{
    constexpr double frequency = 1000.0;

    const double q = Discrete::q_from_decay(frequency, 2.0, sample_rate);
    EXPECT_GT(q, 1000.0);
    EXPECT_NEAR(Discrete::decay_from_q(frequency, q, sample_rate), 2.0, 1e-6);

    EXPECT_GT(Discrete::decay_from_q(frequency, 90.0, sample_rate), Discrete::decay_from_q(frequency, 30.0, sample_rate));
}

TEST(WeightsTest, BackwardDifferenceWeightsAreSignedBinomials)
{
    EXPECT_EQ(Discrete::backward_difference_weights(1), (std::vector<double> { 1.0, -1.0 }));
    EXPECT_EQ(Discrete::backward_difference_weights(2), (std::vector<double> { 1.0, -2.0, 1.0 }));
    EXPECT_EQ(Discrete::backward_difference_weights(3), (std::vector<double> { 1.0, -3.0, 3.0, -1.0 }));
    EXPECT_EQ(Discrete::backward_difference_weights(0), (std::vector<double> { 1.0 }));
}

TEST(WeightsTest, GeometricWeightsFallByTheRatio)
{
    const auto weights = Discrete::geometric_weights(4, 0.5);
    ASSERT_EQ(weights.size(), 4U);
    EXPECT_DOUBLE_EQ(weights.at(0), 1.0);
    EXPECT_DOUBLE_EQ(weights.at(3), 0.125);

    double total = 0.0;
    for (const double w : Discrete::geometric_weights(8, 0.7, true)) {
        total += w;
    }
    EXPECT_NEAR(total, 1.0, 1e-12);
}

TEST(WeightsTest, MovingAverageWeightsAreEqualAndSumToOne)
{
    const auto weights = Discrete::moving_average_weights(4);
    ASSERT_EQ(weights.size(), 4U);
    for (const double w : weights) {
        EXPECT_DOUBLE_EQ(w, 0.25);
    }
    EXPECT_TRUE(Discrete::moving_average_weights(0).empty());
}

}
