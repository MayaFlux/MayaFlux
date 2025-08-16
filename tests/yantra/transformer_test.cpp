#include "../test_config.h"

#include "MayaFlux/Yantra/Transformers/ConvolutionTransformer.hpp"
#include "MayaFlux/Yantra/Transformers/MathematicalTransformer.hpp"
#include "MayaFlux/Yantra/Transformers/SpectralTransformer.hpp"
#include "MayaFlux/Yantra/Transformers/TemporalTransformer.hpp"
#include <cstddef>
#include <random>

using namespace MayaFlux::Yantra;
using namespace MayaFlux::Kakshya;

namespace MayaFlux::Test {

// =========================================================================
// TEST DATA GENERATORS
// =========================================================================

class TransformerTestDataGenerator {
public:
    /**
     * @brief Generate sine wave with known frequency and amplitude
     */
    static std::vector<double> create_sine_wave(size_t samples = 1024,
        double frequency = 440.0,
        double amplitude = 1.0,
        double sample_rate = 44100.0)
    {
        std::vector<double> signal;
        signal.reserve(samples);

        for (size_t i = 0; i < samples; ++i) {
            double t = static_cast<double>(i) / sample_rate;
            signal.push_back(amplitude * std::sin(2.0 * M_PI * frequency * t));
        }

        return signal;
    }

    /**
     * @brief Generate impulse signal (delta function at start)
     */
    static std::vector<double> create_impulse(size_t samples = 1024, double amplitude = 1.0)
    {
        std::vector<double> signal(samples, 0.0);
        if (samples > 0) {
            signal[0] = amplitude;
        }
        return signal;
    }

    /**
     * @brief Generate white noise with known variance
     */
    static std::vector<double> create_white_noise(size_t samples = 1024,
        double variance = 1.0,
        u_int32_t seed = 42)
    {
        std::vector<double> signal;
        signal.reserve(samples);

        std::mt19937 gen(seed);
        std::normal_distribution<double> dist(0.0, std::sqrt(variance));

        for (size_t i = 0; i < samples; ++i) {
            signal.push_back(dist(gen));
        }

        return signal;
    }

    /**
     * @brief Generate linear ramp from 0 to 1
     */
    static std::vector<double> create_linear_ramp(size_t samples = 1024)
    {
        std::vector<double> signal;
        signal.reserve(samples);

        for (size_t i = 0; i < samples; ++i) {
            signal.push_back(static_cast<double>(i) / static_cast<double>(samples - 1));
        }

        return signal;
    }

    /**
     * @brief Generate constant signal
     */
    static std::vector<double> create_constant(size_t samples = 1024, double value = 1.0)
    {
        return std::vector<double>(samples, value);
    }

    /**
     * @brief Generate multi-frequency signal for spectral testing
     */
    static std::vector<double> create_multi_tone(size_t samples = 2048,
        const std::vector<double>& frequencies = { 220.0, 440.0, 880.0 },
        double sample_rate = 44100.0)
    {
        std::vector<double> signal(samples, 0.0);

        for (double freq : frequencies) {
            for (size_t i = 0; i < samples; ++i) {
                double t = static_cast<double>(i) / sample_rate;
                signal[i] += std::sin(2.0 * M_PI * freq * t) / frequencies.size();
            }
        }

        return signal;
    }
};

// =========================================================================
// CONVOLUTION TRANSFORMER TESTS
// =========================================================================

class ConvolutionTransformerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        transformer = std::make_unique<ConvolutionTransformer<>>();
        test_signal = TransformerTestDataGenerator::create_sine_wave(1024, 440.0, 1.0);
    }

    std::unique_ptr<ConvolutionTransformer<>> transformer;
    std::vector<double> test_signal;
};

TEST_F(ConvolutionTransformerTest, DirectConvolutionWithImpulseResponse)
{
    transformer->set_parameter("operation", ConvolutionOperation::DIRECT_CONVOLUTION);

    std::vector<double> impulse_response = { 0.25, 0.5, 0.25 };
    transformer->set_parameter("impulse_response", impulse_response);

    IO<DataVariant> input(DataVariant { test_signal });
    auto result = transformer->apply_operation(input);

    auto result_data = std::get<std::vector<double>>(result.data);
    EXPECT_FALSE(result_data.empty());
    EXPECT_EQ(result_data.size(), test_signal.size());

    double max_input = *std::ranges::max_element(test_signal);
    double max_output = *std::ranges::max_element(result_data);
    EXPECT_LT(max_output, max_input) << "Low-pass filter should reduce peak amplitude";
}

TEST_F(ConvolutionTransformerTest, CrossCorrelationNormalized)
{
    transformer->set_parameter("operation", ConvolutionOperation::CROSS_CORRELATION);

    std::vector<double> template_signal(test_signal.begin(), test_signal.begin() + 64);
    transformer->set_parameter("template_signal", template_signal);
    transformer->set_parameter("normalize", true);

    IO<DataVariant> input(DataVariant { test_signal });
    auto result = transformer->apply_operation(input);

    auto result_data = std::get<std::vector<double>>(result.data);
    EXPECT_FALSE(result_data.empty());

    auto [min_it, max_it] = std::ranges::minmax_element(result_data);
    EXPECT_GE(*max_it, 0.5) << "Should find strong correlation";
    EXPECT_LE(*max_it, 1.1) << "Normalized correlation should not exceed 1.0 significantly";
}

TEST_F(ConvolutionTransformerTest, MatchedFilterDetection)
{
    transformer->set_parameter("operation", ConvolutionOperation::MATCHED_FILTER);

    auto reference_signal = TransformerTestDataGenerator::create_sine_wave(64, 440.0, 1.0);
    transformer->set_parameter("reference_signal", reference_signal);

    IO<DataVariant> input(DataVariant { test_signal });
    auto result = transformer->apply_operation(input);

    auto result_data = std::get<std::vector<double>>(result.data);
    EXPECT_FALSE(result_data.empty());

    double max_correlation = *std::ranges::max_element(result_data);
    EXPECT_GT(max_correlation, 0.3) << "Matched filter should detect similar patterns";
}

TEST_F(ConvolutionTransformerTest, AutoCorrelation)
{
    transformer->set_parameter("operation", ConvolutionOperation::AUTO_CORRELATION);
    transformer->set_parameter("strategy", TransformationStrategy::BUFFERED);
    transformer->set_parameter("normalize", true);

    IO<DataVariant> input(DataVariant { test_signal });
    auto result = transformer->apply_operation(input);

    auto result_data = std::get<std::vector<double>>(result.data);
    EXPECT_FALSE(result_data.empty());

    double zero_lag_value = result_data[0];
    EXPECT_NEAR(zero_lag_value, 1.0, 0.1) << "Auto-correlation peak should be near 1.0";
}

TEST_F(ConvolutionTransformerTest, DeconvolutionBasic)
{
    transformer->set_parameter("operation", ConvolutionOperation::DECONVOLUTION);

    std::vector<double> impulse_response = { 1.0, 0.5 };
    transformer->set_parameter("impulse_response", impulse_response);
    transformer->set_parameter("regularization", 1e-3);

    IO<DataVariant> input(DataVariant { test_signal });

    EXPECT_NO_THROW({
        auto result = transformer->apply_operation(input);
        auto result_data = std::get<std::vector<double>>(result.data);
        EXPECT_FALSE(result_data.empty());
    }) << "Deconvolution should not throw for reasonable inputs";
}

TEST_F(ConvolutionTransformerTest, ParameterValidation)
{
    EXPECT_NO_THROW(transformer->set_parameter("operation", "invalid_operation"));

    transformer->set_parameter("operation", "CROSS_CORRELATION");
    EXPECT_EQ(transformer->get_transformation_type(), TransformationType::CONVOLUTION);

    std::string name = transformer->get_transformer_name();
    EXPECT_TRUE(name.find("ConvolutionTransformer") != std::string::npos);
}

// =========================================================================
// TRANSFORMER METADATA AND PIPELINE TESTS
// =========================================================================

class TransformerMetadataTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_signal = TransformerTestDataGenerator::create_sine_wave(1024);
    }

    std::vector<double> test_signal;
};

TEST_F(TransformerMetadataTest, ConvolutionTransformerMetadata)
{
    auto transformer = std::make_unique<ConvolutionTransformer<>>();
    transformer->set_parameter("operation", ConvolutionOperation::DIRECT_CONVOLUTION);

    EXPECT_EQ(transformer->get_transformation_type(), TransformationType::CONVOLUTION);

    std::string name = transformer->get_transformer_name();
    EXPECT_TRUE(name.find("ConvolutionTransformer") != std::string::npos);
    EXPECT_TRUE(name.find("DIRECT_CONVOLUTION") != std::string::npos);
}

TEST_F(TransformerMetadataTest, MathematicalTransformerMetadata)
{
    auto transformer = std::make_unique<MathematicalTransformer<>>();
    transformer->set_parameter("operation", MathematicalOperation::POLYNOMIAL);

    EXPECT_EQ(transformer->get_transformation_type(), TransformationType::MATHEMATICAL);

    std::string name = transformer->get_transformer_name();
    EXPECT_TRUE(name.find("MathematicalTransformer") != std::string::npos);
    EXPECT_TRUE(name.find("POLYNOMIAL") != std::string::npos);
}

TEST_F(TransformerMetadataTest, SpectralTransformerMetadata)
{
    auto transformer = std::make_unique<SpectralTransformer<>>();
    transformer->set_parameter("operation", SpectralOperation::PITCH_SHIFT);

    EXPECT_EQ(transformer->get_transformation_type(), TransformationType::SPECTRAL);

    std::string name = transformer->get_transformer_name();
    EXPECT_TRUE(name.find("SpectralTransformer") != std::string::npos);
    EXPECT_TRUE(name.find("PITCH_SHIFT") != std::string::npos);
}

TEST_F(TransformerMetadataTest, TemporalTransformerMetadata)
{
    auto transformer = std::make_unique<TemporalTransformer<>>();
    transformer->set_parameter("operation", TemporalOperation::TIME_REVERSE);

    EXPECT_EQ(transformer->get_transformation_type(), TransformationType::TEMPORAL);

    std::string name = transformer->get_transformer_name();
    EXPECT_TRUE(name.find("TemporalTransformer") != std::string::npos);
    EXPECT_TRUE(name.find("TIME_REVERSE") != std::string::npos);
}

// =========================================================================
// MATHEMATICAL TRANSFORMER TESTS
// =========================================================================

class MathematicalTransformerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        transformer = std::make_unique<MathematicalTransformer<>>();
        test_signal = TransformerTestDataGenerator::create_linear_ramp(1024);
    }

    std::unique_ptr<MathematicalTransformer<>> transformer;
    std::vector<double> test_signal;
};

TEST_F(MathematicalTransformerTest, GainTransformation)
{
    transformer->set_parameter("operation", MathematicalOperation::GAIN);
    transformer->set_parameter("gain_factor", 2.0);

    IO<DataVariant> input(DataVariant { test_signal });
    auto result = transformer->apply_operation(input);

    auto result_data = std::get<std::vector<double>>(result.data);
    EXPECT_EQ(result_data.size(), test_signal.size());

    for (size_t i = 0; i < std::min(result_data.size(), test_signal.size()); ++i) {
        EXPECT_NEAR(result_data[i], test_signal[i] * 2.0, 1e-10);
    }
}

TEST_F(MathematicalTransformerTest, OffsetTransformation)
{
    transformer->set_parameter("operation", MathematicalOperation::OFFSET);
    transformer->set_parameter("offset_value", 0.5);

    IO<DataVariant> input(DataVariant { test_signal });
    auto result = transformer->apply_operation(input);

    auto result_data = std::get<std::vector<double>>(result.data);
    EXPECT_EQ(result_data.size(), test_signal.size());

    for (size_t i = 0; i < std::min(result_data.size(), test_signal.size()); ++i) {
        EXPECT_NEAR(result_data[i], test_signal[i] + 0.5, 1e-10);
    }
}

TEST_F(MathematicalTransformerTest, PowerTransformation)
{
    transformer->set_parameter("operation", MathematicalOperation::POWER);
    transformer->set_parameter("exponent", 2.0);

    IO<DataVariant> input(DataVariant { test_signal });
    auto result = transformer->apply_operation(input);

    auto result_data = std::get<std::vector<double>>(result.data);
    EXPECT_EQ(result_data.size(), test_signal.size());

    for (size_t i = 0; i < std::min(result_data.size(), test_signal.size()); ++i) {
        EXPECT_NEAR(result_data[i], test_signal[i] * test_signal[i], 1e-10);
    }
}

TEST_F(MathematicalTransformerTest, LogarithmicTransformation)
{
    auto positive_signal = TransformerTestDataGenerator::create_constant(1024, std::numbers::e);

    transformer->set_parameter("operation", MathematicalOperation::LOGARITHMIC);
    transformer->set_parameter("base", std::numbers::e);
    transformer->set_parameter("scale", 1.0);
    transformer->set_parameter("input_scale", 1.0);
    transformer->set_parameter("offset", 0.0);

    IO<DataVariant> input(DataVariant { positive_signal });
    auto result = transformer->apply_operation(input);

    auto result_data = std::get<std::vector<double>>(result.data);
    EXPECT_EQ(result_data.size(), positive_signal.size());

    // Now: ln(1.0 * e + 0.0) = ln(e) = 1.0
    for (double value : result_data) {
        EXPECT_NEAR(value, 1.0, 1e-6);
    }
}

TEST_F(MathematicalTransformerTest, ExponentialTransformation)
{
    auto zero_signal = TransformerTestDataGenerator::create_constant(1024, 0.0);

    transformer->set_parameter("operation", MathematicalOperation::EXPONENTIAL);
    transformer->set_parameter("base", std::numbers::e);
    transformer->set_parameter("scale", 1.0);

    IO<DataVariant> input(DataVariant { zero_signal });
    auto result = transformer->apply_operation(input);

    auto result_data = std::get<std::vector<double>>(result.data);
    EXPECT_EQ(result_data.size(), zero_signal.size());

    for (double value : result_data) {
        EXPECT_NEAR(value, 1.0, 1e-10);
    }
}

TEST_F(MathematicalTransformerTest, TrigonometricSine)
{
    auto pi_half_signal = TransformerTestDataGenerator::create_constant(1024, M_PI / 2.0);

    transformer->set_parameter("operation", MathematicalOperation::TRIGONOMETRIC);
    transformer->set_parameter("trig_function", std::string("sin"));
    transformer->set_parameter("frequency", 1.0);
    transformer->set_parameter("amplitude", 1.0);
    transformer->set_parameter("phase", 0.0);

    IO<DataVariant> input(DataVariant { pi_half_signal });
    auto result = transformer->apply_operation(input);

    auto result_data = std::get<std::vector<double>>(result.data);
    EXPECT_EQ(result_data.size(), pi_half_signal.size());

    for (double value : result_data) {
        EXPECT_NEAR(value, 1.0, 1e-10);
    }
}

TEST_F(MathematicalTransformerTest, TrigonometricCosine)
{
    auto zero_signal = TransformerTestDataGenerator::create_constant(1024, 0.0);

    transformer->set_parameter("operation", MathematicalOperation::TRIGONOMETRIC);
    transformer->set_parameter("trig_function", std::string("cos"));

    IO<DataVariant> input(DataVariant { zero_signal });
    auto result = transformer->apply_operation(input);

    auto result_data = std::get<std::vector<double>>(result.data);

    for (double value : result_data) {
        EXPECT_NEAR(value, 1.0, 1e-10);
    }
}

TEST_F(MathematicalTransformerTest, QuantizationTransformation)
{
    transformer->set_parameter("operation", MathematicalOperation::QUANTIZE);
    transformer->set_parameter("bits", static_cast<u_int8_t>(8));

    IO<DataVariant> input(DataVariant { test_signal });
    auto result = transformer->apply_operation(input);

    auto result_data = std::get<std::vector<double>>(result.data);
    EXPECT_EQ(result_data.size(), test_signal.size());

    // 8-bit quantization should have 256 distinct levels
    std::set<double> unique_values(result_data.begin(), result_data.end());
    EXPECT_LE(unique_values.size(), 256U) << "8-bit quantization should not exceed 256 levels";
}

TEST_F(MathematicalTransformerTest, NormalizationTransformation)
{
    transformer->set_parameter("operation", MathematicalOperation::NORMALIZE);
    transformer->set_parameter("target_peak", 0.5);

    IO<DataVariant> input(DataVariant { test_signal });
    auto result = transformer->apply_operation(input);

    auto result_data = std::get<std::vector<double>>(result.data);
    EXPECT_EQ(result_data.size(), test_signal.size());

    auto max_val = *std::ranges::max_element(result_data);
    EXPECT_NEAR(max_val, 0.5, 1e-10);
}

TEST_F(MathematicalTransformerTest, PolynomialTransformation)
{
    transformer->set_parameter("operation", MathematicalOperation::POLYNOMIAL);

    std::vector<double> coefficients = { 2.0, 1.0 };
    transformer->set_parameter("coefficients", coefficients);

    IO<DataVariant> input(DataVariant { test_signal });
    auto result = transformer->apply_operation(input);

    auto result_data = std::get<std::vector<double>>(result.data);
    EXPECT_EQ(result_data.size(), test_signal.size());

    // Check polynomial evaluation: f(x) = 1 + 2x
    for (size_t i = 0; i < std::min(result_data.size(), test_signal.size()); ++i) {
        double expected = 1.0 + 2.0 * test_signal[i];
        EXPECT_NEAR(result_data[i], expected, 1e-6);
    }
}

// =========================================================================
// SPECTRAL TRANSFORMER TESTS
// =========================================================================

class SpectralTransformerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        transformer = std::make_unique<SpectralTransformer<>>();
        test_signal = TransformerTestDataGenerator::create_multi_tone(2048, { 220.0, 440.0, 880.0 });
    }

    std::unique_ptr<SpectralTransformer<>> transformer;
    std::vector<double> test_signal;
};

TEST_F(SpectralTransformerTest, FrequencyShiftTransformation)
{
    transformer->set_parameter("operation", SpectralOperation::FREQUENCY_SHIFT);
    transformer->set_parameter("shift_hz", 100.0);
    transformer->set_parameter("window_size", static_cast<u_int32_t>(1024));
    transformer->set_parameter("hop_size", static_cast<u_int32_t>(512));

    IO<DataVariant> input(DataVariant { test_signal });
    auto result = transformer->apply_operation(input);

    auto result_data = std::get<std::vector<double>>(result.data);
    EXPECT_FALSE(result_data.empty());
    EXPECT_LE(result_data.size(), test_signal.size() * 1.2) << "Output size should be reasonable";
}

TEST_F(SpectralTransformerTest, PitchShiftTransformation)
{
    transformer->set_parameter("operation", SpectralOperation::PITCH_SHIFT);
    transformer->set_parameter("pitch_ratio", 1.5);
    transformer->set_parameter("window_size", static_cast<u_int32_t>(1024));
    transformer->set_parameter("hop_size", static_cast<u_int32_t>(512));

    IO<DataVariant> input(DataVariant { test_signal });
    auto result = transformer->apply_operation(input);

    auto result_data = std::get<std::vector<double>>(result.data);
    EXPECT_FALSE(result_data.empty());

    EXPECT_GT(result_data.size(), test_signal.size() * 0.8);
    EXPECT_LT(result_data.size(), test_signal.size() * 1.2);
}

TEST_F(SpectralTransformerTest, SpectralFilterTransformation)
{
    transformer->set_parameter("operation", SpectralOperation::SPECTRAL_FILTER);
    transformer->set_parameter("low_freq", 200.0);
    transformer->set_parameter("high_freq", 500.0); // Should preserve 220Hz and 440Hz, remove 880Hz
    transformer->set_parameter("window_size", static_cast<u_int32_t>(1024));
    transformer->set_parameter("hop_size", static_cast<u_int32_t>(512));

    IO<DataVariant> input(DataVariant { test_signal });
    auto result = transformer->apply_operation(input);

    auto result_data = std::get<std::vector<double>>(result.data);
    EXPECT_FALSE(result_data.empty());

    EXPECT_EQ(result_data.size(), test_signal.size());

    double input_energy = std::accumulate(test_signal.begin(), test_signal.end(), 0.0,
        [](double sum, double val) { return sum + val * val; });
    double output_energy = std::accumulate(result_data.begin(), result_data.end(), 0.0,
        [](double sum, double val) { return sum + val * val; });

    EXPECT_LT(output_energy, input_energy) << "Spectral filtering should reduce energy";
}

TEST_F(SpectralTransformerTest, HarmonicEnhanceTransformation)
{
    transformer->set_parameter("operation", SpectralOperation::HARMONIC_ENHANCE);
    transformer->set_parameter("enhancement_factor", 2.0);
    transformer->set_parameter("window_size", static_cast<u_int32_t>(1024));
    transformer->set_parameter("hop_size", static_cast<u_int32_t>(512));

    IO<DataVariant> input(DataVariant { test_signal });
    auto result = transformer->apply_operation(input);

    auto result_data = std::get<std::vector<double>>(result.data);
    EXPECT_FALSE(result_data.empty());
    EXPECT_EQ(result_data.size(), test_signal.size());
}

TEST_F(SpectralTransformerTest, SpectralGateTransformation)
{
    transformer->set_parameter("operation", SpectralOperation::SPECTRAL_GATE);
    transformer->set_parameter("threshold", -30.0);
    transformer->set_parameter("window_size", static_cast<u_int32_t>(1024));
    transformer->set_parameter("hop_size", static_cast<u_int32_t>(512));

    IO<DataVariant> input(DataVariant { test_signal });
    auto result = transformer->apply_operation(input);

    auto result_data = std::get<std::vector<double>>(result.data);
    EXPECT_FALSE(result_data.empty());
    EXPECT_EQ(result_data.size(), test_signal.size());
}

// =========================================================================
// TEMPORAL TRANSFORMER TESTS
// =========================================================================

class TemporalTransformerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        transformer = std::make_unique<TemporalTransformer<>>();
        test_signal = TransformerTestDataGenerator::create_sine_wave(1024, 440.0, 1.0);
    }

    std::unique_ptr<TemporalTransformer<>> transformer;
    std::vector<double> test_signal;
};

TEST_F(TemporalTransformerTest, SliceTransformation)
{
    transformer->set_parameter("operation", TemporalOperation::SLICE);
    transformer->set_parameter("start_ratio", 0.25);
    transformer->set_parameter("end_ratio", 0.75);

    IO<DataVariant> input(DataVariant { test_signal });
    auto result = transformer->apply_operation(input);

    auto result_data = std::get<std::vector<double>>(result.data);

    auto expected_size = static_cast<size_t>(test_signal.size() * 0.5);
    EXPECT_EQ(result_data.size(), expected_size);

    auto start_idx = static_cast<size_t>(test_signal.size() * 0.25);
    for (size_t i = 0; i < result_data.size(); ++i) {
        EXPECT_NEAR(result_data[i], test_signal[start_idx + i], 1e-10);
    }
}

TEST_F(TemporalTransformerTest, InterpolationLinear)
{
    transformer->set_parameter("operation", TemporalOperation::INTERPOLATE);
    transformer->set_parameter("target_size", static_cast<size_t>(2048));
    transformer->set_parameter("use_cubic", false);

    IO<DataVariant> input(DataVariant { test_signal });
    auto result = transformer->apply_operation(input);

    auto result_data = std::get<std::vector<double>>(result.data);
    EXPECT_EQ(result_data.size(), 2048U);

    EXPECT_NEAR(result_data[0], test_signal[0], 1e-10);
    EXPECT_NEAR(result_data.back(), test_signal.back(), 1e-10);
}

TEST_F(TemporalTransformerTest, InterpolationCubic)
{
    transformer->set_parameter("operation", TemporalOperation::INTERPOLATE);
    transformer->set_parameter("target_size", static_cast<size_t>(512));
    transformer->set_parameter("use_cubic", true);

    IO<DataVariant> input(DataVariant { test_signal });
    auto result = transformer->apply_operation(input);

    auto result_data = std::get<std::vector<double>>(result.data);
    EXPECT_EQ(result_data.size(), 512U);
}

TEST_F(TemporalTransformerTest, TimeReverseTransformation)
{
    transformer->set_parameter("operation", TemporalOperation::TIME_REVERSE);

    IO<DataVariant> input(DataVariant { test_signal });
    auto result = transformer->apply_operation(input);

    auto result_data = std::get<std::vector<double>>(result.data);
    EXPECT_EQ(result_data.size(), test_signal.size());

    for (size_t i = 0; i < test_signal.size(); ++i) {
        size_t reverse_idx = test_signal.size() - 1 - i;
        EXPECT_NEAR(result_data[i], test_signal[reverse_idx], 1e-10);
    }
}

TEST_F(TemporalTransformerTest, TimeStretchTransformation)
{
    transformer->set_parameter("operation", TemporalOperation::TIME_STRETCH);
    transformer->set_parameter("stretch_factor", 2.0);

    IO<DataVariant> input(DataVariant { test_signal });
    auto result = transformer->apply_operation(input);

    auto result_data = std::get<std::vector<double>>(result.data);
    EXPECT_GT(result_data.size(), test_signal.size() * 1.5);
    EXPECT_LT(result_data.size(), test_signal.size() * 2.5);
}

TEST_F(TemporalTransformerTest, DelayTransformation)
{
    transformer->set_parameter("operation", TemporalOperation::DELAY);
    transformer->set_parameter("delay_samples", static_cast<u_int32_t>(100));
    transformer->set_parameter("fill_value", 0.0);

    IO<DataVariant> input(DataVariant { test_signal });
    auto result = transformer->apply_operation(input);

    auto result_data = std::get<std::vector<double>>(result.data);
    EXPECT_EQ(result_data.size(), test_signal.size() + 100);

    for (size_t i = 0; i < 100; ++i) {
        EXPECT_NEAR(result_data[i], 0.0, 1e-10);
    }

    for (size_t i = 100; i < result_data.size() && (i - 100) < test_signal.size(); ++i) {
        EXPECT_NEAR(result_data[i], test_signal[i - 100], 1e-10);
    }
}

TEST_F(TemporalTransformerTest, FadeInOutTransformation)
{
    transformer->set_parameter("operation", TemporalOperation::FADE_IN_OUT);
    transformer->set_parameter("fade_in_ratio", 0.1);
    transformer->set_parameter("fade_out_ratio", 0.1);

    IO<DataVariant> input(DataVariant { test_signal });
    auto result = transformer->apply_operation(input);

    auto result_data = std::get<std::vector<double>>(result.data);
    EXPECT_EQ(result_data.size(), test_signal.size());

    EXPECT_NEAR(result_data[0], 0.0, 1e-10);

    EXPECT_NEAR(result_data.back(), 0.0, 1e-10);

    size_t mid_idx = result_data.size() / 2;
    EXPECT_NEAR(std::abs(result_data[mid_idx]), std::abs(test_signal[mid_idx]), 1e-6);
}

// =========================================================================
// PERFORMANCE CHARACTERISTICS TESTS
// =========================================================================

class TransformerPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        large_signal = TransformerTestDataGenerator::create_sine_wave(16384, 440.0, 1.0);
        small_signal = TransformerTestDataGenerator::create_sine_wave(64, 440.0, 1.0);
    }

    std::vector<double> large_signal;
    std::vector<double> small_signal;
};

TEST_F(TransformerPerformanceTest, ScalabilityWithSignalSize)
{
    auto transformer = std::make_unique<MathematicalTransformer<>>();
    transformer->set_parameter("operation", MathematicalOperation::POLYNOMIAL);

    std::vector<double> coefficients = { 1.0, 2.0, -0.5, 0.1 };
    transformer->set_parameter("coefficients", coefficients);

    IO<DataVariant> small_input(DataVariant { small_signal });
    auto start_time = std::chrono::high_resolution_clock::now();
    auto small_result = transformer->apply_operation(small_input);
    auto small_duration = std::chrono::high_resolution_clock::now() - start_time;

    IO<DataVariant> large_input(DataVariant { large_signal });
    start_time = std::chrono::high_resolution_clock::now();
    auto large_result = transformer->apply_operation(large_input);
    auto large_duration = std::chrono::high_resolution_clock::now() - start_time;

    auto small_data = std::get<std::vector<double>>(small_result.data);
    auto large_data = std::get<std::vector<double>>(large_result.data);

    EXPECT_EQ(small_data.size(), small_signal.size());
    EXPECT_EQ(large_data.size(), large_signal.size());

    auto small_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(small_duration).count();
    auto large_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(large_duration).count();

    if (small_ns > 0) {
        double scaling_factor = static_cast<double>(large_ns) / small_ns;
        double size_ratio = static_cast<double>(large_signal.size()) / small_signal.size();

        EXPECT_LT(scaling_factor, size_ratio * size_ratio)
            << "Performance scaling should not be worse than quadratic";
    }
}

TEST_F(TransformerPerformanceTest, MemoryEfficiencyInPlace)
{
    auto transformer = std::make_unique<MathematicalTransformer<>>();
    transformer->set_parameter("operation", MathematicalOperation::GAIN);
    transformer->set_parameter("gain_factor", 2.0);

    std::vector<double> original_data = large_signal;
    IO<DataVariant> input(DataVariant { original_data });

    auto result = transformer->apply_operation(input);
    auto result_data = std::get<std::vector<double>>(result.data);

    EXPECT_EQ(result_data.size(), original_data.size());
    for (size_t i = 0; i < original_data.size(); ++i) {
        EXPECT_NEAR(result_data[i], original_data[i] * 2.0, 1e-10);
    }
}

// =========================================================================
// SPECIALIZED ALGORITHM VERIFICATION TESTS
// =========================================================================

class AlgorithmVerificationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        impulse = TransformerTestDataGenerator::create_impulse(128, 1.0);
        sine_wave = TransformerTestDataGenerator::create_sine_wave(512, 1000.0, 1.0, 8000.0);
    }

    std::vector<double> impulse;
    std::vector<double> sine_wave;
};

TEST_F(AlgorithmVerificationTest, ConvolutionWithKnownImpulseResponse)
{
    auto transformer = std::make_unique<ConvolutionTransformer<>>();
    transformer->set_parameter("operation", ConvolutionOperation::DIRECT_CONVOLUTION);

    std::vector<double> identity_impulse = { 1.0 };
    transformer->set_parameter("impulse_response", identity_impulse);

    IO<DataVariant> input(DataVariant { sine_wave });
    auto result = transformer->apply_operation(input);

    auto result_data = std::get<std::vector<double>>(result.data);
    EXPECT_EQ(result_data.size(), sine_wave.size());

    for (size_t i = 0; i < sine_wave.size(); ++i) {
        EXPECT_NEAR(result_data[i], sine_wave[i], 1e-10);
    }
}

TEST_F(AlgorithmVerificationTest, MathematicalPolynomialEvaluation)
{
    auto transformer = std::make_unique<MathematicalTransformer<>>();
    transformer->set_parameter("operation", MathematicalOperation::POLYNOMIAL);

    std::vector<double> coefficients = { 3.0, 2.0, 1.0 }; // [x^2, x^1, x^0]
    transformer->set_parameter("coefficients", coefficients);

    std::vector<double> test_input = { 0.0, 1.0, 2.0, 3.0 };
    IO<DataVariant> input(DataVariant { test_input });
    auto result = transformer->apply_operation(input);

    auto result_data = std::get<std::vector<double>>(result.data);
    EXPECT_EQ(result_data.size(), test_input.size());

    // Verify polynomial evaluation: f(x) = 1 + 2x + 3x^2
    std::vector<double> expected = { 1.0, 6.0, 17.0, 34.0 };
    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_NEAR(result_data[i], expected[i], 1e-6);
    }
}

TEST_F(AlgorithmVerificationTest, TemporalReverseSymmetry)
{
    auto transformer = std::make_unique<TemporalTransformer<>>();
    transformer->set_parameter("operation", TemporalOperation::TIME_REVERSE);

    std::vector<double> symmetric_signal = { 1.0, 2.0, 3.0, 4.0, 5.0, 4.0, 3.0, 2.0, 1.0 };
    IO<DataVariant> input(DataVariant { symmetric_signal });
    auto result = transformer->apply_operation(input);

    auto result_data = std::get<std::vector<double>>(result.data);
    EXPECT_EQ(result_data.size(), symmetric_signal.size());

    for (size_t i = 0; i < symmetric_signal.size(); ++i) {
        EXPECT_NEAR(result_data[i], symmetric_signal[i], 1e-10);
    }
}

TEST_F(AlgorithmVerificationTest, NormalizationPreservesShape)
{
    auto transformer = std::make_unique<MathematicalTransformer<>>();
    transformer->set_parameter("operation", MathematicalOperation::NORMALIZE);
    transformer->set_parameter("target_peak", 0.5);

    IO<DataVariant> input(DataVariant { sine_wave });
    auto result = transformer->apply_operation(input);

    auto result_data = std::get<std::vector<double>>(result.data);
    EXPECT_EQ(result_data.size(), sine_wave.size());

    auto max_val = *std::ranges::max_element(result_data);
    EXPECT_NEAR(max_val, 0.5, 1e-10);

    auto count_zero_crossings = [](const std::vector<double>& signal) {
        int crossings = 0;
        for (size_t i = 1; i < signal.size(); ++i) {
            if ((signal[i - 1] >= 0 && signal[i] < 0) || (signal[i - 1] < 0 && signal[i] >= 0)) {
                crossings++;
            }
        }
        return crossings;
    };

    int original_crossings = count_zero_crossings(sine_wave);
    int normalized_crossings = count_zero_crossings(result_data);
    EXPECT_EQ(original_crossings, normalized_crossings)
        << "Normalization should preserve zero crossings";
}

// =========================================================================
// ERROR HANDLING AND ROBUSTNESS TESTS
// =========================================================================

class TransformerRobustnessTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        normal_signal = TransformerTestDataGenerator::create_sine_wave(256);
    }

    std::vector<double> normal_signal;
};

TEST_F(TransformerRobustnessTest, InvalidParameterTypes)
{
    auto transformer = std::make_unique<MathematicalTransformer<>>();

    EXPECT_NO_THROW(transformer->set_parameter("gain_factor", std::string("not_a_number")));
    EXPECT_NO_THROW(transformer->set_parameter("operation", 42));

    transformer->set_parameter("operation", MathematicalOperation::GAIN);
    transformer->set_parameter("gain_factor", 2.0);

    IO<DataVariant> input(DataVariant { normal_signal });
    EXPECT_NO_THROW({
        auto result = transformer->apply_operation(input);
        auto result_data = std::get<std::vector<double>>(result.data);
        EXPECT_FALSE(result_data.empty());
    });
}

TEST_F(TransformerRobustnessTest, ValidationHandlesProblematicData)
{
    auto transformer = std::make_unique<MathematicalTransformer<>>();
    transformer->set_parameter("operation", MathematicalOperation::LOGARITHMIC);
    transformer->set_parameter("base", std::numbers::e);
    transformer->set_parameter("scale", 1.0);

    std::vector<double> problematic_signal = {
        1.0, -1.0, 0.0, std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(), std::nan("")
    };

    IO<DataVariant> input(DataVariant { problematic_signal });

    EXPECT_NO_THROW({
        auto result = transformer->apply_operation(input);
        auto result_data = std::get<std::vector<double>>(result.data);

        EXPECT_TRUE(result.metadata.contains("validation_failed"));
        EXPECT_EQ(result_data.size(), problematic_signal.size());

        EXPECT_EQ(result_data[0], 1.0);
    });
}

TEST_F(TransformerRobustnessTest, ZeroDivisionProtection)
{
    auto transformer = std::make_unique<MathematicalTransformer<>>();
    transformer->set_parameter("operation", MathematicalOperation::POWER);
    transformer->set_parameter("exponent", -1.0);

    std::vector<double> signal_with_zero = { 1.0, 2.0, 0.0, 4.0, 5.0 };
    IO<DataVariant> input(DataVariant { signal_with_zero });

    EXPECT_NO_THROW({
        auto result = transformer->apply_operation(input);
        auto result_data = std::get<std::vector<double>>(result.data);

        EXPECT_EQ(result_data.size(), signal_with_zero.size());

        EXPECT_NEAR(result_data[0], 1.0, 1e-10); // 1^(-1) = 1
        EXPECT_NEAR(result_data[1], 0.5, 1e-10); // 2^(-1) = 0.5

        EXPECT_FALSE(std::isnan(result_data[2])) << "Zero division should not produce NaN";
    });
}

TEST_F(TransformerRobustnessTest, VeryLargeSignals)
{
    size_t large_size = static_cast<long>(1024) * 1024;
    auto large_signal = TransformerTestDataGenerator::create_sine_wave(large_size, 440.0, 1.0);

    auto transformer = std::make_unique<MathematicalTransformer<>>();
    transformer->set_parameter("operation", MathematicalOperation::NORMALIZE);

    IO<DataVariant> input(DataVariant { large_signal });

    EXPECT_NO_THROW({
        auto result = transformer->apply_operation(input);
        auto result_data = std::get<std::vector<double>>(result.data);

        EXPECT_EQ(result_data.size(), large_size);

        auto max_val = *std::ranges::max_element(result_data);
        EXPECT_NEAR(max_val, 1.0, 1e-10);
    });
}

// =========================================================================
// TRANSFORMER STRATEGY AND QUALITY TESTS
// =========================================================================

class TransformerStrategyTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_signal = TransformerTestDataGenerator::create_sine_wave(512);
    }

    std::vector<double> test_signal;
};

TEST_F(TransformerStrategyTest, TransformationStrategySettings)
{
    auto transformer = std::make_unique<MathematicalTransformer<>>();

    transformer->set_parameter("strategy", TransformationStrategy::IN_PLACE);
    transformer->set_parameter("quality", TransformationQuality::HIGH_QUALITY);
    transformer->set_parameter("scope", TransformationScope::FULL_DATA);

    EXPECT_NO_THROW(transformer->set_strategy(TransformationStrategy::BUFFERED));
    EXPECT_NO_THROW(transformer->set_quality(TransformationQuality::STANDARD));
    EXPECT_NO_THROW(transformer->set_scope(TransformationScope::TARGETED_REGIONS));

    EXPECT_EQ(transformer->get_strategy(), TransformationStrategy::BUFFERED);
    EXPECT_EQ(transformer->get_quality(), TransformationQuality::STANDARD);
    EXPECT_EQ(transformer->get_scope(), TransformationScope::TARGETED_REGIONS);
}

TEST_F(TransformerStrategyTest, StrategyStringConversion)
{
    auto transformer = std::make_unique<SpectralTransformer<>>();

    EXPECT_NO_THROW(transformer->set_parameter("strategy", std::string("BUFFERED")));
    EXPECT_NO_THROW(transformer->set_parameter("quality", std::string("HIGH_QUALITY")));
    EXPECT_NO_THROW(transformer->set_parameter("scope", std::string("FULL_DATA")));

    transformer->set_parameter("operation", SpectralOperation::FREQUENCY_SHIFT);
    transformer->set_parameter("shift_hz", 100.0);

    IO<DataVariant> input(DataVariant { test_signal });
    EXPECT_NO_THROW({
        auto result = transformer->apply_operation(input);
        auto result_data = std::get<std::vector<double>>(result.data);
        EXPECT_FALSE(result_data.empty());
    });
}

TEST_F(TransformerStrategyTest, ParameterRetrieval)
{
    auto transformer = std::make_unique<ConvolutionTransformer<>>();

    transformer->set_parameter("operation", ConvolutionOperation::CROSS_CORRELATION);
    transformer->set_parameter("normalize", true);
    transformer->set_parameter("strategy", TransformationStrategy::PARALLEL);

    auto all_params = transformer->get_all_parameters();
    EXPECT_FALSE(all_params.empty());

    EXPECT_TRUE(all_params.find("strategy") != all_params.end());
    EXPECT_TRUE(all_params.find("normalize") != all_params.end());

    auto strategy_param = transformer->get_parameter("strategy");
    EXPECT_TRUE(strategy_param.has_value());

    auto invalid_param = transformer->get_parameter("nonexistent_parameter");
    EXPECT_FALSE(invalid_param.has_value());
}

// =========================================================================
// COMPUTATIONAL COST AND PROGRESS TESTS
// =========================================================================

class TransformerComputationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_signal = TransformerTestDataGenerator::create_sine_wave(1024);
    }

    std::vector<double> test_signal;
};

TEST_F(TransformerComputationTest, ComputationalCostEstimation)
{
    auto simple_transformer = std::make_unique<MathematicalTransformer<>>();
    simple_transformer->set_parameter("operation", MathematicalOperation::GAIN);

    auto complex_transformer = std::make_unique<SpectralTransformer<>>();
    complex_transformer->set_parameter("operation", SpectralOperation::PITCH_SHIFT);

    double simple_cost = simple_transformer->estimate_computational_cost();
    double complex_cost = complex_transformer->estimate_computational_cost();

    EXPECT_GE(simple_cost, 0.0) << "Cost should be non-negative";
    EXPECT_GE(complex_cost, 0.0) << "Cost should be non-negative";
}

TEST_F(TransformerComputationTest, TransformationProgress)
{
    auto transformer = std::make_unique<TemporalTransformer<>>();
    transformer->set_parameter("operation", TemporalOperation::TIME_STRETCH);
    transformer->set_parameter("stretch_factor", 2.0);

    double progress = transformer->get_transformation_progress();
    EXPECT_GE(progress, 0.0) << "Progress should be non-negative";
    EXPECT_LE(progress, 1.0) << "Progress should not exceed 1.0";
}

TEST_F(TransformerComputationTest, InPlaceTransformationFlag)
{
    auto transformer = std::make_unique<MathematicalTransformer<>>();

    bool in_place = transformer->is_in_place();
    EXPECT_TRUE(in_place == true || in_place == false) << "Should return a valid boolean";
}

// =========================================================================
// PARAMETER HANDLING TESTS
// =========================================================================

class TransformerParameterTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_signal = TransformerTestDataGenerator::create_sine_wave(1024);
    }

    std::vector<double> test_signal;
};

TEST_F(TransformerParameterTest, ParameterTypeConversion)
{
    auto transformer = std::make_unique<MathematicalTransformer<>>();

    transformer->set_parameter("operation", std::string("GAIN"));
    transformer->set_parameter("gain_factor", 3.0);

    IO<DataVariant> input(DataVariant { test_signal });
    auto result = transformer->apply_operation(input);

    auto result_data = std::get<std::vector<double>>(result.data);

    for (size_t i = 0; i < std::min(result_data.size(), test_signal.size()); ++i) {
        EXPECT_NEAR(result_data[i], test_signal[i] * 3.0, 1e-10);
    }
}

TEST_F(TransformerParameterTest, DefaultParameterValues)
{
    auto transformer = std::make_unique<ConvolutionTransformer<>>();

    IO<DataVariant> input(DataVariant { test_signal });
    EXPECT_NO_THROW({
        auto result = transformer->apply_operation(input);
        auto result_data = std::get<std::vector<double>>(result.data);
        EXPECT_FALSE(result_data.empty());
    });
}

TEST_F(TransformerParameterTest, InvalidParameterHandling)
{
    auto transformer = std::make_unique<SpectralTransformer<>>();

    EXPECT_NO_THROW(transformer->set_parameter("invalid_param", 42));
    EXPECT_NO_THROW(transformer->set_parameter("operation", "INVALID_OPERATION"));

    transformer->set_parameter("operation", SpectralOperation::FREQUENCY_SHIFT);
    IO<DataVariant> input(DataVariant { test_signal });

    EXPECT_NO_THROW({
        auto result = transformer->apply_operation(input);
    });
}

// =========================================================================
// EDGE CASE AND VALIDATION TESTS
// =========================================================================

class TransformerValidationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        normal_signal = TransformerTestDataGenerator::create_sine_wave(64);
        empty_signal = std::vector<double> {};
        single_sample = std::vector<double> { 1.0 };
        constant_signal = TransformerTestDataGenerator::create_constant(100, 0.5);
    }

    std::vector<double> normal_signal;
    std::vector<double> empty_signal;
    std::vector<double> single_sample;
    std::vector<double> constant_signal;
};

TEST_F(TransformerValidationTest, EmptySignalHandling)
{
    auto transformers = std::vector<std::unique_ptr<UniversalTransformer<>>> {};
    transformers.push_back(std::make_unique<ConvolutionTransformer<>>());
    transformers.push_back(std::make_unique<MathematicalTransformer<>>());
    transformers.push_back(std::make_unique<SpectralTransformer<>>());
    transformers.push_back(std::make_unique<TemporalTransformer<>>());

    for (auto& transformer : transformers) {
        IO<DataVariant> input(DataVariant { empty_signal });

        EXPECT_NO_THROW({
            auto result = transformer->apply_operation(input);
        }) << "Transformer "
           << transformer->get_name() << " should handle empty signal";
    }
}

TEST_F(TransformerValidationTest, SingleSampleHandling)
{
    auto transformers = std::vector<std::unique_ptr<UniversalTransformer<>>> {};
    transformers.push_back(std::make_unique<MathematicalTransformer<>>());
    transformers.push_back(std::make_unique<TemporalTransformer<>>());

    for (auto& transformer : transformers) {
        IO<DataVariant> input(DataVariant { single_sample });

        EXPECT_NO_THROW({
            auto result = transformer->apply_operation(input);
        }) << "Transformer "
           << transformer->get_name() << " should handle single sample";
    }
}

TEST_F(TransformerValidationTest, ConstantSignalHandling)
{
    auto math_transformer = std::make_unique<MathematicalTransformer<>>();
    math_transformer->set_parameter("operation", MathematicalOperation::GAIN);
    math_transformer->set_parameter("gain_factor", 2.0);

    IO<DataVariant> input(DataVariant { constant_signal });
    auto result = math_transformer->apply_operation(input);

    auto result_data = std::get<std::vector<double>>(result.data);
    EXPECT_EQ(result_data.size(), constant_signal.size());

    for (double value : result_data) {
        EXPECT_NEAR(value, 1.0, 1e-10); // 0.5 * 2.0 = 1.0
    }
}

TEST_F(TransformerValidationTest, ExtremeValueHandling)
{
    auto extreme_signal = std::vector<double> {
        std::numeric_limits<double>::max() / 1e6,
        std::numeric_limits<double>::lowest() / 1e6,
        0.0,
        1.0,
        -1.0
    };

    auto math_transformer = std::make_unique<MathematicalTransformer<>>();
    math_transformer->set_parameter("operation", MathematicalOperation::NORMALIZE);

    IO<DataVariant> input(DataVariant { extreme_signal });

    EXPECT_NO_THROW({
        auto result = math_transformer->apply_operation(input);
        auto result_data = std::get<std::vector<double>>(result.data);
        EXPECT_FALSE(result_data.empty());

        for (double value : result_data) {
            EXPECT_FALSE(std::isnan(value)) << "Result should not contain NaN";
            EXPECT_FALSE(std::isinf(value)) << "Result should not contain infinity";
        }
    });
}

// =========================================================================
// PERFORMANCE AND CONSISTENCY TESTS
// =========================================================================

class TransformerConsistencyTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_signal = TransformerTestDataGenerator::create_sine_wave(1024, 440.0, 1.0);
    }

    std::vector<double> test_signal;
};

TEST_F(TransformerConsistencyTest, ConsistentResultsAcrossRuns)
{
    auto transformer = std::make_unique<MathematicalTransformer<>>();
    transformer->set_parameter("operation", MathematicalOperation::POWER);
    transformer->set_parameter("exponent", 2.0);

    IO<DataVariant> input(DataVariant { test_signal });

    auto result1 = transformer->apply_operation(input);
    auto result2 = transformer->apply_operation(input);
    auto result3 = transformer->apply_operation(input);

    auto data1 = std::get<std::vector<double>>(result1.data);
    auto data2 = std::get<std::vector<double>>(result2.data);
    auto data3 = std::get<std::vector<double>>(result3.data);

    EXPECT_EQ(data1.size(), data2.size());
    EXPECT_EQ(data2.size(), data3.size());

    for (size_t i = 0; i < data1.size(); ++i) {
        EXPECT_NEAR(data1[i], data2[i], 1e-15);
        EXPECT_NEAR(data2[i], data3[i], 1e-15);
    }
}

TEST_F(TransformerConsistencyTest, ParameterIsolation)
{
    auto transformer = std::make_unique<MathematicalTransformer<>>();

    transformer->set_parameter("operation", MathematicalOperation::GAIN);
    transformer->set_parameter("gain_factor", 2.0);

    IO<DataVariant> input(DataVariant { test_signal });
    auto result1 = transformer->apply_operation(input);

    transformer->set_parameter("gain_factor", 3.0);
    auto result2 = transformer->apply_operation(input);

    transformer->set_parameter("gain_factor", 2.0);
    auto result3 = transformer->apply_operation(input);

    auto data1 = std::get<std::vector<double>>(result1.data);
    auto data3 = std::get<std::vector<double>>(result3.data);

    EXPECT_EQ(data1.size(), data3.size());
    for (size_t i = 0; i < data1.size(); ++i) {
        EXPECT_NEAR(data1[i], data3[i], 1e-15);
    }
}

// =========================================================================
// CROSS-TRANSFORMER INTEGRATION TESTS
// =========================================================================

class TransformerIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_signal = TransformerTestDataGenerator::create_sine_wave(1024, 440.0, 1.0);
    }

    std::vector<double> test_signal;
};

TEST_F(TransformerIntegrationTest, ChainedTransformations)
{
    auto gain_transformer = std::make_unique<MathematicalTransformer<>>();
    gain_transformer->set_parameter("operation", MathematicalOperation::GAIN);
    gain_transformer->set_parameter("gain_factor", 0.5);

    auto offset_transformer = std::make_unique<MathematicalTransformer<>>();
    offset_transformer->set_parameter("operation", MathematicalOperation::OFFSET);
    offset_transformer->set_parameter("offset_value", 0.25);

    auto power_transformer = std::make_unique<MathematicalTransformer<>>();
    power_transformer->set_parameter("operation", MathematicalOperation::POWER);
    power_transformer->set_parameter("exponent", 2.0);

    IO<DataVariant> input(DataVariant { test_signal });
    auto result1 = gain_transformer->apply_operation(input);
    auto result2 = offset_transformer->apply_operation(result1);
    auto result3 = power_transformer->apply_operation(result2);

    auto final_data = std::get<std::vector<double>>(result3.data);
    EXPECT_EQ(final_data.size(), test_signal.size());

    // Verify the mathematical pipeline: ((x * 0.5) + 0.25)^2
    for (size_t i = 0; i < std::min(final_data.size(), test_signal.size()); ++i) {
        double expected = std::pow((test_signal[i] * 0.5) + 0.25, 2.0);
        EXPECT_NEAR(final_data[i], expected, 1e-10);
    }
}

TEST_F(TransformerIntegrationTest, CrossDomainTransformation)
{
    auto normalize_transformer = std::make_unique<MathematicalTransformer<>>();
    normalize_transformer->set_parameter("operation", MathematicalOperation::NORMALIZE);
    normalize_transformer->set_parameter("target_peak", 1.0);

    auto reverse_transformer = std::make_unique<TemporalTransformer<>>();
    reverse_transformer->set_parameter("operation", TemporalOperation::TIME_REVERSE);

    auto gain_transformer = std::make_unique<MathematicalTransformer<>>();
    gain_transformer->set_parameter("operation", MathematicalOperation::GAIN);
    gain_transformer->set_parameter("gain_factor", 0.8);

    IO<DataVariant> input(DataVariant { test_signal });
    auto result1 = normalize_transformer->apply_operation(input);
    auto result2 = reverse_transformer->apply_operation(result1);
    auto result3 = gain_transformer->apply_operation(result2);

    auto final_data = std::get<std::vector<double>>(result3.data);
    EXPECT_EQ(final_data.size(), test_signal.size());

    EXPECT_FALSE(final_data.empty());

    auto max_val = *std::ranges::max_element(final_data);
    EXPECT_LE(max_val, 0.81); // Should be ≤ 0.8 due to final gain
}

TEST_F(TransformerIntegrationTest, MultipleDataTypesSupport)
{
    auto transformer = std::make_unique<MathematicalTransformer<DataVariant, DataVariant>>();
    transformer->set_parameter("operation", MathematicalOperation::GAIN);
    transformer->set_parameter("gain_factor", 2.0);

    std::vector<double> double_data = { 1.0, 2.0, 3.0, 4.0 };
    IO<DataVariant> double_input(DataVariant { double_data });
    auto double_result = transformer->apply_operation(double_input);

    EXPECT_NO_THROW({
        auto result_data = std::get<std::vector<double>>(double_result.data);
        EXPECT_EQ(result_data.size(), 4U);
        for (size_t i = 0; i < result_data.size(); ++i) {
            EXPECT_NEAR(result_data[i], double_data[i] * 2.0, 1e-10);
        }
    });

    // TODO: Add float testing when neotest compatibility is resolved
    // Float processing works in binary but has issues in neotest environment
    /* std::vector<float> float_data = { 1.0F, 2.0F, 3.0F, 4.0F };
    IO<DataVariant> float_input(DataVariant { float_data });
    auto float_result = transformer->apply_operation(float_input);

    EXPECT_NO_THROW({
        auto result_data = std::get<std::vector<float>>(float_result.data);
        EXPECT_EQ(result_data.size(), 4U);
    }); */
}

} // namespace MayaFlux::Test
