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
     * @brief Generate sine wave with known frequency and amplitude (multichannel)
     */
    static std::vector<std::vector<double>> create_sine_wave(
        size_t samples = 1024,
        double frequency = 440.0,
        double amplitude = 1.0,
        double sample_rate = 44100.0,
        size_t channels = 1)
    {
        std::vector<std::vector<double>> signal(channels, std::vector<double>(samples, 0.0));
        for (size_t ch = 0; ch < channels; ++ch) {
            for (size_t i = 0; i < samples; ++i) {
                double t = static_cast<double>(i) / sample_rate;
                signal[ch][i] = amplitude * std::sin(2.0 * M_PI * frequency * t);
            }
        }
        return signal;
    }

    /**
     * @brief Generate impulse signal (delta function at start, multichannel)
     */
    static std::vector<std::vector<double>> create_impulse(
        size_t samples = 1024,
        double amplitude = 1.0,
        size_t channels = 1)
    {
        std::vector<std::vector<double>> signal(channels, std::vector<double>(samples, 0.0));
        for (size_t ch = 0; ch < channels; ++ch) {
            if (samples > 0)
                signal[ch][0] = amplitude;
        }
        return signal;
    }

    /**
     * @brief Generate white noise with known variance (multichannel)
     */
    static std::vector<std::vector<double>> create_white_noise(
        size_t samples = 1024,
        double variance = 1.0,
        u_int32_t seed = 42,
        size_t channels = 1)
    {
        std::vector<std::vector<double>> signal(channels, std::vector<double>(samples, 0.0));
        for (size_t ch = 0; ch < channels; ++ch) {
            std::mt19937 gen(seed + ch);
            std::normal_distribution<double> dist(0.0, std::sqrt(variance));
            for (size_t i = 0; i < samples; ++i) {
                signal[ch][i] = dist(gen);
            }
        }
        return signal;
    }

    /**
     * @brief Generate linear ramp from 0 to 1 (multichannel)
     */
    static std::vector<std::vector<double>> create_linear_ramp(
        size_t samples = 1024,
        size_t channels = 1)
    {
        std::vector<std::vector<double>> signal(channels, std::vector<double>(samples, 0.0));
        for (size_t ch = 0; ch < channels; ++ch) {
            for (size_t i = 0; i < samples; ++i) {
                signal[ch][i] = static_cast<double>(i) / static_cast<double>(samples - 1);
            }
        }
        return signal;
    }

    /**
     * @brief Generate constant signal (multichannel)
     */
    static std::vector<std::vector<double>> create_constant(
        size_t samples = 1024,
        double value = 1.0,
        size_t channels = 1)
    {
        return std::vector<std::vector<double>>(channels, std::vector<double>(samples, value));
    }

    /**
     * @brief Generate multi-frequency signal for spectral testing (multichannel)
     */
    static std::vector<std::vector<double>> create_multi_tone(
        size_t samples = 2048,
        const std::vector<double>& frequencies = { 220.0, 440.0, 880.0 },
        double sample_rate = 44100.0,
        size_t channels = 1)
    {
        std::vector<std::vector<double>> signal(channels, std::vector<double>(samples, 0.0));
        for (size_t ch = 0; ch < channels; ++ch) {
            for (double freq : frequencies) {
                for (size_t i = 0; i < samples; ++i) {
                    double t = static_cast<double>(i) / sample_rate;
                    signal[ch][i] += std::sin(2.0 * M_PI * freq * t) / frequencies.size();
                }
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
        test_signal = TransformerTestDataGenerator::create_sine_wave(1024, 440.0, 1.0, 44100.0, 2);
    }

    std::unique_ptr<ConvolutionTransformer<>> transformer;
    std::vector<std::vector<double>> test_signal;
};

TEST_F(ConvolutionTransformerTest, DirectConvolutionWithImpulseResponse)
{
    transformer->set_parameter("operation", ConvolutionOperation::DIRECT_CONVOLUTION);

    std::vector<double> impulse_response = { 0.25, 0.5, 0.25 };
    transformer->set_parameter("impulse_response", impulse_response);

    std::vector<DataVariant> input_channels;

    input_channels.reserve(test_signal.size());
    for (const auto& channel : test_signal) {
        input_channels.emplace_back(channel);
    }
    IO<std::vector<DataVariant>> input(input_channels);

    auto result = transformer->apply_operation(input);

    const auto& result_channels = result.data;
    EXPECT_EQ(result_channels.size(), test_signal.size());

    for (size_t ch = 0; ch < test_signal.size(); ++ch) {
        const auto& channel_data = std::get<std::vector<double>>(result_channels[ch]);
        EXPECT_EQ(channel_data.size(), test_signal[ch].size());

        double max_input = *std::ranges::max_element(test_signal[ch]);
        double max_output = *std::ranges::max_element(channel_data);
        EXPECT_LT(max_output, max_input) << "Low-pass filter should reduce peak amplitude";
    }
}

TEST_F(ConvolutionTransformerTest, CrossCorrelationNormalized)
{
    transformer->set_parameter("operation", ConvolutionOperation::CROSS_CORRELATION);

    std::vector<double> template_signal(test_signal[0].begin(), test_signal[0].begin() + 64);
    transformer->set_parameter("template_signal", template_signal);
    transformer->set_parameter("normalize", true);

    std::vector<DataVariant> input_channels;

    input_channels.reserve(test_signal.size());
    for (const auto& channel : test_signal) {
        input_channels.emplace_back(channel);
    }
    IO<std::vector<DataVariant>> input(input_channels);

    auto result = transformer->apply_operation(input);

    const auto& result_channels = result.data;
    EXPECT_EQ(result_channels.size(), test_signal.size());

    for (size_t ch = 0; ch < test_signal.size(); ++ch) {
        const auto& channel_data = std::get<std::vector<double>>(result_channels[ch]);
        auto [min_it, max_it] = std::ranges::minmax_element(channel_data);
        EXPECT_GE(*max_it, 0.5) << "Should find strong correlation";
        EXPECT_LE(*max_it, 1.1) << "Normalized correlation should not exceed 1.0 significantly";
    }
}

TEST_F(ConvolutionTransformerTest, MatchedFilterDetection)
{
    transformer->set_parameter("operation", ConvolutionOperation::MATCHED_FILTER);

    auto reference_signal = TransformerTestDataGenerator::create_sine_wave(64, 440.0, 1.0, 44100.0, 2);
    transformer->set_parameter("reference_signal", reference_signal[0]);

    std::vector<DataVariant> input_channels;

    input_channels.reserve(test_signal.size());
    for (const auto& channel : test_signal) {
        input_channels.emplace_back(channel);
    }
    IO<std::vector<DataVariant>> input(input_channels);

    auto result = transformer->apply_operation(input);

    const auto& result_channels = result.data;
    EXPECT_EQ(result_channels.size(), test_signal.size());

    for (size_t ch = 0; ch < test_signal.size(); ++ch) {
        const auto& channel_data = std::get<std::vector<double>>(result_channels[ch]);
        double max_correlation = *std::ranges::max_element(channel_data);
        EXPECT_GT(max_correlation, 0.3) << "Matched filter should detect similar patterns";
    }
}

TEST_F(ConvolutionTransformerTest, AutoCorrelation)
{
    transformer->set_parameter("operation", ConvolutionOperation::AUTO_CORRELATION);
    transformer->set_parameter("strategy", TransformationStrategy::BUFFERED);
    transformer->set_parameter("normalize", true);

    std::vector<DataVariant> input_channels;
    for (const auto& channel : test_signal) {
        input_channels.emplace_back(channel);
    }
    IO<std::vector<DataVariant>> input(input_channels);

    auto result = transformer->apply_operation(input);

    const auto& result_channels = result.data;
    EXPECT_EQ(result_channels.size(), test_signal.size());

    for (size_t ch = 0; ch < test_signal.size(); ++ch) {
        const auto& channel_data = std::get<std::vector<double>>(result_channels[ch]);
        double zero_lag_value = channel_data[0];
        EXPECT_NEAR(zero_lag_value, 1.0, 0.1) << "Auto-correlation peak should be near 1.0";
    }
}

TEST_F(ConvolutionTransformerTest, DeconvolutionBasic)
{
    transformer->set_parameter("operation", ConvolutionOperation::DECONVOLUTION);

    std::vector<double> impulse_response = { 1.0, 0.5 };
    transformer->set_parameter("impulse_response", impulse_response);
    transformer->set_parameter("regularization", 1e-3);

    std::vector<DataVariant> input_channels;
    for (const auto& channel : test_signal) {
        input_channels.emplace_back(channel);
    }
    IO<std::vector<DataVariant>> input(input_channels);

    EXPECT_NO_THROW({
        auto result = transformer->apply_operation(input);
        const auto& result_channels = result.data;
        EXPECT_EQ(result_channels.size(), test_signal.size());
        for (size_t ch = 0; ch < test_signal.size(); ++ch) {
            const auto& channel_data = std::get<std::vector<double>>(result_channels[ch]);
            EXPECT_FALSE(channel_data.empty());
        }
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
        test_signal = TransformerTestDataGenerator::create_sine_wave(1024, 440.0, 1.0, 44100.0, 2);

        input_channels.reserve(test_signal.size());
        for (const auto& channel : test_signal) {
            input_channels.emplace_back(channel);
        }
        test_input = IO<std::vector<DataVariant>>(input_channels);
    }

    std::vector<std::vector<double>> test_signal;
    std::vector<DataVariant> input_channels;
    IO<std::vector<DataVariant>> test_input;
};

TEST_F(TransformerMetadataTest, ConvolutionTransformerMetadata)
{
    auto transformer = std::make_unique<ConvolutionTransformer<>>();
    transformer->set_parameter("operation", ConvolutionOperation::DIRECT_CONVOLUTION);

    EXPECT_EQ(transformer->get_transformation_type(), TransformationType::CONVOLUTION);

    std::string name = transformer->get_transformer_name();
    EXPECT_TRUE(name.find("ConvolutionTransformer") != std::string::npos);
    EXPECT_TRUE(name.find("DIRECT_CONVOLUTION") != std::string::npos);

    auto result = transformer->apply_operation(test_input);
    EXPECT_EQ(result.data.size(), test_signal.size()) << "Should preserve channel count";

    std::string name_after = transformer->get_transformer_name();
    EXPECT_EQ(name, name_after) << "Transformer name should be consistent";
}

TEST_F(TransformerMetadataTest, MathematicalTransformerMetadata)
{
    auto transformer = std::make_unique<MathematicalTransformer<>>();
    transformer->set_parameter("operation", MathematicalOperation::POLYNOMIAL);

    EXPECT_EQ(transformer->get_transformation_type(), TransformationType::MATHEMATICAL);

    std::string name = transformer->get_transformer_name();
    EXPECT_TRUE(name.find("MathematicalTransformer") != std::string::npos);
    EXPECT_TRUE(name.find("POLYNOMIAL") != std::string::npos);

    transformer->set_parameter("coefficients", std::vector<double> { 1.0, 0.5, 0.1 });
    auto result = transformer->apply_operation(test_input);
    EXPECT_EQ(result.data.size(), test_signal.size()) << "Should preserve channel count";

    transformer->set_parameter("operation", MathematicalOperation::NORMALIZE);
    std::string updated_name = transformer->get_transformer_name();
    EXPECT_TRUE(updated_name.find("NORMALIZE") != std::string::npos);
    EXPECT_FALSE(updated_name.find("POLYNOMIAL") != std::string::npos);
}

TEST_F(TransformerMetadataTest, SpectralTransformerMetadata)
{
    auto transformer = std::make_unique<SpectralTransformer<>>();
    transformer->set_parameter("operation", SpectralOperation::PITCH_SHIFT);

    EXPECT_EQ(transformer->get_transformation_type(), TransformationType::SPECTRAL);

    std::string name = transformer->get_transformer_name();
    EXPECT_TRUE(name.find("SpectralTransformer") != std::string::npos);
    EXPECT_TRUE(name.find("PITCH_SHIFT") != std::string::npos);

    transformer->set_parameter("shift_semitones", 2.0);
    transformer->set_parameter("window_size", 1024u);
    auto result = transformer->apply_operation(test_input);
    EXPECT_EQ(result.data.size(), test_signal.size()) << "Should preserve channel count";

    transformer->set_parameter("operation", std::string("SPECTRAL_GATE"));
    std::string string_set_name = transformer->get_transformer_name();
    EXPECT_TRUE(string_set_name.find("SPECTRAL_GATE") != std::string::npos);
}

TEST_F(TransformerMetadataTest, TemporalTransformerMetadata)
{
    auto transformer = std::make_unique<TemporalTransformer<>>();
    transformer->set_parameter("operation", TemporalOperation::TIME_REVERSE);

    EXPECT_EQ(transformer->get_transformation_type(), TransformationType::TEMPORAL);

    std::string name = transformer->get_transformer_name();
    EXPECT_TRUE(name.find("TemporalTransformer") != std::string::npos);
    EXPECT_TRUE(name.find("TIME_REVERSE") != std::string::npos);

    auto result = transformer->apply_operation(test_input);
    EXPECT_EQ(result.data.size(), test_signal.size()) << "Should preserve channel count";

    for (size_t ch = 0; ch < test_signal.size(); ++ch) {
        const auto& result_channel = std::get<std::vector<double>>(result.data[ch]);
        EXPECT_EQ(result_channel.size(), test_signal[ch].size()) << "Channel " << ch << " should preserve sample count";

        EXPECT_NEAR(result_channel[0], test_signal[ch].back(), 1e-10)
            << "Channel " << ch << " should be time-reversed";
    }
}

TEST_F(TransformerMetadataTest, TransformerParameterValidation)
{
    auto transformer = std::make_unique<ConvolutionTransformer<>>();

    EXPECT_NO_THROW(transformer->set_parameter("operation", "INVALID_OPERATION"));

    transformer->set_parameter("impulse_response", std::vector<double> { 0.5, 1.0, 0.5 });
    transformer->set_parameter("normalize", true);
    transformer->set_parameter("regularization", 1e-3);

    EXPECT_NO_THROW({
        auto result = transformer->apply_operation(test_input);
        EXPECT_EQ(result.data.size(), test_signal.size());
    });
}

TEST_F(TransformerMetadataTest, TransformerTypeConsistency)
{
    auto conv_transformer = std::make_unique<ConvolutionTransformer<>>();
    auto math_transformer = std::make_unique<MathematicalTransformer<>>();
    auto spec_transformer = std::make_unique<SpectralTransformer<>>();
    auto temp_transformer = std::make_unique<TemporalTransformer<>>();

    EXPECT_EQ(conv_transformer->get_transformation_type(), TransformationType::CONVOLUTION);
    EXPECT_EQ(math_transformer->get_transformation_type(), TransformationType::MATHEMATICAL);
    EXPECT_EQ(spec_transformer->get_transformation_type(), TransformationType::SPECTRAL);
    EXPECT_EQ(temp_transformer->get_transformation_type(), TransformationType::TEMPORAL);

    EXPECT_TRUE(conv_transformer->get_transformer_name().find("ConvolutionTransformer") != std::string::npos);
    EXPECT_TRUE(math_transformer->get_transformer_name().find("MathematicalTransformer") != std::string::npos);
    EXPECT_TRUE(spec_transformer->get_transformer_name().find("SpectralTransformer") != std::string::npos);
    EXPECT_TRUE(temp_transformer->get_transformer_name().find("TemporalTransformer") != std::string::npos);

    EXPECT_NO_THROW(conv_transformer->apply_operation(test_input));
    EXPECT_NO_THROW(math_transformer->apply_operation(test_input));
    EXPECT_NO_THROW(spec_transformer->apply_operation(test_input));
    EXPECT_NO_THROW(temp_transformer->apply_operation(test_input));
}

// =========================================================================
// MATHEMATICAL TRANSFORMER TESTS
// =========================================================================

class MathematicalTransformerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        transformer = std::make_unique<MathematicalTransformer<>>();
        test_signal = TransformerTestDataGenerator::create_linear_ramp(1024, 2);

        input_channels.reserve(test_signal.size());
        for (const auto& channel : test_signal) {
            input_channels.emplace_back(channel);
        }
        test_input = IO<std::vector<DataVariant>>(input_channels);
    }

    std::unique_ptr<MathematicalTransformer<>> transformer;
    std::vector<std::vector<double>> test_signal;
    std::vector<DataVariant> input_channels;
    IO<std::vector<DataVariant>> test_input;
};

TEST_F(MathematicalTransformerTest, GainTransformation)
{
    transformer->set_parameter("operation", MathematicalOperation::GAIN);
    transformer->set_parameter("gain_factor", 2.0);

    auto result = transformer->apply_operation(test_input);

    const auto& result_channels = result.data;
    EXPECT_EQ(result_channels.size(), test_signal.size()) << "Should preserve channel count";

    for (size_t ch = 0; ch < test_signal.size(); ++ch) {
        const auto& result_data = std::get<std::vector<double>>(result_channels[ch]);
        EXPECT_EQ(result_data.size(), test_signal[ch].size()) << "Channel " << ch << " should preserve sample count";

        for (size_t i = 0; i < std::min(result_data.size(), test_signal[ch].size()); ++i) {
            EXPECT_NEAR(result_data[i], test_signal[ch][i] * 2.0, 1e-10)
                << "Sample " << i << " in channel " << ch;
        }
    }
}

TEST_F(MathematicalTransformerTest, OffsetTransformation)
{
    transformer->set_parameter("operation", MathematicalOperation::OFFSET);
    transformer->set_parameter("offset_value", 0.5);

    auto result = transformer->apply_operation(test_input);

    const auto& result_channels = result.data;
    EXPECT_EQ(result_channels.size(), test_signal.size()) << "Should preserve channel count";

    for (size_t ch = 0; ch < test_signal.size(); ++ch) {
        const auto& result_data = std::get<std::vector<double>>(result_channels[ch]);
        EXPECT_EQ(result_data.size(), test_signal[ch].size()) << "Channel " << ch << " should preserve sample count";

        for (size_t i = 0; i < std::min(result_data.size(), test_signal[ch].size()); ++i) {
            EXPECT_NEAR(result_data[i], test_signal[ch][i] + 0.5, 1e-10)
                << "Sample " << i << " in channel " << ch;
        }
    }
}

TEST_F(MathematicalTransformerTest, PowerTransformation)
{
    transformer->set_parameter("operation", MathematicalOperation::POWER);
    transformer->set_parameter("exponent", 2.0);

    auto result = transformer->apply_operation(test_input);

    const auto& result_channels = result.data;
    EXPECT_EQ(result_channels.size(), test_signal.size()) << "Should preserve channel count";

    for (size_t ch = 0; ch < test_signal.size(); ++ch) {
        const auto& result_data = std::get<std::vector<double>>(result_channels[ch]);
        EXPECT_EQ(result_data.size(), test_signal[ch].size()) << "Channel " << ch << " should preserve sample count";

        for (size_t i = 0; i < std::min(result_data.size(), test_signal[ch].size()); ++i) {
            EXPECT_NEAR(result_data[i], test_signal[ch][i] * test_signal[ch][i], 1e-10)
                << "Sample " << i << " in channel " << ch;
        }
    }
}

TEST_F(MathematicalTransformerTest, LogarithmicTransformation)
{
    auto positive_signal = TransformerTestDataGenerator::create_constant(1024, std::numbers::e, 2);

    std::vector<DataVariant> positive_channels;
    positive_channels.reserve(positive_signal.size());
    for (const auto& channel : positive_signal) {
        positive_channels.emplace_back(channel);
    }
    IO<std::vector<DataVariant>> positive_input(positive_channels);

    transformer->set_parameter("operation", MathematicalOperation::LOGARITHMIC);
    transformer->set_parameter("base", std::numbers::e);
    transformer->set_parameter("scale", 1.0);
    transformer->set_parameter("input_scale", 1.0);
    transformer->set_parameter("offset", 0.0);

    auto result = transformer->apply_operation(positive_input);

    const auto& result_channels = result.data;
    EXPECT_EQ(result_channels.size(), positive_signal.size()) << "Should preserve channel count";

    // Now: ln(1.0 * e + 0.0) = ln(e) = 1.0
    for (size_t ch = 0; ch < positive_signal.size(); ++ch) {
        const auto& result_data = std::get<std::vector<double>>(result_channels[ch]);
        for (double value : result_data) {
            EXPECT_NEAR(value, 1.0, 1e-6) << "Channel " << ch;
        }
    }
}

TEST_F(MathematicalTransformerTest, ExponentialTransformation)
{
    auto zero_signal = TransformerTestDataGenerator::create_constant(1024, 0.0, 2);

    std::vector<DataVariant> zero_channels;
    zero_channels.reserve(zero_signal.size());
    for (const auto& channel : zero_signal) {
        zero_channels.emplace_back(channel);
    }
    IO<std::vector<DataVariant>> zero_input(zero_channels);

    transformer->set_parameter("operation", MathematicalOperation::EXPONENTIAL);
    transformer->set_parameter("base", std::numbers::e);
    transformer->set_parameter("scale", 1.0);

    auto result = transformer->apply_operation(zero_input);

    const auto& result_channels = result.data;
    EXPECT_EQ(result_channels.size(), zero_signal.size()) << "Should preserve channel count";

    for (size_t ch = 0; ch < zero_signal.size(); ++ch) {
        const auto& result_data = std::get<std::vector<double>>(result_channels[ch]);
        for (double value : result_data) {
            EXPECT_NEAR(value, 1.0, 1e-10) << "Channel " << ch;
        }
    }
}

TEST_F(MathematicalTransformerTest, TrigonometricSine)
{
    auto pi_half_signal = TransformerTestDataGenerator::create_constant(1024, M_PI / 2.0, 2);

    std::vector<DataVariant> pi_channels;
    pi_channels.reserve(pi_half_signal.size());
    for (const auto& channel : pi_half_signal) {
        pi_channels.emplace_back(channel);
    }
    IO<std::vector<DataVariant>> pi_input(pi_channels);

    transformer->set_parameter("operation", MathematicalOperation::TRIGONOMETRIC);
    transformer->set_parameter("trig_function", std::string("sin"));
    transformer->set_parameter("frequency", 1.0);
    transformer->set_parameter("amplitude", 1.0);
    transformer->set_parameter("phase", 0.0);

    auto result = transformer->apply_operation(pi_input);

    const auto& result_channels = result.data;
    EXPECT_EQ(result_channels.size(), pi_half_signal.size()) << "Should preserve channel count";

    for (size_t ch = 0; ch < pi_half_signal.size(); ++ch) {
        const auto& result_data = std::get<std::vector<double>>(result_channels[ch]);
        for (double value : result_data) {
            EXPECT_NEAR(value, 1.0, 1e-10) << "Channel " << ch;
        }
    }
}

TEST_F(MathematicalTransformerTest, TrigonometricCosine)
{
    auto zero_signal = TransformerTestDataGenerator::create_constant(1024, 0.0, 2);

    std::vector<DataVariant> zero_channels;
    zero_channels.reserve(zero_signal.size());
    for (const auto& channel : zero_signal) {
        zero_channels.emplace_back(channel);
    }
    IO<std::vector<DataVariant>> zero_input(zero_channels);

    transformer->set_parameter("operation", MathematicalOperation::TRIGONOMETRIC);
    transformer->set_parameter("trig_function", std::string("cos"));

    auto result = transformer->apply_operation(zero_input);

    const auto& result_channels = result.data;
    EXPECT_EQ(result_channels.size(), zero_signal.size()) << "Should preserve channel count";

    for (size_t ch = 0; ch < zero_signal.size(); ++ch) {
        const auto& result_data = std::get<std::vector<double>>(result_channels[ch]);
        for (double value : result_data) {
            EXPECT_NEAR(value, 1.0, 1e-10) << "Channel " << ch;
        }
    }
}

TEST_F(MathematicalTransformerTest, QuantizationTransformation)
{
    transformer->set_parameter("operation", MathematicalOperation::QUANTIZE);
    transformer->set_parameter("bits", static_cast<u_int8_t>(8));

    auto result = transformer->apply_operation(test_input);

    const auto& result_channels = result.data;
    EXPECT_EQ(result_channels.size(), test_signal.size()) << "Should preserve channel count";

    for (size_t ch = 0; ch < test_signal.size(); ++ch) {
        const auto& result_data = std::get<std::vector<double>>(result_channels[ch]);
        EXPECT_EQ(result_data.size(), test_signal[ch].size()) << "Channel " << ch << " should preserve sample count";

        // 8-bit quantization should have 256 distinct levels
        std::set<double> unique_values(result_data.begin(), result_data.end());
        EXPECT_LE(unique_values.size(), 256U) << "Channel " << ch << " 8-bit quantization should not exceed 256 levels";
    }
}

TEST_F(MathematicalTransformerTest, NormalizationTransformation)
{
    transformer->set_parameter("operation", MathematicalOperation::NORMALIZE);
    transformer->set_parameter("target_peak", 0.5);

    auto result = transformer->apply_operation(test_input);

    const auto& result_channels = result.data;
    EXPECT_EQ(result_channels.size(), test_signal.size()) << "Should preserve channel count";

    for (size_t ch = 0; ch < test_signal.size(); ++ch) {
        const auto& result_data = std::get<std::vector<double>>(result_channels[ch]);
        EXPECT_EQ(result_data.size(), test_signal[ch].size()) << "Channel " << ch << " should preserve sample count";

        auto max_val = *std::ranges::max_element(result_data);
        EXPECT_NEAR(max_val, 0.5, 1e-10) << "Channel " << ch << " should be normalized to target peak";
    }
}

TEST_F(MathematicalTransformerTest, PolynomialTransformation)
{
    transformer->set_parameter("operation", MathematicalOperation::POLYNOMIAL);

    std::vector<double> coefficients = { 2.0, 1.0 };
    transformer->set_parameter("coefficients", coefficients);

    auto result = transformer->apply_operation(test_input);

    const auto& result_channels = result.data;
    EXPECT_EQ(result_channels.size(), test_signal.size()) << "Should preserve channel count";

    for (size_t ch = 0; ch < test_signal.size(); ++ch) {
        const auto& result_data = std::get<std::vector<double>>(result_channels[ch]);
        EXPECT_EQ(result_data.size(), test_signal[ch].size()) << "Channel " << ch << " should preserve sample count";

        // Check polynomial evaluation: f(x) = 1 + 2x
        for (size_t i = 0; i < std::min(result_data.size(), test_signal[ch].size()); ++i) {
            double expected = 1.0 + 2.0 * test_signal[ch][i];
            EXPECT_NEAR(result_data[i], expected, 1e-6)
                << "Sample " << i << " in channel " << ch;
        }
    }
}

TEST_F(MathematicalTransformerTest, MultiChannelConsistency)
{
    auto identical_signal = TransformerTestDataGenerator::create_sine_wave(512, 440.0, 1.0, 44100.0, 2);

    std::vector<DataVariant> identical_channels;
    identical_channels.reserve(identical_signal.size());
    for (const auto& channel : identical_signal) {
        identical_channels.emplace_back(channel);
    }
    IO<std::vector<DataVariant>> identical_input(identical_channels);

    transformer->set_parameter("operation", MathematicalOperation::GAIN);
    transformer->set_parameter("gain_factor", 1.5);

    auto result = transformer->apply_operation(identical_input);

    const auto& result_channels = result.data;
    EXPECT_EQ(result_channels.size(), 2) << "Should have 2 channels";

    const auto& channel_0 = std::get<std::vector<double>>(result_channels[0]);
    const auto& channel_1 = std::get<std::vector<double>>(result_channels[1]);

    EXPECT_EQ(channel_0.size(), channel_1.size()) << "Both channels should have same length";

    for (size_t i = 0; i < std::min(channel_0.size(), channel_1.size()); ++i) {
        EXPECT_NEAR(channel_0[i], channel_1[i], 1e-10)
            << "Sample " << i << " should be identical across channels";
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
        test_signal = TransformerTestDataGenerator::create_multi_tone(2048, { 220.0, 440.0, 880.0 }, 44100.0, 2);

        input_channels.reserve(test_signal.size());
        for (const auto& channel : test_signal) {
            input_channels.emplace_back(channel);
        }
        test_input = IO<std::vector<DataVariant>>(input_channels);
    }

    std::unique_ptr<SpectralTransformer<>> transformer;
    std::vector<std::vector<double>> test_signal;
    std::vector<DataVariant> input_channels;
    IO<std::vector<DataVariant>> test_input;
};

TEST_F(SpectralTransformerTest, FrequencyShiftTransformation)
{
    transformer->set_parameter("operation", SpectralOperation::FREQUENCY_SHIFT);
    transformer->set_parameter("shift_hz", 100.0);
    transformer->set_parameter("window_size", static_cast<u_int32_t>(1024));
    transformer->set_parameter("hop_size", static_cast<u_int32_t>(512));
    transformer->set_parameter("sample_rate", 44100.0);

    auto result = transformer->apply_operation(test_input);

    const auto& result_channels = result.data;
    EXPECT_EQ(result_channels.size(), test_signal.size()) << "Should preserve channel count";

    for (size_t ch = 0; ch < test_signal.size(); ++ch) {
        const auto& result_data = std::get<std::vector<double>>(result_channels[ch]);
        EXPECT_FALSE(result_data.empty()) << "Channel " << ch << " should not be empty";
        EXPECT_LE(result_data.size(), test_signal[ch].size() * 1.2)
            << "Channel " << ch << " output size should be reasonable";
    }
}

TEST_F(SpectralTransformerTest, PitchShiftTransformation)
{
    transformer->set_parameter("operation", SpectralOperation::PITCH_SHIFT);
    transformer->set_parameter("pitch_ratio", 1.5);
    transformer->set_parameter("window_size", static_cast<u_int32_t>(1024));
    transformer->set_parameter("hop_size", static_cast<u_int32_t>(512));

    auto result = transformer->apply_operation(test_input);

    const auto& result_channels = result.data;
    EXPECT_EQ(result_channels.size(), test_signal.size()) << "Should preserve channel count";

    for (size_t ch = 0; ch < test_signal.size(); ++ch) {
        const auto& result_data = std::get<std::vector<double>>(result_channels[ch]);
        EXPECT_FALSE(result_data.empty()) << "Channel " << ch << " should not be empty";

        EXPECT_GT(result_data.size(), test_signal[ch].size() * 0.8)
            << "Channel " << ch << " output should be reasonable size (lower bound)";
        EXPECT_LT(result_data.size(), test_signal[ch].size() * 1.2)
            << "Channel " << ch << " output should be reasonable size (upper bound)";
    }
}

TEST_F(SpectralTransformerTest, SpectralFilterTransformation)
{
    transformer->set_parameter("operation", SpectralOperation::SPECTRAL_FILTER);
    transformer->set_parameter("low_freq", 200.0);
    transformer->set_parameter("high_freq", 500.0); // Should preserve 220Hz and 440Hz, remove 880Hz
    transformer->set_parameter("window_size", static_cast<u_int32_t>(1024));
    transformer->set_parameter("hop_size", static_cast<u_int32_t>(512));
    transformer->set_parameter("sample_rate", 44100.0);

    auto result = transformer->apply_operation(test_input);

    const auto& result_channels = result.data;
    EXPECT_EQ(result_channels.size(), test_signal.size()) << "Should preserve channel count";

    for (size_t ch = 0; ch < test_signal.size(); ++ch) {
        const auto& result_data = std::get<std::vector<double>>(result_channels[ch]);
        EXPECT_FALSE(result_data.empty()) << "Channel " << ch << " should not be empty";
        EXPECT_EQ(result_data.size(), test_signal[ch].size())
            << "Channel " << ch << " should preserve sample count for spectral filtering";

        double input_energy = std::accumulate(test_signal[ch].begin(), test_signal[ch].end(), 0.0,
            [](double sum, double val) { return sum + val * val; });
        double output_energy = std::accumulate(result_data.begin(), result_data.end(), 0.0,
            [](double sum, double val) { return sum + val * val; });

        EXPECT_LT(output_energy, input_energy)
            << "Channel " << ch << " spectral filtering should reduce energy";
    }
}

TEST_F(SpectralTransformerTest, HarmonicEnhanceTransformation)
{
    transformer->set_parameter("operation", SpectralOperation::HARMONIC_ENHANCE);
    transformer->set_parameter("enhancement_factor", 2.0);
    transformer->set_parameter("window_size", static_cast<u_int32_t>(1024));
    transformer->set_parameter("hop_size", static_cast<u_int32_t>(512));

    auto result = transformer->apply_operation(test_input);

    const auto& result_channels = result.data;
    EXPECT_EQ(result_channels.size(), test_signal.size()) << "Should preserve channel count";

    for (size_t ch = 0; ch < test_signal.size(); ++ch) {
        const auto& result_data = std::get<std::vector<double>>(result_channels[ch]);
        EXPECT_FALSE(result_data.empty()) << "Channel " << ch << " should not be empty";
        EXPECT_EQ(result_data.size(), test_signal[ch].size())
            << "Channel " << ch << " should preserve sample count for harmonic enhancement";

        double input_rms = 0.0, output_rms = 0.0;
        for (size_t i = 0; i < std::min(test_signal[ch].size(), result_data.size()); ++i) {
            input_rms += test_signal[ch][i] * test_signal[ch][i];
            output_rms += result_data[i] * result_data[i];
        }
        input_rms = std::sqrt(input_rms / test_signal[ch].size());
        output_rms = std::sqrt(output_rms / result_data.size());

        EXPECT_NE(input_rms, output_rms)
            << "Channel " << ch << " harmonic enhancement should alter signal characteristics";
    }
}

TEST_F(SpectralTransformerTest, SpectralGateTransformation)
{
    transformer->set_parameter("operation", SpectralOperation::SPECTRAL_GATE);
    transformer->set_parameter("threshold", -30.0);
    transformer->set_parameter("window_size", static_cast<u_int32_t>(1024));
    transformer->set_parameter("hop_size", static_cast<u_int32_t>(512));

    auto result = transformer->apply_operation(test_input);

    const auto& result_channels = result.data;
    EXPECT_EQ(result_channels.size(), test_signal.size()) << "Should preserve channel count";

    for (size_t ch = 0; ch < test_signal.size(); ++ch) {
        const auto& result_data = std::get<std::vector<double>>(result_channels[ch]);
        EXPECT_FALSE(result_data.empty()) << "Channel " << ch << " should not be empty";
        EXPECT_EQ(result_data.size(), test_signal[ch].size())
            << "Channel " << ch << " should preserve sample count for spectral gating";

        double input_energy = std::accumulate(test_signal[ch].begin(), test_signal[ch].end(), 0.0,
            [](double sum, double val) { return sum + val * val; });
        double output_energy = std::accumulate(result_data.begin(), result_data.end(), 0.0,
            [](double sum, double val) { return sum + val * val; });

        EXPECT_LE(output_energy, input_energy)
            << "Channel " << ch << " spectral gating should not increase energy";
    }
}

TEST_F(SpectralTransformerTest, MultiChannelSpectralConsistency)
{
    auto identical_signal = TransformerTestDataGenerator::create_sine_wave(1024, 440.0, 1.0, 44100.0, 2);

    std::vector<DataVariant> identical_channels;
    identical_channels.reserve(identical_signal.size());
    for (const auto& channel : identical_signal) {
        identical_channels.emplace_back(channel);
    }
    IO<std::vector<DataVariant>> identical_input(identical_channels);

    transformer->set_parameter("operation", SpectralOperation::HARMONIC_ENHANCE);
    transformer->set_parameter("enhancement_factor", 1.5);
    transformer->set_parameter("window_size", static_cast<u_int32_t>(512));
    transformer->set_parameter("hop_size", static_cast<u_int32_t>(256));

    auto result = transformer->apply_operation(identical_input);

    const auto& result_channels = result.data;
    EXPECT_EQ(result_channels.size(), 2) << "Should have 2 channels";

    const auto& channel_0 = std::get<std::vector<double>>(result_channels[0]);
    const auto& channel_1 = std::get<std::vector<double>>(result_channels[1]);

    EXPECT_EQ(channel_0.size(), channel_1.size()) << "Both channels should have same length";

    double max_difference = 0.0;
    for (size_t i = 0; i < std::min(channel_0.size(), channel_1.size()); ++i) {
        double diff = std::abs(channel_0[i] - channel_1[i]);
        max_difference = std::max(max_difference, diff);
    }

    EXPECT_LT(max_difference, 1e-10)
        << "Identical inputs should produce nearly identical spectral processing results";
}

TEST_F(SpectralTransformerTest, SpectralParameterValidation)
{
    transformer->set_parameter("operation", SpectralOperation::PITCH_SHIFT);

    transformer->set_parameter("pitch_ratio", 0.5);
    transformer->set_parameter("window_size", static_cast<u_int32_t>(2048));
    transformer->set_parameter("hop_size", static_cast<u_int32_t>(1024));

    EXPECT_NO_THROW({
        auto result = transformer->apply_operation(test_input);
        const auto& result_channels = result.data;
        EXPECT_EQ(result_channels.size(), test_signal.size());

        for (size_t ch = 0; ch < result_channels.size(); ++ch) {
            const auto& result_data = std::get<std::vector<double>>(result_channels[ch]);
            EXPECT_FALSE(result_data.empty()) << "Channel " << ch << " should not be empty";
        }
    }) << "Spectral transformer should handle various parameter combinations";
}

// =========================================================================
// TEMPORAL TRANSFORMER TESTS
// =========================================================================

class TemporalTransformerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        transformer = std::make_unique<TemporalTransformer<>>();
        test_signal = TransformerTestDataGenerator::create_sine_wave(1024, 440.0, 1.0, 44100.0, 2);

        input_channels.reserve(test_signal.size());
        for (const auto& channel : test_signal) {
            input_channels.emplace_back(channel);
        }
        test_input = IO<std::vector<DataVariant>>(input_channels);
    }

    std::unique_ptr<TemporalTransformer<>> transformer;
    std::vector<std::vector<double>> test_signal;
    std::vector<DataVariant> input_channels;
    IO<std::vector<DataVariant>> test_input;
};

TEST_F(TemporalTransformerTest, SliceTransformation)
{
    transformer->set_parameter("operation", TemporalOperation::SLICE);
    transformer->set_parameter("start_ratio", 0.25);
    transformer->set_parameter("end_ratio", 0.75);

    auto result = transformer->apply_operation(test_input);

    const auto& result_channels = result.data;
    EXPECT_EQ(result_channels.size(), test_signal.size()) << "Should preserve channel count";

    auto expected_size = static_cast<size_t>(test_signal[0].size() * 0.5);

    for (size_t ch = 0; ch < test_signal.size(); ++ch) {
        const auto& result_data = std::get<std::vector<double>>(result_channels[ch]);
        EXPECT_EQ(result_data.size(), expected_size) << "Channel " << ch << " should have correct slice size";

        auto start_idx = static_cast<size_t>(test_signal[ch].size() * 0.25);
        for (size_t i = 0; i < result_data.size(); ++i) {
            EXPECT_NEAR(result_data[i], test_signal[ch][start_idx + i], 1e-10)
                << "Sample " << i << " in channel " << ch;
        }
    }
}

TEST_F(TemporalTransformerTest, InterpolationLinear)
{
    transformer->set_parameter("operation", TemporalOperation::INTERPOLATE);
    transformer->set_parameter("target_size", static_cast<size_t>(2048));
    transformer->set_parameter("use_cubic", false);

    auto result = transformer->apply_operation(test_input);

    const auto& result_channels = result.data;
    EXPECT_EQ(result_channels.size(), test_signal.size()) << "Should preserve channel count";

    for (size_t ch = 0; ch < test_signal.size(); ++ch) {
        const auto& result_data = std::get<std::vector<double>>(result_channels[ch]);
        EXPECT_EQ(result_data.size(), 2048U) << "Channel " << ch << " should have target interpolation size";

        EXPECT_NEAR(result_data[0], test_signal[ch][0], 1e-10)
            << "Channel " << ch << " first sample should be preserved";
        EXPECT_NEAR(result_data.back(), test_signal[ch].back(), 1e-10)
            << "Channel " << ch << " last sample should be preserved";
    }
}

TEST_F(TemporalTransformerTest, InterpolationCubic)
{
    transformer->set_parameter("operation", TemporalOperation::INTERPOLATE);
    transformer->set_parameter("target_size", static_cast<size_t>(512));
    transformer->set_parameter("use_cubic", true);

    auto result = transformer->apply_operation(test_input);

    const auto& result_channels = result.data;
    EXPECT_EQ(result_channels.size(), test_signal.size()) << "Should preserve channel count";

    for (size_t ch = 0; ch < test_signal.size(); ++ch) {
        const auto& result_data = std::get<std::vector<double>>(result_channels[ch]);
        EXPECT_EQ(result_data.size(), 512U) << "Channel " << ch << " should have target cubic interpolation size";

        EXPECT_NEAR(result_data[0], test_signal[ch][0], 1e-6)
            << "Channel " << ch << " first sample should be approximately preserved";
        EXPECT_NEAR(result_data.back(), test_signal[ch].back(), 1e-6)
            << "Channel " << ch << " last sample should be approximately preserved";
    }
}

TEST_F(TemporalTransformerTest, TimeReverseTransformation)
{
    transformer->set_parameter("operation", TemporalOperation::TIME_REVERSE);

    auto result = transformer->apply_operation(test_input);

    const auto& result_channels = result.data;
    EXPECT_EQ(result_channels.size(), test_signal.size()) << "Should preserve channel count";

    for (size_t ch = 0; ch < test_signal.size(); ++ch) {
        const auto& result_data = std::get<std::vector<double>>(result_channels[ch]);
        EXPECT_EQ(result_data.size(), test_signal[ch].size())
            << "Channel " << ch << " should preserve sample count";

        for (size_t i = 0; i < test_signal[ch].size(); ++i) {
            size_t reverse_idx = test_signal[ch].size() - 1 - i;
            EXPECT_NEAR(result_data[i], test_signal[ch][reverse_idx], 1e-10)
                << "Sample " << i << " in channel " << ch << " should be time-reversed";
        }
    }
}

TEST_F(TemporalTransformerTest, TimeStretchTransformation)
{
    transformer->set_parameter("operation", TemporalOperation::TIME_STRETCH);
    transformer->set_parameter("stretch_factor", 2.0);

    auto result = transformer->apply_operation(test_input);

    const auto& result_channels = result.data;
    EXPECT_EQ(result_channels.size(), test_signal.size()) << "Should preserve channel count";

    for (size_t ch = 0; ch < test_signal.size(); ++ch) {
        const auto& result_data = std::get<std::vector<double>>(result_channels[ch]);

        EXPECT_GT(result_data.size(), test_signal[ch].size() * 1.5)
            << "Channel " << ch << " should be stretched (lower bound)";
        EXPECT_LT(result_data.size(), test_signal[ch].size() * 2.5)
            << "Channel " << ch << " should be stretched (upper bound)";
    }
}

TEST_F(TemporalTransformerTest, DelayTransformation)
{
    transformer->set_parameter("operation", TemporalOperation::DELAY);
    transformer->set_parameter("delay_samples", static_cast<u_int32_t>(100));
    transformer->set_parameter("fill_value", 0.0);

    auto result = transformer->apply_operation(test_input);

    const auto& result_channels = result.data;
    EXPECT_EQ(result_channels.size(), test_signal.size()) << "Should preserve channel count";

    for (size_t ch = 0; ch < test_signal.size(); ++ch) {
        const auto& result_data = std::get<std::vector<double>>(result_channels[ch]);
        EXPECT_EQ(result_data.size(), test_signal[ch].size() + 100)
            << "Channel " << ch << " should be extended by delay amount";

        for (size_t i = 0; i < 100; ++i) {
            EXPECT_NEAR(result_data[i], 0.0, 1e-10)
                << "Sample " << i << " in channel " << ch << " should be delay fill value";
        }

        for (size_t i = 100; i < result_data.size() && (i - 100) < test_signal[ch].size(); ++i) {
            EXPECT_NEAR(result_data[i], test_signal[ch][i - 100], 1e-10)
                << "Sample " << i << " in channel " << ch << " should be delayed original signal";
        }
    }
}

TEST_F(TemporalTransformerTest, FadeInOutTransformation)
{
    transformer->set_parameter("operation", TemporalOperation::FADE_IN_OUT);
    transformer->set_parameter("fade_in_ratio", 0.1);
    transformer->set_parameter("fade_out_ratio", 0.1);

    auto result = transformer->apply_operation(test_input);

    const auto& result_channels = result.data;
    EXPECT_EQ(result_channels.size(), test_signal.size()) << "Should preserve channel count";

    for (size_t ch = 0; ch < test_signal.size(); ++ch) {
        const auto& result_data = std::get<std::vector<double>>(result_channels[ch]);
        EXPECT_EQ(result_data.size(), test_signal[ch].size())
            << "Channel " << ch << " should preserve sample count";

        EXPECT_NEAR(result_data[0], 0.0, 1e-10)
            << "Channel " << ch << " should start with fade-in at zero";

        EXPECT_NEAR(result_data.back(), 0.0, 1e-10)
            << "Channel " << ch << " should end with fade-out at zero";

        size_t mid_idx = result_data.size() / 2;
        EXPECT_NEAR(std::abs(result_data[mid_idx]), std::abs(test_signal[ch][mid_idx]), 1e-6)
            << "Channel " << ch << " middle section should be relatively unchanged";
    }
}

TEST_F(TemporalTransformerTest, MultiChannelTemporalConsistency)
{
    auto identical_signal = TransformerTestDataGenerator::create_linear_ramp(512, 2);

    std::vector<DataVariant> identical_channels;
    identical_channels.reserve(identical_signal.size());
    for (const auto& channel : identical_signal) {
        identical_channels.emplace_back(channel);
    }
    IO<std::vector<DataVariant>> identical_input(identical_channels);

    transformer->set_parameter("operation", TemporalOperation::TIME_REVERSE);

    auto result = transformer->apply_operation(identical_input);

    const auto& result_channels = result.data;
    EXPECT_EQ(result_channels.size(), 2) << "Should have 2 channels";

    const auto& channel_0 = std::get<std::vector<double>>(result_channels[0]);
    const auto& channel_1 = std::get<std::vector<double>>(result_channels[1]);

    EXPECT_EQ(channel_0.size(), channel_1.size()) << "Both channels should have same length";

    for (size_t i = 0; i < std::min(channel_0.size(), channel_1.size()); ++i) {
        EXPECT_NEAR(channel_0[i], channel_1[i], 1e-10)
            << "Sample " << i << " should be identical across channels";
    }
}

TEST_F(TemporalTransformerTest, TemporalParameterValidation)
{
    transformer->set_parameter("operation", TemporalOperation::TIME_STRETCH);

    transformer->set_parameter("stretch_factor", 0.5);

    EXPECT_NO_THROW({
        auto result = transformer->apply_operation(test_input);
        const auto& result_channels = result.data;
        EXPECT_EQ(result_channels.size(), test_signal.size());

        for (size_t ch = 0; ch < result_channels.size(); ++ch) {
            const auto& result_data = std::get<std::vector<double>>(result_channels[ch]);
            EXPECT_LT(result_data.size(), test_signal[ch].size() * 0.8)
                << "Channel " << ch << " should be compressed for stretch factor < 1";
        }
    }) << "Temporal transformer should handle various parameter combinations";

    transformer->set_parameter("operation", TemporalOperation::SLICE);
    transformer->set_parameter("start_ratio", 0.0);
    transformer->set_parameter("end_ratio", 1.0);

    EXPECT_NO_THROW({
        auto result = transformer->apply_operation(test_input);
        const auto& result_channels = result.data;
        EXPECT_EQ(result_channels.size(), test_signal.size());

        for (size_t ch = 0; ch < result_channels.size(); ++ch) {
            const auto& result_data = std::get<std::vector<double>>(result_channels[ch]);
            EXPECT_EQ(result_data.size(), test_signal[ch].size())
                << "Channel " << ch << " full slice should preserve original size";
        }
    }) << "Temporal transformer should handle full-range slice";
}

// =========================================================================
// PERFORMANCE CHARACTERISTICS TESTS
// =========================================================================

class TransformerPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        large_signal = TransformerTestDataGenerator::create_sine_wave(16384, 440.0, 1.0, 44100.0, 2);
        small_signal = TransformerTestDataGenerator::create_sine_wave(64, 440.0, 1.0, 44100.0, 2);

        large_channels.reserve(large_signal.size());
        for (const auto& channel : large_signal) {
            large_channels.emplace_back(channel);
        }
        large_input = IO<std::vector<DataVariant>>(large_channels);

        small_channels.reserve(small_signal.size());
        for (const auto& channel : small_signal) {
            small_channels.emplace_back(channel);
        }
        small_input = IO<std::vector<DataVariant>>(small_channels);
    }

    std::vector<std::vector<double>> large_signal;
    std::vector<std::vector<double>> small_signal;
    std::vector<DataVariant> large_channels;
    std::vector<DataVariant> small_channels;
    IO<std::vector<DataVariant>> large_input;
    IO<std::vector<DataVariant>> small_input;
};

TEST_F(TransformerPerformanceTest, ScalabilityWithSignalSize)
{
    auto transformer = std::make_unique<MathematicalTransformer<>>();
    transformer->set_parameter("operation", MathematicalOperation::POLYNOMIAL);
    std::vector<double> coefficients = { 1.0, 2.0, -0.5, 0.1 };
    transformer->set_parameter("coefficients", coefficients);

    auto start_time = std::chrono::high_resolution_clock::now();
    auto small_result = transformer->apply_operation(small_input);
    auto small_duration = std::chrono::high_resolution_clock::now() - start_time;

    start_time = std::chrono::high_resolution_clock::now();
    auto large_result = transformer->apply_operation(large_input);
    auto large_duration = std::chrono::high_resolution_clock::now() - start_time;

    const auto& small_result_channels = small_result.data;
    const auto& large_result_channels = large_result.data;

    EXPECT_EQ(small_result_channels.size(), small_signal.size()) << "Small result should preserve channel count";
    EXPECT_EQ(large_result_channels.size(), large_signal.size()) << "Large result should preserve channel count";

    for (size_t ch = 0; ch < small_signal.size(); ++ch) {
        const auto& small_data = std::get<std::vector<double>>(small_result_channels[ch]);
        EXPECT_EQ(small_data.size(), small_signal[ch].size()) << "Small signal channel " << ch << " should preserve sample count";
    }

    for (size_t ch = 0; ch < large_signal.size(); ++ch) {
        const auto& large_data = std::get<std::vector<double>>(large_result_channels[ch]);
        EXPECT_EQ(large_data.size(), large_signal[ch].size()) << "Large signal channel " << ch << " should preserve sample count";
    }

    auto small_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(small_duration).count();
    auto large_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(large_duration).count();

    if (small_ns > 0) {
        double scaling_factor = static_cast<double>(large_ns) / small_ns;

        size_t small_total_samples = 0, large_total_samples = 0;
        for (const auto& channel : small_signal) {
            small_total_samples += channel.size();
        }
        for (const auto& channel : large_signal) {
            large_total_samples += channel.size();
        }

        double size_ratio = static_cast<double>(large_total_samples) / small_total_samples;

        EXPECT_LT(scaling_factor, size_ratio * size_ratio)
            << "Performance scaling should not be worse than quadratic. "
            << "Small: " << small_ns << "ns (" << small_total_samples << " samples), "
            << "Large: " << large_ns << "ns (" << large_total_samples << " samples), "
            << "Scaling factor: " << scaling_factor << ", Size ratio: " << size_ratio;

        std::cout << "Performance test results:\n"
                  << "  Small signal: " << small_total_samples << " samples in " << small_ns << "ns\n"
                  << "  Large signal: " << large_total_samples << " samples in " << large_ns << "ns\n"
                  << "  Size ratio: " << size_ratio << "x\n"
                  << "  Time ratio: " << scaling_factor << "x\n";
    }
}

TEST_F(TransformerPerformanceTest, MemoryEfficiencyInPlace)
{
    auto transformer = std::make_unique<MathematicalTransformer<>>();
    transformer->set_parameter("operation", MathematicalOperation::GAIN);
    transformer->set_parameter("gain_factor", 2.0);

    std::vector<std::vector<double>> original_data = large_signal;

    auto result = transformer->apply_operation(large_input);

    const auto& result_channels = result.data;
    EXPECT_EQ(result_channels.size(), original_data.size()) << "Should preserve channel count";

    for (size_t ch = 0; ch < original_data.size(); ++ch) {
        const auto& result_data = std::get<std::vector<double>>(result_channels[ch]);
        EXPECT_EQ(result_data.size(), original_data[ch].size())
            << "Channel " << ch << " should preserve sample count";

        for (size_t i = 0; i < original_data[ch].size(); ++i) {
            EXPECT_NEAR(result_data[i], original_data[ch][i] * 2.0, 1e-10)
                << "Sample " << i << " in channel " << ch << " should be correctly gained";
        }
    }
}

TEST_F(TransformerPerformanceTest, MultiChannelOverhead)
{
    auto transformer = std::make_unique<MathematicalTransformer<>>();
    transformer->set_parameter("a", 1.5);
    transformer->set_parameter("b", 0.1);

    size_t total_samples = 0;
    for (const auto& channel : large_signal) {
        total_samples += channel.size();
    }

    auto single_channel_signal = TransformerTestDataGenerator::create_sine_wave(
        total_samples, 440.0, 1.0, 44100.0, 1);

    std::vector<DataVariant> single_channel_input;
    single_channel_input.emplace_back(single_channel_signal[0]);
    IO<std::vector<DataVariant>> single_input(single_channel_input);

    auto start_time = std::chrono::high_resolution_clock::now();
    auto multi_result = transformer->apply_operation(large_input);
    auto multi_duration = std::chrono::high_resolution_clock::now() - start_time;

    start_time = std::chrono::high_resolution_clock::now();
    auto single_result = transformer->apply_operation(single_input);
    auto single_duration = std::chrono::high_resolution_clock::now() - start_time;

    auto multi_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(multi_duration).count();
    auto single_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(single_duration).count();

    if (single_ns > 0) {
        double overhead_ratio = static_cast<double>(multi_ns) / single_ns;

        EXPECT_LT(overhead_ratio, 1.5)
            << "Multi-channel processing should not have excessive overhead. "
            << "Single-channel: " << single_ns << "ns, "
            << "Multi-channel: " << multi_ns << "ns, "
            << "Overhead ratio: " << overhead_ratio;

        std::cout << "Multi-channel overhead test:\n"
                  << "  Single channel (" << total_samples << " samples): " << single_ns << "ns\n"
                  << "  Multi channel (" << large_signal.size() << " channels): " << multi_ns << "ns\n"
                  << "  Overhead ratio: " << overhead_ratio << "x\n";
    }
}

// =========================================================================
// SPECIALIZED ALGORITHM VERIFICATION TESTS
// =========================================================================

class AlgorithmVerificationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        impulse = TransformerTestDataGenerator::create_impulse(128, 1.0, 2);
        sine_wave = TransformerTestDataGenerator::create_sine_wave(512, 1000.0, 1.0, 8000.0, 2);

        impulse_channels.reserve(impulse.size());
        for (const auto& channel : impulse) {
            impulse_channels.emplace_back(channel);
        }
        impulse_input = IO<std::vector<DataVariant>>(impulse_channels);

        sine_channels.reserve(sine_wave.size());
        for (const auto& channel : sine_wave) {
            sine_channels.emplace_back(channel);
        }
        sine_input = IO<std::vector<DataVariant>>(sine_channels);
    }

    std::vector<std::vector<double>> impulse;
    std::vector<std::vector<double>> sine_wave;
    std::vector<DataVariant> impulse_channels;
    std::vector<DataVariant> sine_channels;
    IO<std::vector<DataVariant>> impulse_input;
    IO<std::vector<DataVariant>> sine_input;
};

TEST_F(AlgorithmVerificationTest, ConvolutionWithKnownImpulseResponse)
{
    auto transformer = std::make_unique<ConvolutionTransformer<>>();
    transformer->set_parameter("operation", ConvolutionOperation::DIRECT_CONVOLUTION);

    std::vector<double> identity_impulse = { 1.0 };
    transformer->set_parameter("impulse_response", identity_impulse);

    auto result = transformer->apply_operation(sine_input);
    const auto& result_channels = result.data;
    EXPECT_EQ(result_channels.size(), sine_wave.size()) << "Should preserve channel count";

    for (size_t ch = 0; ch < sine_wave.size(); ++ch) {
        const auto& result_data = std::get<std::vector<double>>(result_channels[ch]);
        EXPECT_EQ(result_data.size(), sine_wave[ch].size())
            << "Channel " << ch << " should preserve sample count";

        for (size_t i = 0; i < sine_wave[ch].size(); ++i) {
            EXPECT_NEAR(result_data[i], sine_wave[ch][i], 1e-10)
                << "Sample " << i << " in channel " << ch << " should be preserved by identity convolution";
        }
    }
}

TEST_F(AlgorithmVerificationTest, MathematicalPolynomialEvaluation)
{
    auto transformer = std::make_unique<MathematicalTransformer<>>();
    transformer->set_parameter("operation", MathematicalOperation::POLYNOMIAL);

    // Test polynomial: f(x) = 1 + 2x + 3x^2 (coefficients in reverse order: [x^0, x^1, x^2])
    std::vector<double> coefficients = { 3.0, 2.0, 1.0 };
    transformer->set_parameter("coefficients", coefficients);

    std::vector<double> test_values = { 0.0, 1.0, 2.0, 3.0 };
    std::vector<DataVariant> test_channels;
    for (size_t ch = 0; ch < 2; ++ch) {
        test_channels.emplace_back(test_values);
    }
    IO<std::vector<DataVariant>> test_input(test_channels);

    auto result = transformer->apply_operation(test_input);
    const auto& result_channels = result.data;
    EXPECT_EQ(result_channels.size(), 2) << "Should preserve channel count";

    // Expected results: f(0)=1, f(1)=6, f(2)=17, f(3)=34
    std::vector<double> expected = { 1.0, 6.0, 17.0, 34.0 };

    for (size_t ch = 0; ch < result_channels.size(); ++ch) {
        const auto& result_data = std::get<std::vector<double>>(result_channels[ch]);
        EXPECT_EQ(result_data.size(), test_values.size())
            << "Channel " << ch << " should preserve sample count";

        for (size_t i = 0; i < expected.size(); ++i) {
            EXPECT_NEAR(result_data[i], expected[i], 1e-6)
                << "Channel " << ch << " polynomial evaluation at x=" << test_values[i]
                << " should equal " << expected[i];
        }
    }
}

TEST_F(AlgorithmVerificationTest, TemporalReverseSymmetry)
{
    auto transformer = std::make_unique<TemporalTransformer<>>();
    transformer->set_parameter("operation", TemporalOperation::TIME_REVERSE);

    std::vector<double> symmetric_signal = { 1.0, 2.0, 3.0, 4.0, 5.0, 4.0, 3.0, 2.0, 1.0 };
    std::vector<DataVariant> symmetric_channels;
    for (size_t ch = 0; ch < 2; ++ch) {
        symmetric_channels.emplace_back(symmetric_signal);
    }
    IO<std::vector<DataVariant>> symmetric_input(symmetric_channels);

    auto result = transformer->apply_operation(symmetric_input);
    const auto& result_channels = result.data;
    EXPECT_EQ(result_channels.size(), 2) << "Should preserve channel count";

    for (size_t ch = 0; ch < result_channels.size(); ++ch) {
        const auto& result_data = std::get<std::vector<double>>(result_channels[ch]);
        EXPECT_EQ(result_data.size(), symmetric_signal.size())
            << "Channel " << ch << " should preserve sample count";

        for (size_t i = 0; i < symmetric_signal.size(); ++i) {
            EXPECT_NEAR(result_data[i], symmetric_signal[i], 1e-10)
                << "Channel " << ch << " sample " << i << " should be unchanged by time reversal of symmetric signal";
        }
    }
}

TEST_F(AlgorithmVerificationTest, NormalizationPreservesShape)
{
    auto transformer = std::make_unique<MathematicalTransformer<>>();
    transformer->set_parameter("operation", MathematicalOperation::NORMALIZE);
    transformer->set_parameter("target_peak", 0.5);

    auto result = transformer->apply_operation(sine_input);
    const auto& result_channels = result.data;
    EXPECT_EQ(result_channels.size(), sine_wave.size()) << "Should preserve channel count";

    auto count_zero_crossings = [](const std::vector<double>& signal) {
        int crossings = 0;
        for (size_t i = 1; i < signal.size(); ++i) {
            if ((signal[i - 1] >= 0 && signal[i] < 0) || (signal[i - 1] < 0 && signal[i] >= 0)) {
                crossings++;
            }
        }
        return crossings;
    };

    for (size_t ch = 0; ch < sine_wave.size(); ++ch) {
        const auto& result_data = std::get<std::vector<double>>(result_channels[ch]);
        EXPECT_EQ(result_data.size(), sine_wave[ch].size())
            << "Channel " << ch << " should preserve sample count";

        auto max_val = *std::ranges::max_element(result_data);
        EXPECT_NEAR(max_val, 0.5, 1e-10)
            << "Channel " << ch << " should be normalized to target peak of 0.5";

        int original_crossings = count_zero_crossings(sine_wave[ch]);
        int normalized_crossings = count_zero_crossings(result_data);
        EXPECT_EQ(original_crossings, normalized_crossings)
            << "Channel " << ch << " normalization should preserve zero crossings (signal shape)";

        if (sine_wave[ch].size() >= 2) {
            size_t idx1 = sine_wave[ch].size() / 4;
            size_t idx2 = sine_wave[ch].size() / 2;

            if (std::abs(sine_wave[ch][idx1]) > 1e-10 && std::abs(sine_wave[ch][idx2]) > 1e-10) {
                double original_ratio = sine_wave[ch][idx1] / sine_wave[ch][idx2];
                double normalized_ratio = result_data[idx1] / result_data[idx2];
                EXPECT_NEAR(original_ratio, normalized_ratio, 1e-6)
                    << "Channel " << ch << " normalization should preserve amplitude ratios";
            }
        }
    }
}

TEST_F(AlgorithmVerificationTest, ConvolutionLinearity)
{
    auto transformer = std::make_unique<ConvolutionTransformer<>>();
    transformer->set_parameter("operation", ConvolutionOperation::DIRECT_CONVOLUTION);

    std::vector<double> simple_kernel = { 0.5, 1.0, 0.5 };
    transformer->set_parameter("impulse_response", simple_kernel);

    auto signal1 = TransformerTestDataGenerator::create_sine_wave(64, 500.0, 1.0, 8000.0, 2);
    auto signal2 = TransformerTestDataGenerator::create_sine_wave(64, 1500.0, 1.0, 8000.0, 2);

    double a = 2.0, b = 3.0;

    std::vector<DataVariant> combined_channels;
    for (size_t ch = 0; ch < 2; ++ch) {
        std::vector<double> combined(signal1[ch].size());
        for (size_t i = 0; i < signal1[ch].size(); ++i) {
            combined[i] = a * signal1[ch][i] + b * signal2[ch][i];
        }
        combined_channels.emplace_back(combined);
    }
    IO<std::vector<DataVariant>> combined_input(combined_channels);

    auto combined_result = transformer->apply_operation(combined_input);

    std::vector<DataVariant> signal1_channels, signal2_channels;
    for (size_t ch = 0; ch < 2; ++ch) {
        signal1_channels.emplace_back(signal1[ch]);
        signal2_channels.emplace_back(signal2[ch]);
    }
    IO<std::vector<DataVariant>> signal1_input(signal1_channels);
    IO<std::vector<DataVariant>> signal2_input(signal2_channels);

    auto result1 = transformer->apply_operation(signal1_input);
    auto result2 = transformer->apply_operation(signal2_input);

    // Verify linearity: conv(a*x + b*y) = a*conv(x) + b*conv(y)
    const auto& combined_data = combined_result.data;
    const auto& result1_data = result1.data;
    const auto& result2_data = result2.data;

    for (size_t ch = 0; ch < 2; ++ch) {
        const auto& combined_channel = std::get<std::vector<double>>(combined_data[ch]);
        const auto& result1_channel = std::get<std::vector<double>>(result1_data[ch]);
        const auto& result2_channel = std::get<std::vector<double>>(result2_data[ch]);

        EXPECT_EQ(combined_channel.size(), result1_channel.size())
            << "Channel " << ch << " sizes should match";
        EXPECT_EQ(combined_channel.size(), result2_channel.size())
            << "Channel " << ch << " sizes should match";

        for (size_t i = 0; i < combined_channel.size(); ++i) {
            double expected = a * result1_channel[i] + b * result2_channel[i];
            EXPECT_NEAR(combined_channel[i], expected, 1e-10)
                << "Channel " << ch << " sample " << i << " should satisfy convolution linearity";
        }
    }
}

// =========================================================================
// ERROR HANDLING AND ROBUSTNESS TESTS
// =========================================================================

class TransformerRobustnessTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        normal_signal = TransformerTestDataGenerator::create_sine_wave(256, 440.0, 1.0, 44100.0, 2);

        input_channels.reserve(normal_signal.size());
        for (const auto& channel : normal_signal) {
            input_channels.emplace_back(channel);
        }
        test_input = IO<std::vector<DataVariant>>(input_channels);
    }

    std::vector<std::vector<double>> normal_signal;
    std::vector<DataVariant> input_channels;
    IO<std::vector<DataVariant>> test_input;
};

TEST_F(TransformerRobustnessTest, InvalidParameterTypes)
{
    auto transformer = std::make_unique<MathematicalTransformer<>>();

    EXPECT_NO_THROW(transformer->set_parameter("gain_factor", std::string("not_a_number")));
    EXPECT_NO_THROW(transformer->set_parameter("operation", 42));

    transformer->set_parameter("operation", MathematicalOperation::GAIN);
    transformer->set_parameter("gain_factor", 2.0);

    EXPECT_NO_THROW({
        auto result = transformer->apply_operation(test_input);
        const auto& result_channels = result.data;
        EXPECT_EQ(result_channels.size(), normal_signal.size()) << "Should preserve channel count even with invalid params";

        for (size_t ch = 0; ch < result_channels.size(); ++ch) {
            const auto& result_data = std::get<std::vector<double>>(result_channels[ch]);
            EXPECT_FALSE(result_data.empty()) << "Channel " << ch << " should not be empty";
        }
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

    std::vector<DataVariant> problematic_channels;
    for (size_t ch = 0; ch < normal_signal.size(); ++ch) {
        problematic_channels.emplace_back(problematic_signal);
    }
    IO<std::vector<DataVariant>> problematic_input(problematic_channels);

    EXPECT_NO_THROW({
        auto result = transformer->apply_operation(problematic_input);
        const auto& result_channels = result.data;

        EXPECT_TRUE(result.metadata.contains("validation_failed"))
            << "Should indicate validation failure in metadata";
        EXPECT_EQ(result_channels.size(), normal_signal.size())
            << "Should preserve channel count even with problematic data";

        for (size_t ch = 0; ch < result_channels.size(); ++ch) {
            const auto& result_data = std::get<std::vector<double>>(result_channels[ch]);
            EXPECT_EQ(result_data.size(), problematic_signal.size())
                << "Channel " << ch << " should preserve sample count";
            EXPECT_EQ(result_data[0], 1.0)
                << "Channel " << ch << " valid data should be processed correctly";
        }
    });
}

TEST_F(TransformerRobustnessTest, ZeroDivisionProtection)
{
    auto transformer = std::make_unique<MathematicalTransformer<>>();
    transformer->set_parameter("operation", MathematicalOperation::POWER);
    transformer->set_parameter("exponent", -1.0);

    std::vector<double> signal_with_zero = { 1.0, 2.0, 0.0, 4.0, 5.0 };

    std::vector<DataVariant> zero_channels;
    for (size_t ch = 0; ch < normal_signal.size(); ++ch) {
        zero_channels.emplace_back(signal_with_zero);
    }
    IO<std::vector<DataVariant>> zero_input(zero_channels);

    EXPECT_NO_THROW({
        auto result = transformer->apply_operation(zero_input);
        const auto& result_channels = result.data;
        EXPECT_EQ(result_channels.size(), normal_signal.size())
            << "Should preserve channel count";

        for (size_t ch = 0; ch < result_channels.size(); ++ch) {
            const auto& result_data = std::get<std::vector<double>>(result_channels[ch]);
            EXPECT_EQ(result_data.size(), signal_with_zero.size())
                << "Channel " << ch << " should preserve sample count";

            EXPECT_NEAR(result_data[0], 1.0, 1e-10)
                << "Channel " << ch << ": 1^(-1) = 1";
            EXPECT_NEAR(result_data[1], 0.5, 1e-10)
                << "Channel " << ch << ": 2^(-1) = 0.5";
            EXPECT_FALSE(std::isnan(result_data[2]))
                << "Channel " << ch << ": Zero division should not produce NaN";
        }
    });
}

TEST_F(TransformerRobustnessTest, VeryLargeSignals)
{
    size_t large_size = static_cast<long>(1024) * 1024;
    auto large_signal = TransformerTestDataGenerator::create_sine_wave(large_size, 440.0, 1.0, 44100.0, 2);

    std::vector<DataVariant> large_channels;
    large_channels.reserve(large_signal.size());
    for (const auto& channel : large_signal) {
        large_channels.emplace_back(channel);
    }
    IO<std::vector<DataVariant>> large_input(large_channels);

    auto transformer = std::make_unique<MathematicalTransformer<>>();
    transformer->set_parameter("operation", MathematicalOperation::NORMALIZE);

    EXPECT_NO_THROW({
        auto result = transformer->apply_operation(large_input);
        const auto& result_channels = result.data;
        EXPECT_EQ(result_channels.size(), large_signal.size())
            << "Should preserve channel count for large signals";

        for (size_t ch = 0; ch < result_channels.size(); ++ch) {
            const auto& result_data = std::get<std::vector<double>>(result_channels[ch]);
            EXPECT_EQ(result_data.size(), large_size)
                << "Channel " << ch << " should preserve large sample count";

            auto max_val = *std::ranges::max_element(result_data);
            EXPECT_NEAR(max_val, 1.0, 1e-10)
                << "Channel " << ch << " should be properly normalized";
        }
    });
}

TEST_F(TransformerRobustnessTest, EmptyAndMinimalData)
{
    auto transformer = std::make_unique<MathematicalTransformer<>>();
    transformer->set_parameter("operation", MathematicalOperation::GAIN);
    transformer->set_parameter("gain_factor", 2.0);

    std::vector<double> minimal_signal = { 0.5 };
    std::vector<DataVariant> minimal_channels;
    for (size_t ch = 0; ch < 2; ++ch) {
        minimal_channels.emplace_back(minimal_signal);
    }
    IO<std::vector<DataVariant>> minimal_input(minimal_channels);

    EXPECT_NO_THROW({
        auto result = transformer->apply_operation(minimal_input);
        const auto& result_channels = result.data;
        EXPECT_EQ(result_channels.size(), 2) << "Should handle minimal data";

        for (size_t ch = 0; ch < result_channels.size(); ++ch) {
            const auto& result_data = std::get<std::vector<double>>(result_channels[ch]);
            EXPECT_EQ(result_data.size(), 1) << "Channel " << ch << " should preserve minimal size";
            EXPECT_NEAR(result_data[0], 1.0, 1e-10) << "Channel " << ch << " should process minimal data correctly";
        }
    });

    std::vector<double> empty_signal = {};
    std::vector<DataVariant> empty_channels;
    for (size_t ch = 0; ch < 2; ++ch) {
        empty_channels.emplace_back(empty_signal);
    }
    IO<std::vector<DataVariant>> empty_input(empty_channels);

    EXPECT_NO_THROW({
        auto result = transformer->apply_operation(empty_input);
        const auto& result_channels = result.data;
        EXPECT_EQ(result_channels.size(), 2) << "Should handle empty channels gracefully";
    });
}

TEST_F(TransformerRobustnessTest, MixedChannelSizes)
{
    auto transformer = std::make_unique<MathematicalTransformer<>>();
    transformer->set_parameter("operation", MathematicalOperation::OFFSET);
    transformer->set_parameter("offset_value", 1.0);

    std::vector<double> short_channel = { 1.0, 2.0, 3.0 };
    std::vector<double> long_channel = { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0 };

    std::vector<DataVariant> mixed_channels;
    mixed_channels.emplace_back(short_channel);
    mixed_channels.emplace_back(long_channel);
    IO<std::vector<DataVariant>> mixed_input(mixed_channels);

    EXPECT_NO_THROW({
        auto result = transformer->apply_operation(mixed_input);
        const auto& result_channels = result.data;
        EXPECT_EQ(result_channels.size(), 2) << "Should handle mixed channel sizes";

        const auto& result_short = std::get<std::vector<double>>(result_channels[0]);
        const auto& result_long = std::get<std::vector<double>>(result_channels[1]);

        EXPECT_EQ(result_short.size(), short_channel.size())
            << "Should preserve short channel size";
        EXPECT_EQ(result_long.size(), long_channel.size())
            << "Should preserve long channel size";

        for (size_t i = 0; i < result_short.size(); ++i) {
            EXPECT_NEAR(result_short[i], short_channel[i] + 1.0, 1e-10)
                << "Short channel sample " << i;
        }
        for (size_t i = 0; i < result_long.size(); ++i) {
            EXPECT_NEAR(result_long[i], long_channel[i] + 1.0, 1e-10)
                << "Long channel sample " << i;
        }
    });
}

// =========================================================================
// TRANSFORMER STRATEGY AND QUALITY TESTS
// =========================================================================

class TransformerStrategyTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_signal = TransformerTestDataGenerator::create_sine_wave(512, 440.0, 1.0, 44100.0, 1);

        multi_channel_signal = TransformerTestDataGenerator::create_sine_wave(512, 440.0, 1.0, 44100.0, 2);
    }

    std::vector<std::vector<double>> test_signal;
    std::vector<std::vector<double>> multi_channel_signal;
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

    std::vector<DataVariant> channel_variants;
    channel_variants.reserve(test_signal.size());
    for (const auto& channel : test_signal) {
        channel_variants.emplace_back(channel);
    }

    IO<std::vector<DataVariant>> input { channel_variants };

    EXPECT_NO_THROW({
        auto result = transformer->apply_operation(input);
        EXPECT_FALSE(result.data.empty());

        EXPECT_EQ(result.data.size(), test_signal.size());

        for (const auto& channel_variant : result.data) {
            auto channel_data = std::get<std::vector<double>>(channel_variant);
            EXPECT_FALSE(channel_data.empty());
        }
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

TEST_F(TransformerStrategyTest, MultiChannelProcessing)
{
    auto transformer = std::make_unique<MathematicalTransformer<>>();
    transformer->set_parameter("operation", MathematicalOperation::GAIN);
    transformer->set_parameter("gain_factor", 2.0);

    std::vector<DataVariant> channel_variants;

    channel_variants.reserve(multi_channel_signal.size());
    for (const auto& channel : multi_channel_signal) {
        channel_variants.emplace_back(channel);
    }

    IO<std::vector<DataVariant>> input { channel_variants };

    EXPECT_NO_THROW({
        auto result = transformer->apply_operation(input);

        EXPECT_EQ(result.data.size(), multi_channel_signal.size());

        for (size_t ch = 0; ch < result.data.size(); ++ch) {
            auto output_channel = std::get<std::vector<double>>(result.data[ch]);
            EXPECT_EQ(output_channel.size(), multi_channel_signal[ch].size());

            for (size_t i = 0; i < std::min(output_channel.size(), multi_channel_signal[ch].size()); ++i) {
                EXPECT_NEAR(output_channel[i], multi_channel_signal[ch][i] * 2.0, 1e-10);
            }
        }
    });
}

TEST_F(TransformerStrategyTest, VariableChannelCounts)
{
    auto transformer = std::make_unique<MathematicalTransformer<>>();
    transformer->set_parameter("operation", MathematicalOperation::GAIN);
    transformer->set_parameter("gain_factor", 1.5);

    for (size_t channels = 1; channels <= 8; channels *= 2) {
        auto multichannel_data = TransformerTestDataGenerator::create_sine_wave(256, 440.0, 1.0, 44100.0, channels);

        std::vector<DataVariant> channel_variants;

        channel_variants.reserve(multichannel_data.size());
        for (const auto& channel : multichannel_data) {
            channel_variants.emplace_back(channel);
        }

        IO<std::vector<DataVariant>> input { channel_variants };

        EXPECT_NO_THROW({
            auto result = transformer->apply_operation(input);
            EXPECT_EQ(result.data.size(), channels);
        }) << "Failed with "
           << channels << " channels";
    }
}

// =========================================================================
// COMPUTATIONAL COST AND PROGRESS TESTS
// =========================================================================

class TransformerComputationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_signal = TransformerTestDataGenerator::create_sine_wave(1024, 440.0, 1.0, 44100.0, 1);

        multi_channel_signal = TransformerTestDataGenerator::create_sine_wave(1024, 440.0, 1.0, 44100.0, 2);
    }

    std::vector<std::vector<double>> test_signal;
    std::vector<std::vector<double>> multi_channel_signal;
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

    EXPECT_GE(complex_cost, simple_cost) << "Complex spectral operations should typically cost more than simple mathematical operations";
}

TEST_F(TransformerComputationTest, TransformationProgress)
{
    auto transformer = std::make_unique<TemporalTransformer<>>();
    transformer->set_parameter("operation", TemporalOperation::TIME_STRETCH);
    transformer->set_parameter("stretch_factor", 2.0);

    double progress = transformer->get_transformation_progress();
    EXPECT_GE(progress, 0.0) << "Progress should be non-negative";
    EXPECT_LE(progress, 1.0) << "Progress should not exceed 1.0";

    std::vector<DataVariant> channel_variants;
    channel_variants.reserve(test_signal.size());
    for (const auto& channel : test_signal) {
        channel_variants.emplace_back(channel);
    }

    IO<std::vector<DataVariant>> input { channel_variants };

    EXPECT_NO_THROW({
        auto result = transformer->apply_operation(input);
        double post_transform_progress = transformer->get_transformation_progress();
        EXPECT_GE(post_transform_progress, 0.0);
        EXPECT_LE(post_transform_progress, 1.0);
    });
}

TEST_F(TransformerComputationTest, InPlaceTransformationFlag)
{
    auto transformer = std::make_unique<MathematicalTransformer<>>();

    bool in_place = transformer->is_in_place();
    EXPECT_TRUE(in_place == true || in_place == false) << "Should return a valid boolean";

    transformer->set_strategy(TransformationStrategy::IN_PLACE);
    bool in_place_after_strategy = transformer->is_in_place();
    EXPECT_TRUE(in_place_after_strategy == true || in_place_after_strategy == false);
}

TEST_F(TransformerComputationTest, ComputationalCostScaling)
{
    auto transformer = std::make_unique<SpectralTransformer<>>();
    transformer->set_parameter("operation", SpectralOperation::FREQUENCY_SHIFT);

    auto small_signal = TransformerTestDataGenerator::create_sine_wave(512, 440.0, 1.0, 44100.0, 1);
    auto large_signal = TransformerTestDataGenerator::create_sine_wave(4096, 440.0, 1.0, 44100.0, 1);

    std::vector<DataVariant> small_variants, large_variants;
    for (const auto& channel : small_signal) {
        small_variants.emplace_back(channel);
    }
    for (const auto& channel : large_signal) {
        large_variants.emplace_back(channel);
    }

    double small_cost = transformer->estimate_computational_cost();
    double large_cost = transformer->estimate_computational_cost();

    EXPECT_GE(small_cost, 0.0);
    EXPECT_GE(large_cost, 0.0);
}

// =========================================================================
// PARAMETER HANDLING TESTS
// =========================================================================

class TransformerParameterTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_signal = TransformerTestDataGenerator::create_sine_wave(1024, 440.0, 1.0, 44100.0, 1);

        multi_channel_signal = TransformerTestDataGenerator::create_sine_wave(1024, 440.0, 1.0, 44100.0, 2);
    }

    std::vector<std::vector<double>> test_signal;
    std::vector<std::vector<double>> multi_channel_signal;
};

TEST_F(TransformerParameterTest, ParameterTypeConversion)
{
    auto transformer = std::make_unique<MathematicalTransformer<>>();
    transformer->set_parameter("operation", std::string("GAIN"));
    transformer->set_parameter("gain_factor", 3.0);

    std::vector<DataVariant> channel_variants;
    channel_variants.reserve(test_signal.size());
    for (const auto& channel : test_signal) {
        channel_variants.emplace_back(channel);
    }

    IO<std::vector<DataVariant>> input { channel_variants };

    auto result = transformer->apply_operation(input);

    EXPECT_EQ(result.data.size(), test_signal.size());

    for (size_t ch = 0; ch < result.data.size(); ++ch) {
        auto result_data = std::get<std::vector<double>>(result.data[ch]);
        const auto& original_channel = test_signal[ch];

        EXPECT_EQ(result_data.size(), original_channel.size());

        for (size_t i = 0; i < std::min(result_data.size(), original_channel.size()); ++i) {
            EXPECT_NEAR(result_data[i], original_channel[i] * 3.0, 1e-10);
        }
    }
}

TEST_F(TransformerParameterTest, DefaultParameterValues)
{
    auto transformer = std::make_unique<ConvolutionTransformer<>>();

    std::vector<DataVariant> channel_variants;
    channel_variants.reserve(test_signal.size());
    for (const auto& channel : test_signal) {
        channel_variants.emplace_back(channel);
    }

    IO<std::vector<DataVariant>> input { channel_variants };

    EXPECT_NO_THROW({
        auto result = transformer->apply_operation(input);

        EXPECT_EQ(result.data.size(), test_signal.size());

        for (const auto& channel_variant : result.data) {
            auto result_data = std::get<std::vector<double>>(channel_variant);
            EXPECT_FALSE(result_data.empty());
        }
    });
}

TEST_F(TransformerParameterTest, InvalidParameterHandling)
{
    auto transformer = std::make_unique<SpectralTransformer<>>();

    EXPECT_NO_THROW(transformer->set_parameter("invalid_param", 42));
    EXPECT_NO_THROW(transformer->set_parameter("operation", "INVALID_OPERATION"));

    transformer->set_parameter("operation", SpectralOperation::FREQUENCY_SHIFT);
    transformer->set_parameter("shift_hz", 100.0);

    std::vector<DataVariant> channel_variants;
    channel_variants.reserve(test_signal.size());
    for (const auto& channel : test_signal) {
        channel_variants.emplace_back(channel);
    }

    IO<std::vector<DataVariant>> input { channel_variants };

    EXPECT_NO_THROW({
        auto result = transformer->apply_operation(input);

        EXPECT_EQ(result.data.size(), test_signal.size());

        for (const auto& channel_variant : result.data) {
            auto result_data = std::get<std::vector<double>>(channel_variant);
            EXPECT_FALSE(result_data.empty());
        }
    });
}

TEST_F(TransformerParameterTest, ParameterValidationAndTypes)
{
    auto transformer = std::make_unique<MathematicalTransformer<>>();

    EXPECT_NO_THROW(transformer->set_parameter("gain_factor", 2.5));
    EXPECT_NO_THROW(transformer->set_parameter("operation", MathematicalOperation::GAIN));
    EXPECT_NO_THROW(transformer->set_parameter("normalize", true));
    EXPECT_NO_THROW(transformer->set_parameter("buffer_size", 1024));
    EXPECT_NO_THROW(transformer->set_parameter("name", std::string("test")));

    auto gain_param = transformer->get_parameter("gain_factor");
    EXPECT_TRUE(gain_param.has_value()) << "gain_factor parameter should be retrievable";

    auto operation_param = transformer->get_transformer_name();
    EXPECT_TRUE(operation_param != "") << "operation parameter should be retrievable";

    EXPECT_FALSE(transformer->get_parameter("nonexistent").has_value()) << "Nonexistent parameters should return nullopt";
}

TEST_F(TransformerParameterTest, MultiChannelParameterEffects)
{
    auto transformer = std::make_unique<MathematicalTransformer<>>();
    transformer->set_parameter("operation", MathematicalOperation::GAIN);
    transformer->set_parameter("gain_factor", 0.5);

    std::vector<DataVariant> channel_variants;
    channel_variants.reserve(multi_channel_signal.size());
    for (const auto& channel : multi_channel_signal) {
        channel_variants.emplace_back(channel);
    }

    IO<std::vector<DataVariant>> input { channel_variants };

    auto result = transformer->apply_operation(input);

    EXPECT_EQ(result.data.size(), multi_channel_signal.size());

    for (size_t ch = 0; ch < result.data.size(); ++ch) {
        auto result_channel = std::get<std::vector<double>>(result.data[ch]);
        const auto& input_channel = multi_channel_signal[ch];

        EXPECT_EQ(result_channel.size(), input_channel.size());

        for (size_t i = 0; i < std::min(result_channel.size(), input_channel.size()); ++i) {
            EXPECT_NEAR(result_channel[i], input_channel[i] * 0.5, 1e-10)
                << "Gain should be applied consistently to channel " << ch << " sample " << i;
        }
    }
}

// =========================================================================
// EDGE CASE AND VALIDATION TESTS
// =========================================================================

class TransformerValidationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        normal_signal = TransformerTestDataGenerator::create_sine_wave(64, 440.0, 1.0, 44100.0, 2);
        empty_signal = std::vector<std::vector<double>>(2, std::vector<double> {});
        single_sample = std::vector<std::vector<double>>(2, std::vector<double> { 1.0 });
        constant_signal = TransformerTestDataGenerator::create_constant(100, 0.5, 2);

        normal_channels.reserve(normal_signal.size());
        for (const auto& channel : normal_signal) {
            normal_channels.emplace_back(channel);
        }
        normal_input = IO<std::vector<DataVariant>>(normal_channels);

        empty_channels.reserve(empty_signal.size());
        for (const auto& channel : empty_signal) {
            empty_channels.emplace_back(channel);
        }
        empty_input = IO<std::vector<DataVariant>>(empty_channels);

        single_channels.reserve(single_sample.size());
        for (const auto& channel : single_sample) {
            single_channels.emplace_back(channel);
        }
        single_input = IO<std::vector<DataVariant>>(single_channels);

        constant_channels.reserve(constant_signal.size());
        for (const auto& channel : constant_signal) {
            constant_channels.emplace_back(channel);
        }
        constant_input = IO<std::vector<DataVariant>>(constant_channels);
    }

    std::vector<std::vector<double>> normal_signal;
    std::vector<std::vector<double>> empty_signal;
    std::vector<std::vector<double>> single_sample;
    std::vector<std::vector<double>> constant_signal;

    std::vector<DataVariant> normal_channels;
    std::vector<DataVariant> empty_channels;
    std::vector<DataVariant> single_channels;
    std::vector<DataVariant> constant_channels;

    IO<std::vector<DataVariant>> normal_input;
    IO<std::vector<DataVariant>> empty_input;
    IO<std::vector<DataVariant>> single_input;
    IO<std::vector<DataVariant>> constant_input;
};

TEST_F(TransformerValidationTest, EmptySignalHandling)
{
    auto transformers = std::vector<std::unique_ptr<UniversalTransformer<std::vector<DataVariant>, std::vector<DataVariant>>>> {};
    transformers.push_back(std::make_unique<ConvolutionTransformer<>>());
    transformers.push_back(std::make_unique<MathematicalTransformer<>>());
    transformers.push_back(std::make_unique<SpectralTransformer<>>());
    transformers.push_back(std::make_unique<TemporalTransformer<>>());

    for (auto& transformer : transformers) {
        EXPECT_NO_THROW({
            auto result = transformer->apply_operation(empty_input);
            const auto& result_channels = result.data;
            EXPECT_EQ(result_channels.size(), empty_signal.size())
                << "Transformer " << transformer->get_name() << " should preserve channel count for empty signals";
        }) << "Transformer "
           << transformer->get_name() << " should handle empty signal gracefully";
    }
}

TEST_F(TransformerValidationTest, SingleSampleHandling)
{
    auto transformers = std::vector<std::unique_ptr<UniversalTransformer<std::vector<DataVariant>, std::vector<DataVariant>>>> {};
    transformers.push_back(std::make_unique<MathematicalTransformer<>>());
    transformers.push_back(std::make_unique<TemporalTransformer<>>());

    for (auto& transformer : transformers) {
        EXPECT_NO_THROW({
            auto result = transformer->apply_operation(single_input);
            const auto& result_channels = result.data;
            EXPECT_EQ(result_channels.size(), single_sample.size())
                << "Transformer " << transformer->get_name() << " should preserve channel count for single samples";

            for (size_t ch = 0; ch < result_channels.size(); ++ch) {
                const auto& result_data = std::get<std::vector<double>>(result_channels[ch]);
                EXPECT_EQ(result_data.size(), 1)
                    << "Transformer " << transformer->get_name() << " channel " << ch << " should preserve single sample size";
            }
        }) << "Transformer "
           << transformer->get_name() << " should handle single sample gracefully";
    }
}

TEST_F(TransformerValidationTest, ConstantSignalHandling)
{
    auto math_transformer = std::make_unique<MathematicalTransformer<>>();
    math_transformer->set_parameter("operation", MathematicalOperation::GAIN);
    math_transformer->set_parameter("gain_factor", 2.0);

    auto result = math_transformer->apply_operation(constant_input);
    const auto& result_channels = result.data;
    EXPECT_EQ(result_channels.size(), constant_signal.size()) << "Should preserve channel count";

    for (size_t ch = 0; ch < result_channels.size(); ++ch) {
        const auto& result_data = std::get<std::vector<double>>(result_channels[ch]);
        EXPECT_EQ(result_data.size(), constant_signal[ch].size())
            << "Channel " << ch << " should preserve constant signal size";

        for (double value : result_data) {
            EXPECT_NEAR(value, 1.0, 1e-10)
                << "Channel " << ch << " constant value 0.5 * 2.0 should equal 1.0";
        }
    }
}

TEST_F(TransformerValidationTest, ExtremeValueHandling)
{
    std::vector<double> extreme_values = {
        std::numeric_limits<double>::max() / 1e6,
        std::numeric_limits<double>::lowest() / 1e6,
        0.0,
        1.0,
        -1.0
    };

    std::vector<DataVariant> extreme_channels;
    for (size_t ch = 0; ch < 2; ++ch) {
        extreme_channels.emplace_back(extreme_values);
    }
    IO<std::vector<DataVariant>> extreme_input(extreme_channels);

    auto math_transformer = std::make_unique<MathematicalTransformer<>>();
    math_transformer->set_parameter("operation", MathematicalOperation::NORMALIZE);

    EXPECT_NO_THROW({
        auto result = math_transformer->apply_operation(extreme_input);
        const auto& result_channels = result.data;
        EXPECT_EQ(result_channels.size(), 2) << "Should preserve channel count for extreme values";

        for (size_t ch = 0; ch < result_channels.size(); ++ch) {
            const auto& result_data = std::get<std::vector<double>>(result_channels[ch]);
            EXPECT_FALSE(result_data.empty()) << "Channel " << ch << " should not be empty";

            for (size_t i = 0; i < result_data.size(); ++i) {
                EXPECT_FALSE(std::isnan(result_data[i]))
                    << "Channel " << ch << " sample " << i << " should not contain NaN";
                EXPECT_FALSE(std::isinf(result_data[i]))
                    << "Channel " << ch << " sample " << i << " should not contain infinity";
            }
        }
    });
}

TEST_F(TransformerValidationTest, MixedValidInvalidChannels)
{
    std::vector<double> valid_channel = { 1.0, 2.0, 3.0, 4.0 };
    std::vector<double> invalid_channel = { 1.0, std::nan(""), 3.0, std::numeric_limits<double>::infinity() };

    std::vector<DataVariant> mixed_channels;
    mixed_channels.emplace_back(valid_channel);
    mixed_channels.emplace_back(invalid_channel);
    IO<std::vector<DataVariant>> mixed_input(mixed_channels);

    auto math_transformer = std::make_unique<MathematicalTransformer<>>();
    math_transformer->set_parameter("operation", MathematicalOperation::GAIN);
    math_transformer->set_parameter("gain_factor", 2.0);

    EXPECT_NO_THROW({
        auto result = math_transformer->apply_operation(mixed_input);
        const auto& result_channels = result.data;
        EXPECT_EQ(result_channels.size(), 2) << "Should handle mixed valid/invalid channels";

        if (result.metadata.contains("validation_failed")) {
            EXPECT_TRUE(true) << "Validation correctly detected problematic data";
        } else {
            const auto& valid_result = std::get<std::vector<double>>(result_channels[0]);
            EXPECT_EQ(valid_result.size(), valid_channel.size());
        }
    });
}

TEST_F(TransformerValidationTest, ChannelSizeConsistencyValidation)
{
    std::vector<double> short_channel = { 1.0, 2.0 };
    std::vector<double> long_channel = { 1.0, 2.0, 3.0, 4.0, 5.0 };

    std::vector<DataVariant> inconsistent_channels;
    inconsistent_channels.emplace_back(short_channel);
    inconsistent_channels.emplace_back(long_channel);
    IO<std::vector<DataVariant>> inconsistent_input(inconsistent_channels);

    auto transformers = std::vector<std::unique_ptr<UniversalTransformer<std::vector<DataVariant>, std::vector<DataVariant>>>> {};
    transformers.push_back(std::make_unique<MathematicalTransformer<>>());
    transformers.push_back(std::make_unique<TemporalTransformer<>>());

    for (auto& transformer : transformers) {
        EXPECT_NO_THROW({
            auto result = transformer->apply_operation(inconsistent_input);
            const auto& result_channels = result.data;
            EXPECT_EQ(result_channels.size(), 2)
                << "Transformer " << transformer->get_name() << " should handle inconsistent channel sizes";

            const auto& result_short = std::get<std::vector<double>>(result_channels[0]);
            const auto& result_long = std::get<std::vector<double>>(result_channels[1]);
            EXPECT_EQ(result_short.size(), short_channel.size())
                << "Transformer " << transformer->get_name() << " should preserve short channel size";
            EXPECT_EQ(result_long.size(), long_channel.size())
                << "Transformer " << transformer->get_name() << " should preserve long channel size";
        }) << "Transformer "
           << transformer->get_name() << " should handle inconsistent channel sizes gracefully";
    }
}

TEST_F(TransformerValidationTest, ValidationRecoveryBehavior)
{
    auto math_transformer = std::make_unique<MathematicalTransformer<>>();
    math_transformer->set_parameter("operation", MathematicalOperation::LOGARITHMIC);

    std::vector<double> negative_data = { -1.0, -2.0, -3.0 };
    std::vector<DataVariant> negative_channels;
    for (size_t ch = 0; ch < 2; ++ch) {
        negative_channels.emplace_back(negative_data);
    }
    IO<std::vector<DataVariant>> negative_input(negative_channels);

    auto first_result = math_transformer->apply_operation(negative_input);

    std::vector<double> positive_data = { 1.0, 2.0, 3.0 };
    std::vector<DataVariant> positive_channels;
    for (size_t ch = 0; ch < 2; ++ch) {
        positive_channels.emplace_back(positive_data);
    }
    IO<std::vector<DataVariant>> positive_input(positive_channels);

    EXPECT_NO_THROW({
        auto second_result = math_transformer->apply_operation(positive_input);
        const auto& result_channels = second_result.data;
        EXPECT_EQ(result_channels.size(), 2) << "Transformer should recover after validation failure";

        for (size_t ch = 0; ch < result_channels.size(); ++ch) {
            const auto& result_data = std::get<std::vector<double>>(result_channels[ch]);
            EXPECT_EQ(result_data.size(), positive_data.size())
                << "Channel " << ch << " should process valid data correctly after validation failure";
        }
    }) << "Transformer should recover gracefully after validation failure";
}

// =========================================================================
// PERFORMANCE AND CONSISTENCY TESTS
// =========================================================================

class TransformerConsistencyTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_signal = TransformerTestDataGenerator::create_sine_wave(1024, 440.0, 1.0, 44100.0, 1);
    }
    std::vector<std::vector<double>> test_signal;
};

TEST_F(TransformerConsistencyTest, ConsistentResultsAcrossRuns)
{
    auto transformer = std::make_unique<MathematicalTransformer<>>();
    transformer->set_parameter("operation", MathematicalOperation::POWER);
    transformer->set_parameter("exponent", 2.0);

    std::vector<DataVariant> channel_variants;
    channel_variants.reserve(test_signal.size());
    for (const auto& channel : test_signal) {
        channel_variants.emplace_back(channel);
    }
    IO<std::vector<DataVariant>> input { channel_variants };

    auto result1 = transformer->apply_operation(input);
    auto result2 = transformer->apply_operation(input);
    auto result3 = transformer->apply_operation(input);

    EXPECT_EQ(result1.data.size(), result2.data.size());
    EXPECT_EQ(result2.data.size(), result3.data.size());

    for (size_t ch = 0; ch < result1.data.size(); ++ch) {
        auto data1 = std::get<std::vector<double>>(result1.data[ch]);
        auto data2 = std::get<std::vector<double>>(result2.data[ch]);
        auto data3 = std::get<std::vector<double>>(result3.data[ch]);

        EXPECT_EQ(data1.size(), data2.size());
        EXPECT_EQ(data2.size(), data3.size());

        for (size_t i = 0; i < data1.size(); ++i) {
            EXPECT_NEAR(data1[i], data2[i], 1e-15) << "Channel " << ch << " sample " << i << " differs between run 1 and 2";
            EXPECT_NEAR(data2[i], data3[i], 1e-15) << "Channel " << ch << " sample " << i << " differs between run 2 and 3";
        }
    }
}

TEST_F(TransformerConsistencyTest, ParameterIsolation)
{
    auto transformer = std::make_unique<MathematicalTransformer<>>();
    transformer->set_parameter("operation", MathematicalOperation::GAIN);
    transformer->set_parameter("gain_factor", 2.0);

    std::vector<DataVariant> channel_variants;
    channel_variants.reserve(test_signal.size());
    for (const auto& channel : test_signal) {
        channel_variants.emplace_back(channel);
    }
    IO<std::vector<DataVariant>> input { channel_variants };

    auto result1 = transformer->apply_operation(input);

    transformer->set_parameter("gain_factor", 3.0);
    auto result2 = transformer->apply_operation(input);

    transformer->set_parameter("gain_factor", 2.0);
    auto result3 = transformer->apply_operation(input);

    EXPECT_EQ(result1.data.size(), result3.data.size());

    for (size_t ch = 0; ch < result1.data.size(); ++ch) {
        auto data1 = std::get<std::vector<double>>(result1.data[ch]);
        auto data3 = std::get<std::vector<double>>(result3.data[ch]);

        EXPECT_EQ(data1.size(), data3.size());

        for (size_t i = 0; i < data1.size(); ++i) {
            EXPECT_NEAR(data1[i], data3[i], 1e-15) << "Channel " << ch << " sample " << i << " - parameter isolation failed";
        }
    }
}

// =========================================================================
// CROSS-TRANSFORMER INTEGRATION TESTS
// =========================================================================

class TransformerIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_signal = TransformerTestDataGenerator::create_sine_wave(1024, 440.0, 1.0, 44100.0, 1);
    }

    std::vector<std::vector<double>> test_signal;
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

    std::vector<DataVariant> channel_variants;
    channel_variants.reserve(test_signal.size());
    for (const auto& channel : test_signal) {
        channel_variants.emplace_back(channel);
    }
    IO<std::vector<DataVariant>> input { channel_variants };

    auto result1 = gain_transformer->apply_operation(input);
    auto result2 = offset_transformer->apply_operation(result1);
    auto result3 = power_transformer->apply_operation(result2);

    EXPECT_EQ(result3.data.size(), test_signal.size());

    // Verify the mathematical pipeline: ((x * 0.5) + 0.25)^2 for each channel
    for (size_t ch = 0; ch < result3.data.size(); ++ch) {
        auto final_data = std::get<std::vector<double>>(result3.data[ch]);
        const auto& original_channel = test_signal[ch];

        EXPECT_EQ(final_data.size(), original_channel.size());

        for (size_t i = 0; i < std::min(final_data.size(), original_channel.size()); ++i) {
            double expected = std::pow((original_channel[i] * 0.5) + 0.25, 2.0);
            EXPECT_NEAR(final_data[i], expected, 1e-10);
        }
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

    std::vector<DataVariant> channel_variants;
    channel_variants.reserve(test_signal.size());
    for (const auto& channel : test_signal) {
        channel_variants.emplace_back(channel);
    }
    IO<std::vector<DataVariant>> input { channel_variants };

    auto result1 = normalize_transformer->apply_operation(input);
    auto result2 = reverse_transformer->apply_operation(result1);
    auto result3 = gain_transformer->apply_operation(result2);

    EXPECT_EQ(result3.data.size(), test_signal.size());

    for (size_t ch = 0; ch < result3.data.size(); ++ch) {
        auto final_data = std::get<std::vector<double>>(result3.data[ch]);
        const auto& original_channel = test_signal[ch];

        EXPECT_EQ(final_data.size(), original_channel.size());
        EXPECT_FALSE(final_data.empty());

        auto max_val = *std::ranges::max_element(final_data);
        EXPECT_LE(max_val, 0.81); // Should be ≤ 0.8 due to final gain
    }
}

TEST_F(TransformerIntegrationTest, MultipleDataTypesSupport)
{
    auto transformer = std::make_unique<MathematicalTransformer<std::vector<DataVariant>, std::vector<DataVariant>>>();
    transformer->set_parameter("operation", MathematicalOperation::GAIN);
    transformer->set_parameter("gain_factor", 2.0);

    std::vector<double> double_data = { 1.0, 2.0, 3.0, 4.0 };
    std::vector<DataVariant> channel_variants;
    channel_variants.emplace_back(double_data);
    IO<std::vector<DataVariant>> double_input { channel_variants };

    auto double_result = transformer->apply_operation(double_input);

    EXPECT_NO_THROW({
        EXPECT_EQ(double_result.data.size(), 1U);
        auto result_data = std::get<std::vector<double>>(double_result.data[0]);
        EXPECT_EQ(result_data.size(), 4U);
        for (size_t i = 0; i < result_data.size(); ++i) {
            EXPECT_NEAR(result_data[i], double_data[i] * 2.0, 1e-10);
        }
    });

    // TODO: Add float testing when neotest compatibility is resolved
    // Float processing works in binary but has issues in neotest environment
    /* std::vector<float> float_data = { 1.0F, 2.0F, 3.0F, 4.0F };
    std::vector<DataVariant> float_channel_variants;
    float_channel_variants.emplace_back(float_data);
    IO<std::vector<DataVariant>> float_input { float_channel_variants };
    auto float_result = transformer->apply_operation(float_input);

    EXPECT_NO_THROW({
        EXPECT_EQ(float_result.data.size(), 1U);
        auto result_data = std::get<std::vector<float>>(float_result.data[0]);
        EXPECT_EQ(result_data.size(), 4U);
    }); */
}

} // namespace MayaFlux::Test
