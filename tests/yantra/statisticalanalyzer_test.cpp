#include "../test_config.h"

#include "MayaFlux/Yantra/Analyzers/StatisticalAnalyzer.hpp"

using namespace MayaFlux::Yantra;
using namespace MayaFlux::Kakshya;

namespace MayaFlux::Test {

class StatisticalAnalyzerNewTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_data = { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0 };

        // analyzer = std::make_shared<StatisticalAnalyzer<std::vector<Kakshya::DataVariant>, Eigen::VectorXd>>(10, 5);
        analyzer = std::make_shared<StatisticalAnalyzer<std::vector<Kakshya::DataVariant>, Eigen::MatrixXd>>(10, 5);
    }

    std::vector<double> test_data;
    // std::shared_ptr<StatisticalAnalyzer<std::vector<Kakshya::DataVariant>, Eigen::VectorXd>> analyzer;
    std::shared_ptr<StatisticalAnalyzer<std::vector<Kakshya::DataVariant>, Eigen::MatrixXd>> analyzer;
};

TEST_F(StatisticalAnalyzerNewTest, BasicMean)
{
    analyzer->set_method("mean");

    std::vector<Kakshya::DataVariant> multi_channel_input = { Kakshya::DataVariant { test_data } };
    auto result = analyzer->analyze_data(multi_channel_input);
    auto stats = safe_any_cast_or_throw<StatisticalAnalysis>(result);

    for (const auto& channel : stats.channel_statistics) {
        EXPECT_FALSE(channel.statistical_values.empty());
        EXPECT_NEAR(channel.statistical_values[0], 5.5, 1e-10);
    }
}

TEST_F(StatisticalAnalyzerNewTest, BasicVariance)
{
    analyzer->set_method("variance");

    std::vector<Kakshya::DataVariant> multi_channel_input = { Kakshya::DataVariant { test_data } };
    auto result = analyzer->analyze_data(multi_channel_input);
    auto stats = safe_any_cast_or_throw<StatisticalAnalysis>(result);

    for (const auto& channel : stats.channel_statistics) {
        EXPECT_GT(channel.statistical_values[0], 0.0);
        EXPECT_NEAR(channel.statistical_values[0], 9.166667, 1e-5);
    }
}

TEST_F(StatisticalAnalyzerNewTest, BasicStdDev)
{
    analyzer->set_method("std_dev");

    std::vector<Kakshya::DataVariant> multi_channel_input = { Kakshya::DataVariant { test_data } };
    auto result = analyzer->analyze_data(multi_channel_input);
    auto stats = safe_any_cast_or_throw<StatisticalAnalysis>(result);

    for (const auto& channel : stats.channel_statistics) {
        EXPECT_NEAR(channel.statistical_values[0], std::sqrt(9.166667), 1e-5);
    }
}

TEST_F(StatisticalAnalyzerNewTest, MinMaxRange)
{
    std::vector<Kakshya::DataVariant> multi_channel_input = { Kakshya::DataVariant { test_data } };

    analyzer->set_method("min");
    auto min_result = analyzer->analyze_data(multi_channel_input);
    auto min_stats = safe_any_cast_or_throw<StatisticalAnalysis>(min_result);
    for (const auto& channel : min_stats.channel_statistics) {
        EXPECT_EQ(channel.statistical_values[0], 1.0);
    }

    analyzer->set_method("max");
    auto max_result = analyzer->analyze_data(multi_channel_input);
    auto max_stats = safe_any_cast_or_throw<StatisticalAnalysis>(max_result);
    for (const auto& channel : max_stats.channel_statistics) {
        EXPECT_EQ(channel.statistical_values[0], 10.0);
    }

    analyzer->set_method("range");
    auto range_result = analyzer->analyze_data(multi_channel_input);
    auto range_stats = safe_any_cast_or_throw<StatisticalAnalysis>(range_result);
    for (const auto& channel : range_stats.channel_statistics) {
        EXPECT_EQ(channel.statistical_values[0], 9.0);
    }
}

TEST_F(StatisticalAnalyzerNewTest, Median)
{
    analyzer->set_method("median");

    std::vector<Kakshya::DataVariant> multi_channel_input = { Kakshya::DataVariant { test_data } };
    auto result = analyzer->analyze_data(multi_channel_input);
    auto stats = safe_any_cast_or_throw<StatisticalAnalysis>(result);

    for (const auto& channel : stats.channel_statistics) {
        EXPECT_EQ(channel.statistical_values[0], 5.5);
    }
}

TEST_F(StatisticalAnalyzerNewTest, ParameterSetting)
{
    analyzer->set_method(StatisticalMethod::PERCENTILE);
    analyzer->set_parameter("percentile", 25.0);

    std::vector<Kakshya::DataVariant> multi_channel_input = { Kakshya::DataVariant { test_data } };
    auto result = analyzer->analyze_data(multi_channel_input);
    auto stats = safe_any_cast_or_throw<StatisticalAnalysis>(result);

    for (const auto& channel : stats.channel_statistics) {
        EXPECT_NEAR(channel.statistical_values[0], 3.25, 1e-10);
    }
}

TEST_F(StatisticalAnalyzerNewTest, WindowedAnalysis)
{
    std::vector<double> long_data(100);
    std::iota(long_data.begin(), long_data.end(), 1.0);

    analyzer->set_window_size(10);
    analyzer->set_hop_size(5);
    analyzer->set_method("mean");

    std::vector<Kakshya::DataVariant> multi_channel_input = { Kakshya::DataVariant { long_data } };
    auto result = analyzer->analyze_data(multi_channel_input);
    auto stats = safe_any_cast_or_throw<StatisticalAnalysis>(result);

    for (const auto& channel : stats.channel_statistics) {
        EXPECT_GT(channel.statistical_values.size(), 1);
    }
}

TEST_F(StatisticalAnalyzerNewTest, PipelineOutput)
{
    std::vector<Kakshya::DataVariant> multi_channel_input = { Kakshya::DataVariant { test_data } };
    IO<std::vector<Kakshya::DataVariant>> input { multi_channel_input };

    analyzer->set_method("mean");
    auto output = analyzer->apply_operation(input);

    EXPECT_GT(output.data.size(), 0);
}

TEST_F(StatisticalAnalyzerNewTest, AnalysisResultCompleteness)
{
    analyzer->set_method("mean");
    analyzer->set_classification_enabled(true);

    std::vector<Kakshya::DataVariant> multi_channel_input = { Kakshya::DataVariant { test_data } };
    auto result = analyzer->analyze_data(multi_channel_input);
    auto stats = safe_any_cast_or_throw<StatisticalAnalysis>(result);

    EXPECT_EQ(stats.method_used, StatisticalMethod::MEAN);
    EXPECT_FALSE(stats.channel_statistics.empty());

    for (const auto& channel : stats.channel_statistics) {
        EXPECT_FALSE(channel.statistical_values.empty());
        EXPECT_GT(channel.mean_stat, 0.0);
        EXPECT_GE(channel.max_stat, channel.min_stat);
        EXPECT_FALSE(channel.percentiles.empty());
    }
}

TEST_F(StatisticalAnalyzerNewTest, EmptyDataThrows)
{
    std::vector<Kakshya::DataVariant> empty_multi_channel;
    EXPECT_THROW(analyzer->analyze_data(empty_multi_channel), std::runtime_error);
}

TEST_F(StatisticalAnalyzerNewTest, InvalidMethodThrows)
{
    EXPECT_THROW(analyzer->set_method("invalid_method"), std::invalid_argument);
}

TEST_F(StatisticalAnalyzerNewTest, TypeSafeAccess)
{
    analyzer->set_method("mean");

    std::vector<Kakshya::DataVariant> multi_channel_input = { Kakshya::DataVariant { test_data } };
    auto stats = analyzer->analyze_statistics(multi_channel_input);

    EXPECT_EQ(stats.method_used, StatisticalMethod::MEAN);
    for (const auto& channel : stats.channel_statistics) {
        EXPECT_FALSE(channel.statistical_values.empty());
    }
}

TEST_F(StatisticalAnalyzerNewTest, DataVariantInput)
{
    std::vector<Kakshya::DataVariant> multi_channel_input = { Kakshya::DataVariant { test_data } };

    analyzer->set_method("mean");
    auto result = analyzer->analyze_data(multi_channel_input);
    auto stats = safe_any_cast_or_throw<StatisticalAnalysis>(result);

    for (const auto& channel : stats.channel_statistics) {
        EXPECT_NEAR(channel.statistical_values[0], 5.5, 1e-10);
    }
}

TEST_F(StatisticalAnalyzerNewTest, MultiChannelAnalysis)
{
    std::vector<double> channel1_data = { 1.0, 2.0, 3.0, 4.0, 5.0 };
    std::vector<double> channel2_data = { 10.0, 20.0, 30.0, 40.0, 50.0 };

    std::vector<Kakshya::DataVariant> multi_channel_input = {
        Kakshya::DataVariant { channel1_data },
        Kakshya::DataVariant { channel2_data }
    };

    analyzer->set_method("mean");

    analyzer->set_window_size(channel1_data.size());
    analyzer->set_hop_size(channel1_data.size());

    auto result = analyzer->analyze_data(multi_channel_input);
    auto stats = safe_any_cast_or_throw<StatisticalAnalysis>(result);

    EXPECT_EQ(stats.channel_statistics.size(), 2);

    EXPECT_NEAR(stats.channel_statistics[0].statistical_values[0], 3.0, 1e-10);
    EXPECT_NEAR(stats.channel_statistics[1].statistical_values[0], 30.0, 1e-10);

    EXPECT_NE(stats.channel_statistics[0].mean_stat, stats.channel_statistics[1].mean_stat);
}

TEST_F(StatisticalAnalyzerNewTest, ChannelStatisticsStructure)
{
    analyzer->set_method("mean");
    analyzer->set_classification_enabled(true);

    std::vector<Kakshya::DataVariant> multi_channel_input = { Kakshya::DataVariant { test_data } };
    auto result = analyzer->analyze_data(multi_channel_input);
    auto stats = safe_any_cast_or_throw<StatisticalAnalysis>(result);

    for (const auto& channel : stats.channel_statistics) {
        EXPECT_FALSE(channel.statistical_values.empty());
        EXPECT_GT(channel.mean_stat, 0.0);
        EXPECT_GE(channel.max_stat, channel.min_stat);
        EXPECT_GE(channel.stat_variance, 0.0);
        EXPECT_GE(channel.stat_std_dev, 0.0);
        EXPECT_EQ(channel.percentiles.size(), 3); // Q1, Q2, Q3
        EXPECT_FALSE(channel.window_positions.empty());

        bool has_classifications = false;
        for (int level_count : channel.level_counts) {
            if (level_count > 0) {
                has_classifications = true;
                break;
            }
        }
        EXPECT_TRUE(has_classifications);
    }
}

} // namespace MayaFlux::Test
