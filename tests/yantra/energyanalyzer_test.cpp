#include "../mock_signalsourcecontainer.hpp"
#include "MayaFlux/Kakshya/SignalSourceContainer.hpp"
#include "MayaFlux/Yantra/Analyzers/EnergyAnalyzer.hpp"

using namespace MayaFlux::Yantra;
using namespace MayaFlux::Kakshya;

namespace MayaFlux::Test {

class EnergyAnalyzerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_data.resize(1024);
        for (size_t i = 0; i < test_data.size(); ++i)
            test_data[i] = static_cast<double>(i) / 1023.0;

        container = std::make_shared<MockSignalSourceContainer>();
        container->set_test_data(test_data);

        analyzer = std::make_unique<EnergyAnalyzer<std::shared_ptr<SignalSourceContainer>, Eigen::MatrixXd>>(256, 128);
    }

    std::vector<double> test_data;
    std::shared_ptr<MockSignalSourceContainer> container;
    std::unique_ptr<EnergyAnalyzer<std::shared_ptr<SignalSourceContainer>, Eigen::MatrixXd>> analyzer;
};

TEST_F(EnergyAnalyzerTest, CalculateRMSEnergy)
{
    analyzer->set_parameter("method", "rms");
    analyzer->set_analysis_granularity(AnalysisGranularity::RAW_VALUES);

    IO<std::shared_ptr<SignalSourceContainer>> input(container);
    auto pipeline_result = analyzer->apply_operation(input);

    EXPECT_TRUE(pipeline_result.data.size() > 0);

    auto analysis_result = analyzer->analyze_energy(container);
    EXPECT_FALSE(analysis_result.channels.empty());
    EXPECT_EQ(analysis_result.method_used, EnergyMethod::RMS);

    for (const auto& channel : analysis_result.channels) {
        for (double v : channel.energy_values) {
            EXPECT_GE(v, 0.0);
        }
    }
}

TEST_F(EnergyAnalyzerTest, CalculatePeakEnergy)
{
    analyzer->set_parameter("method", "peak");
    analyzer->set_analysis_granularity(AnalysisGranularity::RAW_VALUES);

    auto analysis_result = analyzer->analyze_energy(container);
    EXPECT_FALSE(analysis_result.channels.empty());
    EXPECT_EQ(analysis_result.method_used, EnergyMethod::PEAK);

    for (const auto& channel : analysis_result.channels) {
        for (double v : channel.energy_values) {
            EXPECT_GE(v, 0.0);
        }
    }
}

TEST_F(EnergyAnalyzerTest, CalculateSpectralEnergy)
{
    analyzer->set_parameter("method", "spectral");
    analyzer->set_analysis_granularity(AnalysisGranularity::RAW_VALUES);

    auto analysis_result = analyzer->analyze_energy(container);
    EXPECT_FALSE(analysis_result.channels.empty());
    EXPECT_EQ(analysis_result.method_used, EnergyMethod::SPECTRAL);

    for (const auto& channel : analysis_result.channels) {
        for (double v : channel.energy_values) {
            EXPECT_GE(v, 0.0);
        }
    }
}

TEST_F(EnergyAnalyzerTest, EnergyAnalysisResultStructure)
{
    analyzer->set_parameter("method", "rms");

    auto analysis_result = analyzer->analyze_energy(container);

    EXPECT_FALSE(analysis_result.channels.empty());
    EXPECT_EQ(analysis_result.window_size, 256);
    EXPECT_EQ(analysis_result.hop_size, 128);

    const auto& first_channel = analysis_result.channels[0];
    EXPECT_GT(first_channel.mean_energy, 0.0);
    EXPECT_GE(first_channel.max_energy, first_channel.min_energy);
    EXPECT_EQ(first_channel.window_positions.size(), first_channel.energy_values.size());

    for (const auto& [start, end] : first_channel.window_positions) {
        EXPECT_LT(start, end);
        EXPECT_LE(end, test_data.size());
    }
}

TEST_F(EnergyAnalyzerTest, EnergyClassification)
{
    analyzer->set_energy_thresholds(0.01, 0.05, 0.1, 0.5);
    analyzer->enable_classification(true);
    analyzer->set_parameter("method", "rms");

    auto analysis_result = analyzer->analyze_energy(container);

    EXPECT_FALSE(analysis_result.channels.empty());

    for (const auto& channel : analysis_result.channels) {
        EXPECT_FALSE(channel.classifications.empty());
        EXPECT_EQ(channel.classifications.size(), channel.energy_values.size());

        bool has_classifications = false;
        for (int level_count : channel.level_counts) {
            if (level_count > 0) {
                has_classifications = true;
                break;
            }
        }
        EXPECT_TRUE(has_classifications);

        for (const auto& level : channel.classifications) {
            EXPECT_TRUE(level >= EnergyLevel::SILENT && level <= EnergyLevel::PEAK);
        }
    }
}

TEST_F(EnergyAnalyzerTest, PipelineMetadata)
{
    analyzer->set_parameter("method", "rms");

    IO<std::shared_ptr<SignalSourceContainer>> input(container);
    auto pipeline_result = analyzer->apply_operation(input);

    EXPECT_TRUE(pipeline_result.metadata.contains("source_analyzer"));
    EXPECT_TRUE(pipeline_result.metadata.contains("energy_method"));
    EXPECT_TRUE(pipeline_result.metadata.contains("window_size"));
    EXPECT_TRUE(pipeline_result.metadata.contains("hop_size"));
    EXPECT_TRUE(pipeline_result.metadata.contains("num_channels"));

    auto method = safe_any_cast_or_default<std::string>(pipeline_result.metadata["energy_method"], "");
    EXPECT_EQ(method, "rms");

    EXPECT_TRUE(pipeline_result.metadata.contains("mean_energy_per_channel"));
    EXPECT_TRUE(pipeline_result.metadata.contains("max_energy_per_channel"));
}

TEST_F(EnergyAnalyzerTest, AnalysisDataAccessibility)
{
    analyzer->set_parameter("method", "peak");

    IO<std::shared_ptr<SignalSourceContainer>> input(container);
    auto pipeline_result = analyzer->apply_operation(input);

    auto cached_analysis = analyzer->get_energy_analysis();
    EXPECT_FALSE(cached_analysis.channels.empty());
    EXPECT_EQ(cached_analysis.method_used, EnergyMethod::PEAK);

    auto generic_analysis = analyzer->get_current_analysis();
    auto typed_analysis = safe_any_cast_or_throw<EnergyAnalysis>(generic_analysis);
    EXPECT_EQ(typed_analysis.method_used, EnergyMethod::PEAK);
}

TEST_F(EnergyAnalyzerTest, BatchAnalysis)
{
    std::vector<std::shared_ptr<SignalSourceContainer>> containers;
    for (int i = 0; i < 3; ++i) {
        auto cont = std::make_shared<MockSignalSourceContainer>();
        cont->set_test_data(test_data);
        containers.push_back(cont);
    }

    auto batch_results = analyzer->analyze_batch(containers);
    EXPECT_EQ(batch_results.size(), 3);

    for (const auto& result : batch_results) {
        auto energy_result = safe_any_cast_or_throw<EnergyAnalysis>(result);
        EXPECT_FALSE(energy_result.channels.empty());
    }
}

TEST_F(EnergyAnalyzerTest, RMSEnergyCorrectness)
{
    std::vector<double> sine_data(1024);
    double amplitude = 0.5;
    double frequency = 10.0;

    for (size_t i = 0; i < sine_data.size(); ++i) {
        sine_data[i] = amplitude * std::sin(2.0 * M_PI * frequency * static_cast<double>(i) / static_cast<double>(sine_data.size()));
    }

    container->set_test_data(sine_data);
    analyzer->set_parameter("method", "rms");

    auto analysis_result = analyzer->analyze_energy(container);

    // For a sine wave, RMS should be amplitude / sqrt(2) ≈ 0.5 / 1.414 ≈ 0.354
    double expected_rms = amplitude / std::numbers::sqrt2;
    double tolerance = 0.05;

    const auto& first_channel = analysis_result.channels[0];
    for (double rms_value : first_channel.energy_values) {
        EXPECT_NEAR(rms_value, expected_rms, tolerance);
    }

    EXPECT_NEAR(first_channel.mean_energy, expected_rms, tolerance);
}

TEST_F(EnergyAnalyzerTest, PeakEnergyCorrectness)
{
    std::vector<double> impulse_signal(1024, 0.1);
    double peak_amplitude = 0.8;

    for (size_t i = 25; i < impulse_signal.size(); i += 50) {
        impulse_signal[i] = peak_amplitude;
    }

    container->set_test_data(impulse_signal);
    analyzer->set_parameter("method", "peak");

    auto analysis_result = analyzer->analyze_energy(container);

    double tolerance = 0.05;

    const auto& first_channel = analysis_result.channels[0];
    for (double peak_value : first_channel.energy_values) {
        EXPECT_NEAR(peak_value, peak_amplitude, tolerance);
    }

    EXPECT_NEAR(first_channel.max_energy, peak_amplitude, tolerance);
}

TEST_F(EnergyAnalyzerTest, EnergyClassificationCorrectness)
{
    std::vector<double> varied_signal(1024);

    // First quarter: silent (0.005 - below silent threshold of 0.01)
    // Second quarter: quiet (0.03 - between 0.01 and 0.05)
    // Third quarter: moderate (0.07 - between 0.05 and 0.1)
    // Fourth quarter: loud (0.3 - between 0.1 and 0.5)

    for (size_t i = 0; i < varied_signal.size(); ++i) {
        if (i < 256) {
            varied_signal[i] = 0.005;
        } else if (i < 512) {
            varied_signal[i] = 0.03;
        } else if (i < 768) {
            varied_signal[i] = 0.07;
        } else {
            varied_signal[i] = 0.3;
        }
    }

    container->set_test_data(varied_signal);
    analyzer->set_energy_thresholds(0.01, 0.05, 0.1, 0.5);
    analyzer->enable_classification(true);
    analyzer->set_parameter("method", "rms");

    auto analysis_result = analyzer->analyze_energy(container);

    for (const auto& channel : analysis_result.channels) {
        EXPECT_GT(channel.level_counts[static_cast<size_t>(EnergyLevel::SILENT)], 0);
        EXPECT_GT(channel.level_counts[static_cast<size_t>(EnergyLevel::QUIET)], 0);
        EXPECT_GT(channel.level_counts[static_cast<size_t>(EnergyLevel::MODERATE)], 0);
        EXPECT_GT(channel.level_counts[static_cast<size_t>(EnergyLevel::LOUD)], 0);

        EXPECT_TRUE(std::ranges::any_of(channel.classifications,
            [](EnergyLevel level) { return level == EnergyLevel::SILENT; }));

        EXPECT_TRUE(std::ranges::any_of(channel.classifications,
            [](EnergyLevel level) { return level == EnergyLevel::LOUD; }));
    }
}

TEST_F(EnergyAnalyzerTest, SilentSignalCorrectness)
{
    std::vector<double> silent_signal(1024, 0.0);

    container->set_test_data(silent_signal);
    analyzer->set_parameter("method", "rms");

    auto analysis_result = analyzer->analyze_energy(container);

    const auto& first_channel = analysis_result.channels[0];
    for (double energy_value : first_channel.energy_values) {
        EXPECT_NEAR(energy_value, 0.0, 1e-10);
    }

    EXPECT_NEAR(first_channel.mean_energy, 0.0, 1e-10);
    EXPECT_NEAR(first_channel.min_energy, 0.0, 1e-10);
    EXPECT_NEAR(first_channel.max_energy, 0.0, 1e-10);
}

TEST_F(EnergyAnalyzerTest, MultiChannelAnalysis)
{
    std::vector<std::vector<double>> multi_channel_data(2);
    multi_channel_data[0] = std::vector<double>(1024, 0.3); // Channel 0: constant 0.3
    multi_channel_data[1] = std::vector<double>(1024, 0.7); // Channel 1: constant 0.7

    container->set_multi_channel_test_data(multi_channel_data);
    analyzer->set_parameter("method", "rms");

    auto analysis_result = analyzer->analyze_energy(container);

    EXPECT_EQ(analysis_result.channels.size(), 2);

    std::vector<double> expected_means = { 0.3, 0.7 };
    for (size_t ch = 0; ch < analysis_result.channels.size(); ++ch) {
        EXPECT_NEAR(analysis_result.channels[ch].mean_energy, expected_means[ch], 0.01);
    }

    EXPECT_NE(analysis_result.channels[0].mean_energy, analysis_result.channels[1].mean_energy);
}

TEST_F(EnergyAnalyzerTest, WindowSizeImpactOnResolution)
{
    auto analyzer_small = std::make_unique<EnergyAnalyzer<std::shared_ptr<SignalSourceContainer>, Eigen::MatrixXd>>(128, 64);
    auto analyzer_large = std::make_unique<EnergyAnalyzer<std::shared_ptr<SignalSourceContainer>, Eigen::MatrixXd>>(512, 256);

    analyzer_small->set_parameter("method", "rms");
    analyzer_large->set_parameter("method", "rms");

    auto result_small = analyzer_small->analyze_energy(container);
    auto result_large = analyzer_large->analyze_energy(container);

    EXPECT_GT(result_small.channels[0].energy_values.size(), result_large.channels[0].energy_values.size());

    EXPECT_EQ(result_small.window_size, 128);
    EXPECT_EQ(result_large.window_size, 512);
}

} // namespace MayaFlux::Test
