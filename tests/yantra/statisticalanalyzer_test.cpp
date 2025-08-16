#include "../mock_signalsourcecontainer.hpp"
#include "MayaFlux/Yantra/Analyzers/StatisticalAnalyzer.hpp"

using namespace MayaFlux::Yantra;
using namespace MayaFlux::Kakshya;

namespace MayaFlux::Test {

class StatisticalAnalyzerNewTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_data = { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0 };
        analyzer = std::make_shared<StandardStatisticalAnalyzer>(10, 5);
    }

    std::vector<double> test_data;
    std::shared_ptr<StandardStatisticalAnalyzer> analyzer;
};

TEST_F(StatisticalAnalyzerNewTest, BasicMean)
{
    analyzer->set_method("mean");
    auto result = analyzer->analyze_data(test_data);
    auto stats = safe_any_cast_or_throw<StatisticalAnalysisResult>(result);

    EXPECT_NEAR(stats.statistical_values[0], 5.5, 1e-10);
}

TEST_F(StatisticalAnalyzerNewTest, BasicVariance)
{
    analyzer->set_method("variance");
    auto result = analyzer->analyze_data(test_data);
    auto stats = safe_any_cast_or_throw<StatisticalAnalysisResult>(result);

    EXPECT_GT(stats.statistical_values[0], 0.0);
    EXPECT_NEAR(stats.statistical_values[0], 9.166667, 1e-5);
}

TEST_F(StatisticalAnalyzerNewTest, BasicStdDev)
{
    analyzer->set_method("std_dev");
    auto result = analyzer->analyze_data(test_data);
    auto stats = safe_any_cast_or_throw<StatisticalAnalysisResult>(result);

    EXPECT_NEAR(stats.statistical_values[0], std::sqrt(9.166667), 1e-5);
}

TEST_F(StatisticalAnalyzerNewTest, MinMaxRange)
{
    analyzer->set_method("min");
    auto min_result = analyzer->analyze_data(test_data);
    auto min_stats = safe_any_cast_or_throw<StatisticalAnalysisResult>(min_result);
    EXPECT_EQ(min_stats.statistical_values[0], 1.0);

    analyzer->set_method("max");
    auto max_result = analyzer->analyze_data(test_data);
    auto max_stats = safe_any_cast_or_throw<StatisticalAnalysisResult>(max_result);
    EXPECT_EQ(max_stats.statistical_values[0], 10.0);

    analyzer->set_method("range");
    auto range_result = analyzer->analyze_data(test_data);
    auto range_stats = safe_any_cast_or_throw<StatisticalAnalysisResult>(range_result);
    EXPECT_EQ(range_stats.statistical_values[0], 9.0);
}

TEST_F(StatisticalAnalyzerNewTest, Median)
{
    analyzer->set_method("median");
    auto result = analyzer->analyze_data(test_data);
    auto stats = safe_any_cast_or_throw<StatisticalAnalysisResult>(result);

    EXPECT_EQ(stats.statistical_values[0], 5.5);
}

TEST_F(StatisticalAnalyzerNewTest, ParameterSetting)
{
    analyzer->set_method(StatisticalMethod::PERCENTILE);
    analyzer->set_parameter("percentile", 25.0);

    auto result = analyzer->analyze_data(test_data);
    auto stats = safe_any_cast_or_throw<StatisticalAnalysisResult>(result);

    EXPECT_NEAR(stats.statistical_values[0], 3.25, 1e-10);
}

TEST_F(StatisticalAnalyzerNewTest, WindowedAnalysis)
{
    std::vector<double> long_data(100);
    std::iota(long_data.begin(), long_data.end(), 1.0);

    analyzer->set_window_size(10);
    analyzer->set_hop_size(5);
    analyzer->set_method("mean");

    auto result = analyzer->analyze_data(long_data);
    auto stats = safe_any_cast_or_throw<StatisticalAnalysisResult>(result);

    EXPECT_GT(stats.statistical_values.size(), 1);
}

TEST_F(StatisticalAnalyzerNewTest, PipelineOutput)
{
    IO<Kakshya::DataVariant> input { test_data };

    analyzer->set_method("mean");
    auto output = analyzer->apply_operation(input);

    EXPECT_EQ(output.data.size(), 1);
}

TEST_F(StatisticalAnalyzerNewTest, AnalysisResultCompleteness)
{
    analyzer->set_method("mean");
    analyzer->set_classification_enabled(true);

    auto result = analyzer->analyze_data(test_data);
    auto stats = safe_any_cast_or_throw<StatisticalAnalysisResult>(result);

    EXPECT_EQ(stats.method_used, StatisticalMethod::MEAN);
    EXPECT_FALSE(stats.statistical_values.empty());
    EXPECT_GT(stats.mean_stat, 0.0);
    EXPECT_GE(stats.max_stat, stats.min_stat);
    EXPECT_FALSE(stats.percentiles.empty());
}

TEST_F(StatisticalAnalyzerNewTest, EmptyDataThrows)
{
    std::vector<double> empty_data;
    EXPECT_THROW(analyzer->analyze_data(empty_data), std::runtime_error);
}

TEST_F(StatisticalAnalyzerNewTest, InvalidMethodThrows)
{
    EXPECT_THROW(analyzer->set_method("invalid_method"), std::invalid_argument);
}

TEST_F(StatisticalAnalyzerNewTest, TypeSafeAccess)
{
    analyzer->set_method("mean");
    auto stats = analyzer->analyze_statistics(test_data);

    EXPECT_EQ(stats.method_used, StatisticalMethod::MEAN);
    EXPECT_FALSE(stats.statistical_values.empty());
}

TEST_F(StatisticalAnalyzerNewTest, DataVariantInput)
{
    Kakshya::DataVariant variant_data = test_data;

    analyzer->set_method("mean");
    auto result = analyzer->analyze_data(variant_data);
    auto stats = safe_any_cast_or_throw<StatisticalAnalysisResult>(result);

    EXPECT_NEAR(stats.statistical_values[0], 5.5, 1e-10);
}

} // namespace MayaFlux::Test
