#include "../test_config.h"

#include "MayaFlux/Nodes/Filters/FIR.hpp"
#include "MayaFlux/Nodes/Filters/IIR.hpp"

namespace MayaFlux::Test {

using Nodes::Filters::coefficients;
using Nodes::Filters::FIR;
using Nodes::Filters::IIR;

namespace {

    std::vector<double> reference_response(
        const std::vector<double>& a,
        const std::vector<double>& b,
        const std::vector<double>& x)
    {
        std::vector<double> y(x.size(), 0.0);
        for (size_t n = 0; n < x.size(); ++n) {
            double acc = 0.0;
            for (size_t k = 0; k < b.size() && k <= n; ++k) {
                acc += b.at(k) * x.at(n - k);
            }
            for (size_t i = 1; i < a.size() && i <= n; ++i) {
                acc -= a.at(i) * y.at(n - i);
            }
            y.at(n) = acc / a.at(0);
        }
        return y;
    }

    std::vector<double> impulse(size_t length)
    {
        std::vector<double> x(length, 0.0);
        x.at(0) = 1.0;
        return x;
    }

    std::vector<double> step(size_t length)
    {
        return std::vector<double>(length, 1.0);
    }

    std::vector<double> test_signal(size_t length)
    {
        std::vector<double> x(length);
        for (size_t n = 0; n < length; ++n) {
            const auto t = static_cast<double>(n);
            x.at(n) = 0.7 * std::sin(0.31 * t) + 0.3 * std::sin(1.7 * t + 0.4) + ((n % 7 == 0) ? 0.2 : -0.05);
        }
        return x;
    }

    std::vector<double> sine(double frequency, double sample_rate, size_t length)
    {
        std::vector<double> x(length);
        for (size_t n = 0; n < length; ++n) {
            x.at(n) = std::sin(2.0 * std::numbers::pi * frequency * static_cast<double>(n) / sample_rate);
        }
        return x;
    }

    template <typename FilterT>
    std::vector<double> run(FilterT& filter, const std::vector<double>& x)
    {
        std::vector<double> y;
        y.reserve(x.size());
        for (const double sample : x) {
            y.push_back(filter.process_sample(sample));
        }
        return y;
    }

    double rms_of_tail(const std::vector<double>& y, size_t tail)
    {
        double acc = 0.0;
        for (size_t n = y.size() - tail; n < y.size(); ++n) {
            acc += y.at(n) * y.at(n);
        }
        return std::sqrt(acc / static_cast<double>(tail));
    }

    void expect_sequences_near(
        const std::vector<double>& actual,
        const std::vector<double>& expected,
        double tolerance)
    {
        ASSERT_EQ(actual.size(), expected.size());
        for (size_t n = 0; n < actual.size(); ++n) {
            EXPECT_NEAR(actual.at(n), expected.at(n), tolerance) << "n=" << n;
        }
    }

    struct RbjBandpass {
        std::vector<double> a;
        std::vector<double> b;
    };

    RbjBandpass rbj_bandpass(double frequency, double q, double sample_rate)
    {
        const double w0 = 2.0 * std::numbers::pi * frequency / sample_rate;
        const double alpha = std::sin(w0) / (2.0 * q);
        const double a0 = 1.0 + alpha;
        return {
            .a = { 1.0, (-2.0 * std::cos(w0)) / a0, (1.0 - alpha) / a0 },
            .b = { alpha / a0, 0.0, -alpha / a0 },
        };
    }

}

TEST(IIRRecursionTest, SinglePoleImpulseResponseIsGeometric)
{
    IIR filter({ 1.0, -0.5 }, { 1.0 });

    const auto y = run(filter, impulse(16));

    double expected = 1.0;
    for (size_t n = 0; n < y.size(); ++n) {
        EXPECT_NEAR(y.at(n), expected, 1e-12) << "n=" << n;
        expected *= 0.5;
    }
}

TEST(IIRRecursionTest, NegativePoleAlternatesSign)
{
    IIR filter({ 1.0, 0.5 }, { 1.0 });

    const auto y = run(filter, impulse(16));

    double expected = 1.0;
    for (size_t n = 0; n < y.size(); ++n) {
        EXPECT_NEAR(y.at(n), expected, 1e-12) << "n=" << n;
        expected *= -0.5;
    }
}

TEST(IIRRecursionTest, SinglePoleStepResponseSettlesToDcGain)
{
    IIR filter({ 1.0, -0.5 }, { 1.0 });

    const auto y = run(filter, step(128));

    EXPECT_NEAR(y.back(), 2.0, 1e-9);
}

TEST(IIRRecursionTest, TwoPoleImpulseResponseMatchesDifferenceEquation)
{
    const std::vector<double> a { 1.0, -0.9, 0.2 };
    const std::vector<double> b { 1.0, 0.0, 0.0 };
    IIR filter(a, b);

    const auto x = impulse(64);
    expect_sequences_near(run(filter, x), reference_response(a, b, x), 1e-12);
}

TEST(IIRRecursionTest, FourthOrderMixedTapsMatchReferenceOnArbitrarySignal)
{
    const std::vector<double> a { 1.0, -0.4, 0.35, -0.02, 0.15 };
    const std::vector<double> b { 0.5, 0.25, -0.1, 0.05, 0.2 };
    IIR filter(a, b);

    const auto x = test_signal(2048);
    expect_sequences_near(run(filter, x), reference_response(a, b, x), 1e-10);
}

TEST(IIRRecursionTest, FeedforwardOnlyFilterIgnoresFeedbackHistory)
{
    const std::vector<double> b { 0.25, 0.5, 0.25 };
    IIR filter({ 1.0 }, b);

    const auto x = test_signal(256);
    expect_sequences_near(run(filter, x), reference_response({ 1.0 }, b, x), 1e-12);
}

TEST(IIRRecursionTest, FeedbackEchoLandsOnTheNextSampleOnly)
{
    IIR filter({ 1.0, -1.0 }, { 1.0 });

    const auto y = run(filter, impulse(8));

    for (const double sample : y) {
        EXPECT_NEAR(sample, 1.0, 1e-12);
    }
}

TEST(IIRRecursionTest, SecondFeedbackTapActsTwoSamplesBack)
{
    IIR filter({ 1.0, 0.0, -1.0 }, { 1.0 });

    const auto y = run(filter, impulse(8));
    const std::vector<double> expected { 1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0 };

    expect_sequences_near(y, expected, 1e-12);
}

TEST(IIRResonatorTest, FormantBiquadsMatchReferenceAndDecay)
{
    constexpr double sample_rate = 48000.0;
    const std::vector<std::pair<double, double>> formants {
        { 800.0, 16.0 }, { 1200.0, 30.0 }, { 2500.0, 55.0 }, { 3500.0, 70.0 }, { 4500.0, 90.0 }
    };

    for (const auto& [frequency, q] : formants) {
        const auto design = rbj_bandpass(frequency, q, sample_rate);
        IIR filter(design.a, design.b);

        EXPECT_LT(filter.max_pole_magnitude(), 1.0) << "f=" << frequency;

        const auto x = impulse(8192);
        const auto y = run(filter, x);
        expect_sequences_near(y, reference_response(design.a, design.b, x), 1e-12);

        EXPECT_LT(rms_of_tail(y, 256), 1e-3) << "f=" << frequency;
    }
}

TEST(IIRResonatorTest, RingsAtTheDesignFrequency)
{
    constexpr double sample_rate = 48000.0;
    constexpr double frequency = 1200.0;
    const auto design = rbj_bandpass(frequency, 30.0, sample_rate);
    IIR filter(design.a, design.b);

    const auto y = run(filter, impulse(2048));

    size_t crossings = 0;
    for (size_t n = 1; n < 400; ++n) {
        if ((y.at(n - 1) < 0.0) != (y.at(n) < 0.0)) {
            ++crossings;
        }
    }
    const double measured = static_cast<double>(crossings) * 0.5 * sample_rate / 399.0;
    EXPECT_NEAR(measured, frequency, 0.05 * frequency);
}

TEST(IIRResonatorTest, SteadyStateGainMatchesFrequencyResponse)
{
    constexpr double sample_rate = 48000.0;
    constexpr double frequency = 800.0;
    const auto design = rbj_bandpass(frequency, 16.0, sample_rate);
    IIR filter(design.a, design.b);

    const auto y = run(filter, sine(frequency, sample_rate, 8192));
    const double measured_amplitude = rms_of_tail(y, 600) * std::numbers::sqrt2;
    const double analytic = std::abs(filter.get_frequency_response(frequency, sample_rate));

    EXPECT_NEAR(measured_amplitude, analytic, 0.01 * analytic);
    EXPECT_NEAR(analytic, 1.0, 1e-6);
}

TEST(IIRResonatorTest, OffCentreToneIsAttenuatedAsPredicted)
{
    constexpr double sample_rate = 48000.0;
    constexpr double probe = 1600.0;
    const auto design = rbj_bandpass(800.0, 16.0, sample_rate);
    IIR filter(design.a, design.b);

    const auto y = run(filter, sine(probe, sample_rate, 8192));
    const double measured_amplitude = rms_of_tail(y, 600) * std::numbers::sqrt2;
    const double analytic = std::abs(filter.get_frequency_response(probe, sample_rate));

    EXPECT_LT(analytic, 0.5);
    EXPECT_NEAR(measured_amplitude, analytic, 0.02 * analytic + 1e-6);
}

TEST(IIRStabilityTest, PoleMagnitudeReportsDesignedStableFilter)
{
    IIR filter({ 1.0, -0.4, 0.35, -0.02, 0.15 }, { 1.0 });

    EXPECT_LT(filter.max_pole_magnitude(), 1.0);
}

TEST(IIRStabilityTest, PoleMagnitudeFlagsUnstableDenominator)
{
    IIR filter({ 1.0, -2.1, 1.0 }, { 1.0 });

    EXPECT_GT(filter.max_pole_magnitude(), 1.0);
}

TEST(IIRCoefficientTest, NonUnityLeadingCoefficientIsNormalized)
{
    IIR filter({ 2.0, -1.0, 0.5 }, { 1.0, 0.0, 0.0 });

    const auto& a = filter.getACoefficients();
    ASSERT_EQ(a.size(), 3U);
    EXPECT_DOUBLE_EQ(a.at(0), 1.0);
    EXPECT_DOUBLE_EQ(a.at(1), -0.5);
    EXPECT_DOUBLE_EQ(a.at(2), 0.25);

    const auto x = test_signal(256);
    expect_sequences_near(
        run(filter, x),
        reference_response({ 1.0, -0.5, 0.25 }, { 1.0, 0.0, 0.0 }, x),
        1e-12);
}

TEST(IIRCoefficientTest, ProcessingAndAnalysisAgreeForNonUnityLeadingCoefficient)
{
    constexpr double sample_rate = 48000.0;
    constexpr double frequency = 900.0;
    IIR filter({ 4.0, -3.2, 1.2 }, { 1.0, 0.5, 0.0 });

    const auto y = run(filter, sine(frequency, sample_rate, 4096));
    const double measured = rms_of_tail(y, 480) * std::numbers::sqrt2;
    const double analytic = std::abs(filter.get_frequency_response(frequency, sample_rate));

    EXPECT_NEAR(measured, analytic, 0.01 * analytic);
}

TEST(IIRCoefficientTest, RejectsNegligibleLeadingCoefficient)
{
    EXPECT_THROW(IIR({ 0.0, -0.5 }, { 1.0 }), std::invalid_argument);
    EXPECT_THROW(IIR({ 1e-9, -0.5 }, { 1.0 }), std::invalid_argument);
}

TEST(IIRCoefficientTest, RejectsEmptyCoefficientVectors)
{
    EXPECT_THROW(IIR({}, { 1.0 }), std::invalid_argument);
    EXPECT_THROW(IIR({ 1.0 }, {}), std::invalid_argument);
}

TEST(IIRCoefficientTest, AddCoefRoutesInputToFeedforwardAndOutputToFeedback)
{
    IIR filter({ 1.0 }, { 1.0 });

    filter.add_coef(1, 0.5, coefficients::INPUT);
    ASSERT_EQ(filter.getBCoefficients().size(), 2U);
    EXPECT_DOUBLE_EQ(filter.getBCoefficients().at(1), 0.5);
    ASSERT_EQ(filter.getACoefficients().size(), 1U);

    filter.add_coef(1, -0.5, coefficients::OUTPUT);
    ASSERT_EQ(filter.getACoefficients().size(), 2U);
    EXPECT_DOUBLE_EQ(filter.getACoefficients().at(1), -0.5);
    ASSERT_EQ(filter.getBCoefficients().size(), 2U);

    const auto x = test_signal(128);
    expect_sequences_near(
        run(filter, x),
        reference_response({ 1.0, -0.5 }, { 1.0, 0.5 }, x),
        1e-12);
}

TEST(IIRCoefficientTest, AddCoefPastTheEndZeroFillsAndGrowsHistory)
{
    IIR filter({ 1.0 }, { 1.0 });

    filter.add_coef(3, 0.25, coefficients::OUTPUT);

    const auto& a = filter.getACoefficients();
    ASSERT_EQ(a.size(), 4U);
    EXPECT_DOUBLE_EQ(a.at(1), 0.0);
    EXPECT_DOUBLE_EQ(a.at(2), 0.0);
    EXPECT_DOUBLE_EQ(a.at(3), 0.25);
    EXPECT_EQ(filter.get_output_history().size(), 4U);

    const auto x = impulse(64);
    expect_sequences_near(run(filter, x), reference_response({ 1.0, 0.0, 0.0, 0.25 }, { 1.0 }, x), 1e-12);
}

TEST(IIRCoefficientTest, AddCoefOnFeedforwardGrowsInputHistory)
{
    IIR filter({ 1.0 }, { 1.0 });

    filter.add_coef(2, 0.5, coefficients::INPUT);

    EXPECT_EQ(filter.getBCoefficients().size(), 3U);
    EXPECT_EQ(filter.get_input_history().size(), 3U);

    const auto x = test_signal(64);
    expect_sequences_near(run(filter, x), reference_response({ 1.0 }, { 1.0, 0.0, 0.5 }, x), 1e-12);
}

TEST(IIRCoefficientTest, EditFeedbackCoefsIndexZeroIsTheFirstFeedbackTap)
{
    IIR filter({ 1.0, -0.5 }, { 1.0 });

    auto taps = filter.edit_feedback_coefs();
    ASSERT_EQ(taps.size(), 1U);
    taps.front() = -0.25;

    const auto y = run(filter, impulse(8));

    double expected = 1.0;
    for (size_t n = 0; n < y.size(); ++n) {
        EXPECT_NEAR(y.at(n), expected, 1e-12) << "n=" << n;
        expected *= 0.25;
    }
}

TEST(IIRCoefficientTest, EditFeedforwardCoefsChangeTheResponseImmediately)
{
    IIR filter({ 1.0 }, { 1.0, 0.0 });

    auto taps = filter.edit_feedforward_coefs();
    ASSERT_EQ(taps.size(), 2U);
    taps.back() = 0.75;

    const auto y = run(filter, impulse(4));
    const std::vector<double> expected { 1.0, 0.75, 0.0, 0.0 };
    expect_sequences_near(y, expected, 1e-12);
}

TEST(IIRHistoryTest, OutputHistoryLengthTracksDenominatorAndSlotZeroIsLatest)
{
    IIR filter({ 1.0, -0.5, 0.25 }, { 1.0, 0.0, 0.0 });

    EXPECT_EQ(filter.get_output_history().size(), 3U);

    const double first = filter.process_sample(1.0);
    EXPECT_DOUBLE_EQ(filter.get_output_history().at(0), first);

    const double second = filter.process_sample(0.0);
    EXPECT_DOUBLE_EQ(filter.get_output_history().at(0), second);
    EXPECT_DOUBLE_EQ(filter.get_output_history().at(1), first);

    const double third = filter.process_sample(0.0);
    EXPECT_DOUBLE_EQ(filter.get_output_history().at(0), third);
    EXPECT_DOUBLE_EQ(filter.get_output_history().at(1), second);
    EXPECT_DOUBLE_EQ(filter.get_output_history().at(2), first);
}

TEST(IIRHistoryTest, InputHistorySlotZeroIsLatestInput)
{
    IIR filter({ 1.0, -0.5 }, { 1.0, 0.5, 0.25 });

    filter.process_sample(0.3);
    filter.process_sample(0.6);
    filter.process_sample(0.9);

    const auto& history = filter.get_input_history();
    ASSERT_EQ(history.size(), 3U);
    EXPECT_DOUBLE_EQ(history.at(0), 0.9);
    EXPECT_DOUBLE_EQ(history.at(1), 0.6);
    EXPECT_DOUBLE_EQ(history.at(2), 0.3);
}

TEST(IIRHistoryTest, ResetClearsStateSoResponseRepeats)
{
    IIR filter({ 1.0, -0.9, 0.2 }, { 1.0, 0.3, 0.0 });

    const auto first = run(filter, impulse(64));
    run(filter, test_signal(512));
    filter.reset();
    const auto second = run(filter, impulse(64));

    expect_sequences_near(second, first, 1e-12);
}

TEST(IIRHistoryTest, SaveAndRestoreReplayIdentically)
{
    IIR filter({ 1.0, -0.9, 0.2 }, { 0.6, 0.3, 0.1 });
    const auto warmup = test_signal(200);
    const auto probe = test_signal(300);

    run(filter, warmup);
    filter.save_state();
    const auto first = run(filter, probe);
    filter.restore_state();
    const auto second = run(filter, probe);

    expect_sequences_near(second, first, 1e-12);
}

TEST(IIRGainTest, GainScalesOutputWithoutMovingPoles)
{
    const std::vector<double> a { 1.0, -0.9, 0.2 };
    const std::vector<double> b { 1.0, 0.0, 0.0 };
    IIR filter(a, b);
    filter.set_gain(0.5);

    const auto x = test_signal(256);
    auto expected = reference_response(a, b, x);
    for (double& sample : expected) {
        sample *= 0.5;
    }

    expect_sequences_near(run(filter, x), expected, 1e-12);
}

TEST(IIRGainTest, LastOutputMatchesReturnedValue)
{
    IIR filter({ 1.0, -0.5 }, { 1.0 });
    filter.set_gain(0.25);

    for (const double sample : test_signal(32)) {
        const double returned = filter.process_sample(sample);
        EXPECT_DOUBLE_EQ(filter.get_last_output(), returned);
    }
}

TEST(IIRGainTest, BypassPassesInputThroughAndCachesIt)
{
    IIR filter({ 1.0, -0.5 }, { 0.1 });
    filter.set_bypass(true);

    for (const double sample : test_signal(32)) {
        EXPECT_DOUBLE_EQ(filter.process_sample(sample), sample);
        EXPECT_DOUBLE_EQ(filter.get_last_output(), sample);
    }
}

TEST(FIRTest, ImpulseResponseEqualsTapsThenZeros)
{
    const std::vector<double> taps { 0.1, 0.4, 0.3, -0.2, 0.05 };
    FIR filter(taps);

    const auto y = run(filter, impulse(12));

    for (size_t n = 0; n < y.size(); ++n) {
        const double expected = n < taps.size() ? taps.at(n) : 0.0;
        EXPECT_NEAR(y.at(n), expected, 1e-12) << "n=" << n;
    }
}

TEST(FIRTest, MatchesDirectConvolutionOnArbitrarySignal)
{
    const std::vector<double> taps { 0.05, 0.1, 0.2, 0.3, 0.2, 0.1, 0.05 };
    FIR filter(taps);

    const auto x = test_signal(1024);
    expect_sequences_near(run(filter, x), reference_response({ 1.0 }, taps, x), 1e-12);
}

TEST(FIRTest, SingleTapIsAPureGain)
{
    FIR filter({ 0.5 });

    const auto x = test_signal(64);
    const auto y = run(filter, x);

    for (size_t n = 0; n < x.size(); ++n) {
        EXPECT_DOUBLE_EQ(y.at(n), 0.5 * x.at(n));
    }
}

TEST(FIRTest, StepResponseSettlesToTapSum)
{
    FIR filter({ 0.1, 0.4, 0.3, -0.2, 0.05 });

    const auto y = run(filter, step(32));

    EXPECT_NEAR(y.back(), 0.65, 1e-12);
}

TEST(FIRTest, DenominatorIsUnityAndOutputHistoryTracksLatestOutput)
{
    FIR filter({ 0.5, 0.25, 0.125 });

    ASSERT_EQ(filter.getACoefficients().size(), 1U);
    EXPECT_DOUBLE_EQ(filter.getACoefficients().at(0), 1.0);
    ASSERT_EQ(filter.get_output_history().size(), 1U);

    for (const double sample : test_signal(16)) {
        const double raw = filter.process_sample(sample);
        EXPECT_DOUBLE_EQ(filter.get_output_history().at(0), raw);
    }
}

TEST(FIRTest, InputHistorySlotZeroIsLatestInput)
{
    FIR filter({ 0.5, 0.25, 0.125 });

    filter.process_sample(0.3);
    filter.process_sample(0.6);
    filter.process_sample(0.9);

    const auto& history = filter.get_input_history();
    ASSERT_EQ(history.size(), 3U);
    EXPECT_DOUBLE_EQ(history.at(0), 0.9);
    EXPECT_DOUBLE_EQ(history.at(1), 0.6);
    EXPECT_DOUBLE_EQ(history.at(2), 0.3);
}

TEST(FIRTest, MovingAverageSteadyStateGainMatchesFrequencyResponse)
{
    constexpr double sample_rate = 48000.0;
    constexpr double frequency = 3000.0;
    FIR filter({ 0.25, 0.25, 0.25, 0.25 });

    const auto y = run(filter, sine(frequency, sample_rate, 4096));
    const double measured = rms_of_tail(y, 480) * std::numbers::sqrt2;
    const double analytic = std::abs(filter.get_frequency_response(frequency, sample_rate));

    EXPECT_NEAR(measured, analytic, 0.01 * analytic);
}

TEST(FIRTest, UnityDcGainAtZeroFrequency)
{
    FIR filter({ 0.25, 0.25, 0.25, 0.25 });

    EXPECT_NEAR(std::abs(filter.get_frequency_response(0.0, 48000.0)), 1.0, 1e-12);
}

TEST(FIRTest, ResetClearsHistorySoResponseRepeats)
{
    FIR filter({ 0.1, 0.4, 0.3, -0.2, 0.05 });

    const auto first = run(filter, impulse(16));
    run(filter, test_signal(128));
    filter.reset();
    const auto second = run(filter, impulse(16));

    expect_sequences_near(second, first, 1e-12);
}

TEST(FIRTest, SaveAndRestoreReplayIdentically)
{
    FIR filter({ 0.1, 0.4, 0.3, -0.2, 0.05 });
    const auto warmup = test_signal(50);
    const auto probe = test_signal(100);

    run(filter, warmup);
    filter.save_state();
    const auto first = run(filter, probe);
    filter.restore_state();
    const auto second = run(filter, probe);

    expect_sequences_near(second, first, 1e-12);
}

TEST(FIRTest, GainScalesOutput)
{
    const std::vector<double> taps { 0.1, 0.4, 0.3 };
    FIR filter(taps);
    filter.set_gain(2.0);

    const auto x = test_signal(128);
    auto expected = reference_response({ 1.0 }, taps, x);
    for (double& sample : expected) {
        sample *= 2.0;
    }

    expect_sequences_near(run(filter, x), expected, 1e-12);
}

TEST(FIRTest, LastOutputMatchesReturnedValue)
{
    FIR filter({ 0.5, 0.5 });
    filter.set_gain(0.25);

    for (const double sample : test_signal(32)) {
        const double returned = filter.process_sample(sample);
        EXPECT_DOUBLE_EQ(filter.get_last_output(), returned);
    }
}

TEST(FIRTest, BypassPassesInputThroughAndCachesIt)
{
    FIR filter({ 0.5, 0.5 });
    filter.set_bypass(true);

    for (const double sample : test_signal(32)) {
        EXPECT_DOUBLE_EQ(filter.process_sample(sample), sample);
        EXPECT_DOUBLE_EQ(filter.get_last_output(), sample);
    }
}

}
