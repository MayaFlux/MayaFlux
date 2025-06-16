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
        // Simple test signal: ramp from 0 to 1
        test_data.resize(1024);
        for (size_t i = 0; i < test_data.size(); ++i)
            test_data[i] = static_cast<double>(i) / 1023.0f;

        container = std::make_shared<MockSignalSourceContainer>();
        container->set_test_data(test_data);
        analyzer = std::make_unique<EnergyAnalyzer>(256, 128);
    }

    std::vector<double> test_data;
    std::shared_ptr<MockSignalSourceContainer> container;
    std::unique_ptr<EnergyAnalyzer> analyzer;
};

TEST_F(EnergyAnalyzerTest, CalculateRMSEnergy)
{
    analyzer->set_parameter("method", "rms");
    analyzer->set_output_granularity(AnalysisGranularity::RAW_VALUES);

    AnalyzerInput input = container;
    auto result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& values = std::get<std::vector<double>>(result);

    EXPECT_FALSE(values.empty());
    for (double v : values) {
        EXPECT_GE(v, 0.0);
    }
}

TEST_F(EnergyAnalyzerTest, CalculatePeakEnergy)
{
    analyzer->set_parameter("method", "peak");
    analyzer->set_output_granularity(AnalysisGranularity::RAW_VALUES);

    AnalyzerInput input = container;
    auto result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& values = std::get<std::vector<double>>(result);

    EXPECT_FALSE(values.empty());
    for (double v : values) {
        EXPECT_GE(v, 0.0);
    }
}

TEST_F(EnergyAnalyzerTest, CalculateSpectralEnergy)
{
    analyzer->set_parameter("method", "spectral");
    analyzer->set_output_granularity(AnalysisGranularity::RAW_VALUES);

    AnalyzerInput input = container;
    auto result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& values = std::get<std::vector<double>>(result);

    EXPECT_FALSE(values.empty());
    for (double v : values) {
        EXPECT_GE(v, 0.0);
    }
}

TEST_F(EnergyAnalyzerTest, EnergyRegionsOutput)
{
    analyzer->set_parameter("method", "rms");
    analyzer->set_output_granularity(AnalysisGranularity::ORGANIZED_GROUPS);

    AnalyzerInput input = container;
    auto result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<RegionGroup>(result));
    const auto& group = std::get<RegionGroup>(result);

    EXPECT_FALSE(group.regions.empty());
}

TEST_F(EnergyAnalyzerTest, ThresholdConfiguration)
{
    analyzer->set_energy_thresholds(0.01, 0.05, 0.1, 0.5);
    analyzer->set_parameter("method", "rms");
    analyzer->set_output_granularity(AnalysisGranularity::ORGANIZED_GROUPS);

    AnalyzerInput input = container;
    auto result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<RegionGroup>(result));
}

TEST_F(EnergyAnalyzerTest, InvalidContainerThrows)
{
    std::shared_ptr<MockSignalSourceContainer> empty_container = std::make_shared<MockSignalSourceContainer>();
    empty_container->set_test_data({});
    AnalyzerInput input = empty_container;

    EXPECT_THROW(analyzer->apply_operation(input), std::invalid_argument);
}

TEST_F(EnergyAnalyzerTest, InvalidMethodThrows)
{
    analyzer->set_parameter("method", "not_a_method");
    AnalyzerInput input = container;

    EXPECT_THROW(analyzer->apply_operation(input), std::invalid_argument);
}

} // namespace MayaFlux::Test
