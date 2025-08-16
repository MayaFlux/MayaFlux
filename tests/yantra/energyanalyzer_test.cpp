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

        analyzer = std::make_unique<ContainerEnergyAnalyzer>(256, 128);
    }

    std::vector<double> test_data;
    std::shared_ptr<MockSignalSourceContainer> container;
    std::unique_ptr<ContainerEnergyAnalyzer> analyzer;
};

TEST_F(EnergyAnalyzerTest, CalculateRMSEnergy)
{
    analyzer->set_parameter("method", "rms");
    analyzer->set_analysis_granularity(AnalysisGranularity::RAW_VALUES);

    IO<std::shared_ptr<SignalSourceContainer>> input(container);
    auto pipeline_result = analyzer->apply_operation(input);

    EXPECT_TRUE(pipeline_result.data.size() > 0);

    auto analysis_result = analyzer->analyze_energy(container);
    EXPECT_FALSE(analysis_result.energy_values.empty());
    EXPECT_EQ(analysis_result.method_used, EnergyMethod::RMS);

    for (double v : analysis_result.energy_values) {
        EXPECT_GE(v, 0.0);
    }
}

TEST_F(EnergyAnalyzerTest, CalculatePeakEnergy)
{
    analyzer->set_parameter("method", "peak");
    analyzer->set_analysis_granularity(AnalysisGranularity::RAW_VALUES);

    auto analysis_result = analyzer->analyze_energy(container);
    EXPECT_FALSE(analysis_result.energy_values.empty());
    EXPECT_EQ(analysis_result.method_used, EnergyMethod::PEAK);

    for (double v : analysis_result.energy_values) {
        EXPECT_GE(v, 0.0);
    }
}

TEST_F(EnergyAnalyzerTest, CalculateSpectralEnergy)
{
    analyzer->set_parameter("method", "spectral");
    analyzer->set_analysis_granularity(AnalysisGranularity::RAW_VALUES);

    auto analysis_result = analyzer->analyze_energy(container);
    EXPECT_FALSE(analysis_result.energy_values.empty());
    EXPECT_EQ(analysis_result.method_used, EnergyMethod::SPECTRAL);

    for (double v : analysis_result.energy_values) {
        EXPECT_GE(v, 0.0);
    }
}

TEST_F(EnergyAnalyzerTest, CalculateHarmonicEnergy)
{
    analyzer->set_parameter("method", "harmonic");

    auto analysis_result = analyzer->analyze_energy(container);
    EXPECT_FALSE(analysis_result.energy_values.empty());
    EXPECT_EQ(analysis_result.method_used, EnergyMethod::HARMONIC);

    for (double v : analysis_result.energy_values) {
        EXPECT_GE(v, 0.0);
    }
}

TEST_F(EnergyAnalyzerTest, EnergyAnalysisResultStructure)
{
    analyzer->set_parameter("method", "rms");

    auto analysis_result = analyzer->analyze_energy(container);

    EXPECT_FALSE(analysis_result.energy_values.empty());
    EXPECT_EQ(analysis_result.window_size, 256);
    EXPECT_EQ(analysis_result.hop_size, 128);
    EXPECT_GT(analysis_result.mean_energy, 0.0);
    EXPECT_GE(analysis_result.max_energy, analysis_result.min_energy);
    EXPECT_EQ(analysis_result.window_positions.size(), analysis_result.energy_values.size());

    for (const auto& [start, end] : analysis_result.window_positions) {
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

    EXPECT_FALSE(analysis_result.energy_classifications.empty());
    EXPECT_EQ(analysis_result.energy_classifications.size(), analysis_result.energy_values.size());
    EXPECT_FALSE(analysis_result.level_distribution.empty());

    for (const auto& level : analysis_result.energy_classifications) {
        EXPECT_TRUE(level >= EnergyLevel::SILENT && level <= EnergyLevel::PEAK);
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

    auto method = safe_any_cast_or_default<std::string>(pipeline_result.metadata["energy_method"], "");
    EXPECT_EQ(method, "rms");
}

TEST_F(EnergyAnalyzerTest, AnalysisDataAccessibility)
{
    analyzer->set_parameter("method", "peak");

    IO<std::shared_ptr<SignalSourceContainer>> input(container);
    auto pipeline_result = analyzer->apply_operation(input);

    auto cached_analysis = analyzer->get_energy_analysis();
    EXPECT_FALSE(cached_analysis.energy_values.empty());
    EXPECT_EQ(cached_analysis.method_used, EnergyMethod::PEAK);

    auto generic_analysis = analyzer->get_current_analysis();
    auto typed_analysis = safe_any_cast_or_throw<EnergyAnalysisResult>(generic_analysis);
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
        auto energy_result = safe_any_cast_or_throw<EnergyAnalysisResult>(result);
        EXPECT_FALSE(energy_result.energy_values.empty());
    }
}

TEST_F(EnergyAnalyzerTest, InvalidContainerThrows)
{
    std::shared_ptr<MockSignalSourceContainer> empty_container = std::make_shared<MockSignalSourceContainer>();
    empty_container->set_test_data({});

    EXPECT_THROW(analyzer->analyze_energy(empty_container), std::runtime_error);
}

TEST_F(EnergyAnalyzerTest, WindowParameterValidation)
{
    EXPECT_THROW(analyzer->set_window_parameters(0, 128), std::invalid_argument);
    EXPECT_THROW(analyzer->set_window_parameters(256, 0), std::invalid_argument);
    EXPECT_THROW(analyzer->set_window_parameters(128, 256), std::invalid_argument);
}

TEST_F(EnergyAnalyzerTest, ThresholdValidation)
{
    EXPECT_THROW(analyzer->set_energy_thresholds(0.5, 0.1, 0.05, 0.01), std::invalid_argument);

    EXPECT_NO_THROW(analyzer->set_energy_thresholds(0.01, 0.05, 0.1, 0.5));
}

TEST_F(EnergyAnalyzerTest, RMSEnergyCorrectness)
{
    std::vector<double> sine_data(1024);
    double amplitude = 0.5;
    double frequency = 10.0;

    for (size_t i = 0; i < sine_data.size(); ++i) {
        sine_data[i] = amplitude * std::sin(2.0 * M_PI * frequency * i / sine_data.size());
    }

    container->set_test_data(sine_data);
    analyzer->set_parameter("method", "rms");

    auto analysis_result = analyzer->analyze_energy(container);

    // For a sine wave, RMS should be amplitude / sqrt(2) ≈ 0.5 / 1.414 ≈ 0.354
    double expected_rms = amplitude / std::numbers::sqrt2;
    double tolerance = 0.05;

    for (double rms_value : analysis_result.energy_values) {
        EXPECT_NEAR(rms_value, expected_rms, tolerance);
    }

    EXPECT_NEAR(analysis_result.mean_energy, expected_rms, tolerance);
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

    for (double peak_value : analysis_result.energy_values) {
        EXPECT_NEAR(peak_value, peak_amplitude, tolerance);
    }

    EXPECT_NEAR(analysis_result.max_energy, peak_amplitude, tolerance);
}

TEST_F(EnergyAnalyzerTest, ZeroCrossingCorrectness)
{
    std::vector<double> square_wave(1024);
    int cycles = 8;

    for (size_t i = 0; i < square_wave.size(); ++i) {
        double t = static_cast<double>(i) / square_wave.size();
        square_wave[i] = (std::sin(2.0 * M_PI * cycles * t) > 0) ? 1.0 : -1.0;
    }

    container->set_test_data(square_wave);
    analyzer->set_parameter("method", "zero_crossing");

    auto analysis_result = analyzer->analyze_energy(container);

    // Square wave should have high zero crossing rate
    // Each cycle has 2 zero crossings, so 8 cycles = 16 crossings over 1024 samples
    // In a window of 256 samples, we expect roughly 4 crossings
    // ZCR = crossings / (window_size - 1) ≈ 4/255 ≈ 0.016

    double expected_zcr = (2.0 * cycles * analyzer->get_parameter_or_default<u_int32_t>("window_size", 256))
        / (1024.0 * (analyzer->get_parameter_or_default<u_int32_t>("window_size", 256) - 1));

    for (double zcr_value : analysis_result.energy_values) {
        EXPECT_GT(zcr_value, 0.01);
        EXPECT_LT(zcr_value, 0.1);
    }
}

TEST_F(EnergyAnalyzerTest, PowerEnergyCorrectness)
{
    std::vector<double> constant_signal(1024, 0.6);

    container->set_test_data(constant_signal);
    analyzer->set_parameter("method", "power");

    auto analysis_result = analyzer->analyze_energy(container);

    // Power = sum of squares = window_size * (0.6)^2 = 256 * 0.36 = 92.16
    double expected_power = analyzer->get_parameter_or_default<u_int32_t>("window_size", 256) * 0.6 * 0.6;
    double tolerance = 1.0;

    for (double power_value : analysis_result.energy_values) {
        EXPECT_NEAR(power_value, expected_power, tolerance);
    }
}

TEST_F(EnergyAnalyzerTest, DynamicRangeCorrectness)
{
    std::vector<double> dynamic_signal(1024);
    double min_val = 0.01;
    double max_val = 0.5;

    for (size_t i = 0; i < dynamic_signal.size(); ++i) {
        double t = static_cast<double>(i) / dynamic_signal.size();
        dynamic_signal[i] = min_val + (max_val - min_val) * std::abs(std::sin(2.0 * M_PI * t));
    }

    container->set_test_data(dynamic_signal);
    analyzer->set_parameter("method", "dynamic_range");

    auto analysis_result = analyzer->analyze_energy(container);

    // Expected dynamic range in dB = 20 * log10(max_val / min_val)
    double expected_dr = 20.0 * std::log10(max_val / min_val);
    double tolerance = 5.0;

    for (double dr_value : analysis_result.energy_values) {
        EXPECT_GT(dr_value, 0.0);
        EXPECT_LT(dr_value, expected_dr + tolerance);
    }
}

TEST_F(EnergyAnalyzerTest, SilentSignalCorrectness)
{
    std::vector<double> silent_signal(1024, 0.0);

    container->set_test_data(silent_signal);
    analyzer->set_parameter("method", "rms");

    auto analysis_result = analyzer->analyze_energy(container);

    for (double energy_value : analysis_result.energy_values) {
        EXPECT_NEAR(energy_value, 0.0, 1e-10);
    }

    EXPECT_NEAR(analysis_result.mean_energy, 0.0, 1e-10);
    EXPECT_NEAR(analysis_result.min_energy, 0.0, 1e-10);
    EXPECT_NEAR(analysis_result.max_energy, 0.0, 1e-10);
}

TEST_F(EnergyAnalyzerTest, EnergyClassificationCorrectness)
{
    std::vector<double> varied_signal(1024);

    // First quarter: silent (0.005 - below silent threshold of 0.01)
    // Second quarter: quiet (0.03 - between 0.01 and 0.05)
    // Third quarter: moderate (0.07 - between 0.05 and 0.1)
    // Fourth quarter: loud (0.3 - between 0.1 and 0.5)

    for (size_t i = 0; i < varied_signal.size(); ++i) {
        if (i < 256)
            varied_signal[i] = 0.005;
        else if (i < 512)
            varied_signal[i] = 0.03;
        else if (i < 768)
            varied_signal[i] = 0.07;
        else
            varied_signal[i] = 0.3;
    }

    container->set_test_data(varied_signal);
    analyzer->set_energy_thresholds(0.01, 0.05, 0.1, 0.5);
    analyzer->enable_classification(true);
    analyzer->set_parameter("method", "rms");

    auto analysis_result = analyzer->analyze_energy(container);

    EXPECT_GT(analysis_result.level_distribution[EnergyLevel::SILENT], 0);
    EXPECT_GT(analysis_result.level_distribution[EnergyLevel::QUIET], 0);
    EXPECT_GT(analysis_result.level_distribution[EnergyLevel::MODERATE], 0);
    EXPECT_GT(analysis_result.level_distribution[EnergyLevel::LOUD], 0);

    EXPECT_TRUE(std::ranges::any_of(analysis_result.energy_classifications,
        [](EnergyLevel level) { return level == EnergyLevel::SILENT; }));

    EXPECT_TRUE(std::ranges::any_of(analysis_result.energy_classifications,
        [](EnergyLevel level) { return level == EnergyLevel::LOUD; }));
}

TEST_F(EnergyAnalyzerTest, WindowSizeImpactOnResolution)
{
    auto analyzer_small = std::make_unique<ContainerEnergyAnalyzer>(128, 64);
    auto analyzer_large = std::make_unique<ContainerEnergyAnalyzer>(512, 256);

    analyzer_small->set_parameter("method", "rms");
    analyzer_large->set_parameter("method", "rms");

    auto result_small = analyzer_small->analyze_energy(container);
    auto result_large = analyzer_large->analyze_energy(container);

    EXPECT_GT(result_small.energy_values.size(), result_large.energy_values.size());

    EXPECT_EQ(result_small.window_size, 128);
    EXPECT_EQ(result_large.window_size, 512);
}
} // namespace MayaFlux::Test
