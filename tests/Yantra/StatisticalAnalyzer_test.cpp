#include "../mock_signalsourcecontainer.hpp"

#include "MayaFlux/Yantra/Analyzers/StatisticalAnalyzer.hpp"

using namespace MayaFlux::Yantra;
using namespace MayaFlux::Kakshya;

namespace MayaFlux::Test {

class StatisticalAnalyzerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_data = { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0 };

        std::normal_distribution<double> normal_dist(50.0, 10.0);
        std::default_random_engine generator(42); // Fixed seed for reproducibility
        normal_data.resize(1000);
        for (auto& val : normal_data) {
            val = normal_dist(generator);
        }

        skewed_data = { 1.0, 1.0, 1.0, 2.0, 2.0, 3.0, 4.0, 5.0, 10.0, 20.0 };

        container = std::make_shared<MockSignalSourceContainer>();
        container->set_test_data(test_data);

        analyzer = std::make_unique<StatisticalAnalyzer>();
    }

    std::vector<double> test_data;
    std::vector<double> normal_data;
    std::vector<double> skewed_data;
    std::shared_ptr<MockSignalSourceContainer> container;
    std::unique_ptr<StatisticalAnalyzer> analyzer;
};

// ===== Basic Statistical Methods Tests =====

TEST_F(StatisticalAnalyzerTest, CalculateMean)
{
    analyzer->set_parameter("method", "mean");
    analyzer->set_output_granularity(AnalysisGranularity::RAW_VALUES);

    AnalyzerInput input = container;
    auto result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& values = std::get<std::vector<double>>(result);

    ASSERT_EQ(values.size(), 1);
    EXPECT_NEAR(values[0], 5.5, 1e-10); // Mean of 1-10 is 5.5
}

TEST_F(StatisticalAnalyzerTest, CalculateVariance)
{
    analyzer->set_parameter("method", "variance");
    analyzer->set_parameter("sample_variance", true);
    analyzer->set_output_granularity(AnalysisGranularity::RAW_VALUES);

    AnalyzerInput input = container;
    auto result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& values = std::get<std::vector<double>>(result);

    ASSERT_EQ(values.size(), 1);
    // Sample variance of 1-10 is 9.166667
    EXPECT_NEAR(values[0], 9.166667, 1e-5);
}

TEST_F(StatisticalAnalyzerTest, CalculatePopulationVariance)
{
    analyzer->set_parameter("method", "variance");
    analyzer->set_parameter("sample_variance", false);
    analyzer->set_output_granularity(AnalysisGranularity::RAW_VALUES);

    AnalyzerInput input = container;
    auto result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& values = std::get<std::vector<double>>(result);

    ASSERT_EQ(values.size(), 1);
    // Population variance of 1-10 is 8.25
    EXPECT_NEAR(values[0], 8.25, 1e-10);
}

TEST_F(StatisticalAnalyzerTest, CalculateStandardDeviation)
{
    analyzer->set_parameter("method", "std_dev");
    analyzer->set_parameter("sample_variance", true);
    analyzer->set_output_granularity(AnalysisGranularity::RAW_VALUES);

    AnalyzerInput input = container;
    auto result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& values = std::get<std::vector<double>>(result);

    ASSERT_EQ(values.size(), 1);
    EXPECT_NEAR(values[0], std::sqrt(9.166667), 1e-5);
}

TEST_F(StatisticalAnalyzerTest, CalculateMinMax)
{
    analyzer->set_output_granularity(AnalysisGranularity::RAW_VALUES);

    // Test MIN
    analyzer->set_parameter("method", "min");
    AnalyzerInput input = container;
    auto min_result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(min_result));
    const auto& min_values = std::get<std::vector<double>>(min_result);
    ASSERT_EQ(min_values.size(), 1);
    EXPECT_EQ(min_values[0], 1.0);

    // Test MAX
    analyzer->set_parameter("method", "max");
    auto max_result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(max_result));
    const auto& max_values = std::get<std::vector<double>>(max_result);
    ASSERT_EQ(max_values.size(), 1);
    EXPECT_EQ(max_values[0], 10.0);
}

TEST_F(StatisticalAnalyzerTest, CalculateRange)
{
    analyzer->set_parameter("method", "range");
    analyzer->set_output_granularity(AnalysisGranularity::RAW_VALUES);

    AnalyzerInput input = container;
    auto result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& values = std::get<std::vector<double>>(result);

    ASSERT_EQ(values.size(), 1);
    EXPECT_EQ(values[0], 9.0); // 10 - 1 = 9
}

TEST_F(StatisticalAnalyzerTest, CalculateMedian)
{
    analyzer->set_parameter("method", "median");
    analyzer->set_output_granularity(AnalysisGranularity::RAW_VALUES);

    AnalyzerInput input = container;
    auto result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& values = std::get<std::vector<double>>(result);

    ASSERT_EQ(values.size(), 1);
    EXPECT_EQ(values[0], 5.5); // Median of 1-10 is 5.5
}

TEST_F(StatisticalAnalyzerTest, CalculateMedianOddSize)
{
    std::vector<double> odd_data = { 1.0, 3.0, 5.0, 7.0, 9.0 };
    container->set_test_data(odd_data);

    analyzer->set_parameter("method", "median");
    analyzer->set_output_granularity(AnalysisGranularity::RAW_VALUES);

    AnalyzerInput input = container;
    auto result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& values = std::get<std::vector<double>>(result);

    ASSERT_EQ(values.size(), 1);
    EXPECT_EQ(values[0], 5.0);
}

TEST_F(StatisticalAnalyzerTest, CalculatePercentile)
{
    analyzer->set_parameter("method", "percentile");
    analyzer->set_parameter("percentile", 25.0);
    analyzer->set_output_granularity(AnalysisGranularity::RAW_VALUES);

    AnalyzerInput input = container;
    auto result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& values = std::get<std::vector<double>>(result);

    ASSERT_EQ(values.size(), 1);
    EXPECT_NEAR(values[0], 3.25, 1e-10); // 25th percentile
}

TEST_F(StatisticalAnalyzerTest, CalculateMode)
{
    std::vector<double> mode_data = { 1.0, 2.0, 2.0, 3.0, 3.0, 3.0, 4.0 };
    container->set_test_data(mode_data);

    analyzer->set_parameter("method", "mode");
    analyzer->set_output_granularity(AnalysisGranularity::RAW_VALUES);

    AnalyzerInput input = container;
    auto result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& values = std::get<std::vector<double>>(result);

    ASSERT_EQ(values.size(), 1);
    EXPECT_EQ(values[0], 3.0); // Mode is 3 (appears 3 times)
}

TEST_F(StatisticalAnalyzerTest, CalculateSumAndCount)
{
    analyzer->set_output_granularity(AnalysisGranularity::RAW_VALUES);

    // Test SUM
    analyzer->set_parameter("method", "sum");
    AnalyzerInput input = container;
    auto sum_result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(sum_result));
    const auto& sum_values = std::get<std::vector<double>>(sum_result);
    ASSERT_EQ(sum_values.size(), 1);
    EXPECT_EQ(sum_values[0], 55.0); // Sum of 1-10

    // Test COUNT
    analyzer->set_parameter("method", "count");
    auto count_result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(count_result));
    const auto& count_values = std::get<std::vector<double>>(count_result);
    ASSERT_EQ(count_values.size(), 1);
    EXPECT_EQ(count_values[0], 10.0);
}

TEST_F(StatisticalAnalyzerTest, CalculateRMS)
{
    analyzer->set_parameter("method", "rms");
    analyzer->set_output_granularity(AnalysisGranularity::RAW_VALUES);

    AnalyzerInput input = container;
    auto result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& values = std::get<std::vector<double>>(result);

    ASSERT_EQ(values.size(), 1);
    // RMS = sqrt((1^2 + 2^2 + ... + 10^2) / 10) = sqrt(385/10) = sqrt(38.5)
    EXPECT_NEAR(values[0], std::sqrt(38.5), 1e-10);
}

// ===== Advanced Statistical Methods Tests =====

TEST_F(StatisticalAnalyzerTest, CalculateSkewness)
{
    container->set_test_data(skewed_data);

    analyzer->set_parameter("method", "skewness");
    analyzer->set_output_granularity(AnalysisGranularity::RAW_VALUES);

    AnalyzerInput input = container;
    auto result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& values = std::get<std::vector<double>>(result);

    ASSERT_EQ(values.size(), 1);
    EXPECT_GT(values[0], 1.0); // Should be positively skewed
}

TEST_F(StatisticalAnalyzerTest, CalculateKurtosis)
{
    container->set_test_data(normal_data);

    analyzer->set_parameter("method", "kurtosis");
    analyzer->set_output_granularity(AnalysisGranularity::RAW_VALUES);

    AnalyzerInput input = container;
    auto result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& values = std::get<std::vector<double>>(result);

    ASSERT_EQ(values.size(), 1);
    // Normal distribution kurtosis should be close to 0 (excess kurtosis)
    EXPECT_NEAR(values[0], 0.0, 0.5);
}

TEST_F(StatisticalAnalyzerTest, CalculateMAD)
{
    analyzer->set_parameter("method", "mad");
    analyzer->set_output_granularity(AnalysisGranularity::RAW_VALUES);

    AnalyzerInput input = container;
    auto result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& values = std::get<std::vector<double>>(result);

    ASSERT_EQ(values.size(), 1);
    EXPECT_GT(values[0], 0.0); // MAD should be positive
}

TEST_F(StatisticalAnalyzerTest, CalculateCoefficientOfVariation)
{
    analyzer->set_parameter("method", "cv");
    analyzer->set_output_granularity(AnalysisGranularity::RAW_VALUES);

    AnalyzerInput input = container;
    auto result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& values = std::get<std::vector<double>>(result);

    ASSERT_EQ(values.size(), 1);
    // CV = std_dev / mean
    double expected_cv = std::sqrt(9.166667) / 5.5;
    EXPECT_NEAR(values[0], expected_cv, 1e-5);
}

// ===== Edge Cases and Error Handling Tests =====

TEST_F(StatisticalAnalyzerTest, EmptyDataThrows)
{
    std::vector<double> empty_data;
    container->set_test_data(empty_data);

    analyzer->set_parameter("method", "mean");
    AnalyzerInput input = container;

    EXPECT_THROW(analyzer->apply_operation(input), std::invalid_argument);
}

TEST_F(StatisticalAnalyzerTest, InsufficientDataForVariance)
{
    std::vector<double> single_value = { 42.0 };
    container->set_test_data(single_value);

    analyzer->set_parameter("method", "variance");
    AnalyzerInput input = container;

    EXPECT_THROW(analyzer->apply_operation(input), std::invalid_argument);
}

TEST_F(StatisticalAnalyzerTest, InsufficientDataForSkewness)
{
    std::vector<double> two_values = { 1.0, 2.0 };
    container->set_test_data(two_values);

    analyzer->set_parameter("method", "skewness");
    AnalyzerInput input = container;

    EXPECT_THROW(analyzer->apply_operation(input), std::invalid_argument);
}

TEST_F(StatisticalAnalyzerTest, InsufficientDataForKurtosis)
{
    std::vector<double> three_values = { 1.0, 2.0, 3.0 };
    container->set_test_data(three_values);

    analyzer->set_parameter("method", "kurtosis");
    AnalyzerInput input = container;

    EXPECT_THROW(analyzer->apply_operation(input), std::invalid_argument);
}

TEST_F(StatisticalAnalyzerTest, HandleNaNValues)
{
    std::vector<double> nan_data = { 1.0, 2.0, std::numeric_limits<double>::quiet_NaN(), 4.0 };
    container->set_test_data(nan_data);

    analyzer->set_parameter("method", "mean");
    AnalyzerInput input = container;

    EXPECT_THROW(analyzer->apply_operation(input), std::invalid_argument);
}

TEST_F(StatisticalAnalyzerTest, HandleInfiniteValues)
{
    std::vector<double> inf_data = { 1.0, 2.0, std::numeric_limits<double>::infinity(), 4.0 };
    container->set_test_data(inf_data);

    analyzer->set_parameter("method", "mean");
    AnalyzerInput input = container;

    EXPECT_THROW(analyzer->apply_operation(input), std::invalid_argument);
}

TEST_F(StatisticalAnalyzerTest, InvalidMethodThrows)
{
    analyzer->set_parameter("method", "not_a_method");
    AnalyzerInput input = container;

    EXPECT_THROW(analyzer->apply_operation(input), std::invalid_argument);
}

TEST_F(StatisticalAnalyzerTest, CVWithZeroMeanThrows)
{
    std::vector<double> zero_mean_data = { -2.0, -1.0, 0.0, 1.0, 2.0 };
    container->set_test_data(zero_mean_data);

    analyzer->set_parameter("method", "cv");
    AnalyzerInput input = container;

    EXPECT_THROW(analyzer->apply_operation(input), std::runtime_error);
}

// ===== Output Granularity Tests =====

TEST_F(StatisticalAnalyzerTest, RawValuesGranularity)
{
    analyzer->set_parameter("method", "mean");
    analyzer->set_output_granularity(AnalysisGranularity::RAW_VALUES);

    AnalyzerInput input = container;
    auto result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
}

TEST_F(StatisticalAnalyzerTest, AttributedSegmentsGranularity)
{
    analyzer->set_parameter("method", "mean");
    analyzer->set_output_granularity(AnalysisGranularity::ATTRIBUTED_SEGMENTS);

    AnalyzerInput input = container;
    auto result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<RegionSegment>>(result));
    const auto& segments = std::get<std::vector<RegionSegment>>(result);

    EXPECT_FALSE(segments.empty());
}

TEST_F(StatisticalAnalyzerTest, OrganizedGroupsGranularity)
{
    analyzer->set_parameter("method", "mean");
    analyzer->set_output_granularity(AnalysisGranularity::ORGANIZED_GROUPS);

    AnalyzerInput input = container;
    auto result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<RegionGroup>(result));
    const auto& group = std::get<RegionGroup>(result);

    EXPECT_EQ(group.name, "Statistical Analysis - mean");
    EXPECT_FALSE(group.regions.empty());
}

// ===== Multi-Modal Data Tests =====

TEST_F(StatisticalAnalyzerTest, AnalyzeDataVariant)
{
    DataVariant data_variant = test_data;

    analyzer->set_parameter("method", "mean");
    analyzer->set_output_granularity(AnalysisGranularity::RAW_VALUES);

    AnalyzerInput input = data_variant;
    auto result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& values = std::get<std::vector<double>>(result);

    ASSERT_EQ(values.size(), 1);
    EXPECT_NEAR(values[0], 5.5, 1e-10);
}

TEST_F(StatisticalAnalyzerTest, AnalyzeFloatData)
{
    std::vector<float> float_data = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f };
    DataVariant data_variant = float_data;

    analyzer->set_parameter("method", "mean");
    analyzer->set_output_granularity(AnalysisGranularity::RAW_VALUES);

    AnalyzerInput input = data_variant;
    auto result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& values = std::get<std::vector<double>>(result);

    ASSERT_EQ(values.size(), 1);
    EXPECT_NEAR(values[0], 3.0, 1e-10);
}

TEST_F(StatisticalAnalyzerTest, AnalyzeRegion)
{
    Region test_region { { 0 }, { 10 }, {} };
    test_region.attributes["data"] = test_data;

    analyzer->set_parameter("method", "mean");
    analyzer->set_output_granularity(AnalysisGranularity::RAW_VALUES);

    AnalyzerInput input = test_region;
    auto result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
}

TEST_F(StatisticalAnalyzerTest, AnalyzeRegionGroup)
{
    RegionGroup group("test_group");
    Region r1 { { 0 }, { 5 }, {} };
    r1.attributes["data"] = std::vector<double> { 1.0, 2.0, 3.0, 4.0, 5.0 };
    Region r2 { { 5 }, { 10 }, {} };
    r2.attributes["data"] = std::vector<double> { 6.0, 7.0, 8.0, 9.0, 10.0 };

    group.add_region(r1);
    group.add_region(r2);

    analyzer->set_parameter("method", "mean");
    analyzer->set_output_granularity(AnalysisGranularity::RAW_VALUES);

    AnalyzerInput input = group;
    auto result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& values = std::get<std::vector<double>>(result);

    // Should calculate mean for each region
    ASSERT_EQ(values.size(), 2);
    EXPECT_NEAR(values[0], 3.0, 1e-10); // Mean of 1-5
    EXPECT_NEAR(values[1], 8.0, 1e-10); // Mean of 6-10
}

// ===== Multi-Dimensional Data Tests =====

TEST_F(StatisticalAnalyzerTest, Analyze2DSpectralData)
{
    // Create 2D spectral data (time x frequency)
    std::vector<double> spectral_data;
    size_t time_frames = 10;
    size_t freq_bins = 5;

    for (size_t t = 0; t < time_frames; ++t) {
        for (size_t f = 0; f < freq_bins; ++f) {
            spectral_data.push_back(static_cast<double>(t * freq_bins + f + 1));
        }
    }

    container->set_test_data(spectral_data);

    DataDimension time_dim("time", time_frames);
    time_dim.role = DataDimension::Role::TIME;
    container->add_dimension(time_dim);
    DataDimension freq_dim("frequency", freq_bins);
    freq_dim.role = DataDimension::Role::FREQUENCY;
    container->add_dimension(freq_dim);

    analyzer->set_parameter("method", "mean");
    analyzer->set_output_granularity(AnalysisGranularity::RAW_VALUES);

    AnalyzerInput input = container;
    auto result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& values = std::get<std::vector<double>>(result);

    size_t expected_freq_bins = 0;
    for (const auto& dim : container->get_dimensions()) {
        if (dim.role == DataDimension::Role::FREQUENCY) {
            expected_freq_bins = dim.size;
            break;
        }
    }
    ASSERT_GT(expected_freq_bins, 0u);

    // Should have one mean value per frequency bin
    EXPECT_EQ(values.size(), expected_freq_bins);
}

// ===== Parameter Configuration Tests =====

TEST_F(StatisticalAnalyzerTest, ParameterPersistence)
{
    analyzer->set_parameter("sample_variance", false);
    analyzer->set_parameter("percentile", 75.0);
    analyzer->set_parameter("precision", 1e-8);

    auto sample_var = analyzer->get_parameter("sample_variance");
    ASSERT_TRUE(sample_var.has_value());
    EXPECT_FALSE(std::any_cast<bool>(sample_var));

    auto percentile = analyzer->get_parameter("percentile");
    ASSERT_TRUE(percentile.has_value());
    EXPECT_EQ(std::any_cast<double>(percentile), 75.0);

    auto precision = analyzer->get_parameter("precision");
    ASSERT_TRUE(precision.has_value());
    EXPECT_EQ(std::any_cast<double>(precision), 1e-8);
}

TEST_F(StatisticalAnalyzerTest, GetAllParameters)
{
    analyzer->set_parameter("method", "mean");
    analyzer->set_parameter("sample_variance", false);

    auto all_params = analyzer->get_all_parameters();

    EXPECT_TRUE(all_params.find("method") != all_params.end());
    EXPECT_TRUE(all_params.find("sample_variance") != all_params.end());
}

// ===== Method Availability Tests =====

TEST_F(StatisticalAnalyzerTest, GetAvailableMethods)
{
    auto methods = analyzer->get_available_methods();

    std::vector<std::string> expected_methods = {
        "mean", "variance", "std_dev", "skewness", "kurtosis",
        "min", "max", "median", "range", "percentile",
        "mode", "mad", "cv", "sum", "count", "rms"
    };

    EXPECT_EQ(methods.size(), expected_methods.size());

    for (const auto& method : expected_methods) {
        EXPECT_TRUE(std::find(methods.begin(), methods.end(), method) != methods.end());
    }
}

TEST_F(StatisticalAnalyzerTest, GetMethodsForType)
{
    // All methods should be available for all types
    auto double_methods = analyzer->get_methods_for_type<std::vector<double>>();
    auto float_methods = analyzer->get_methods_for_type<std::vector<float>>();
    auto container_methods = analyzer->get_methods_for_type<std::shared_ptr<SignalSourceContainer>>();

    EXPECT_EQ(double_methods.size(), 16);
    EXPECT_EQ(float_methods.size(), 16);
    EXPECT_EQ(container_methods.size(), 16);
}

// ===== Performance Tests =====

TEST_F(StatisticalAnalyzerTest, LargeDatasetPerformance)
{
    std::vector<double> large_data(100000);
    std::iota(large_data.begin(), large_data.end(), 1.0);
    container->set_test_data(large_data);

    analyzer->set_parameter("method", "mean");
    analyzer->set_output_granularity(AnalysisGranularity::RAW_VALUES);

    auto start_time = std::chrono::high_resolution_clock::now();

    AnalyzerInput input = container;
    auto result = analyzer->apply_operation(input);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    EXPECT_LT(duration.count(), 100); // Should complete in less than 100ms
}

TEST_F(StatisticalAnalyzerTest, MultipleMethodsPerformance)
{
    std::vector<std::string> methods = { "mean", "variance", "std_dev", "skewness", "kurtosis" };

    for (const auto& method : methods) {
        analyzer->set_parameter("method", method);
        analyzer->set_output_granularity(AnalysisGranularity::RAW_VALUES);

        AnalyzerInput input = container;
        auto result = analyzer->apply_operation(input);

        ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    }
}

// ===== Thread Safety Tests =====

TEST_F(StatisticalAnalyzerTest, ConcurrentAnalysis)
{
    const size_t num_threads = 4;
    std::vector<std::thread> threads;
    std::vector<bool> thread_results(num_threads, false);

    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, t, &thread_results]() {
            try {
                auto local_analyzer = std::make_unique<StatisticalAnalyzer>();
                local_analyzer->set_parameter("method", "mean");
                local_analyzer->set_output_granularity(AnalysisGranularity::RAW_VALUES);

                AnalyzerInput input = container;
                auto result = local_analyzer->apply_operation(input);

                EXPECT_TRUE(std::holds_alternative<std::vector<double>>(result));
                const auto& values = std::get<std::vector<double>>(result);
                EXPECT_NEAR(values[0], 5.5, 1e-10);

                thread_results[t] = true;
            } catch (...) {
                thread_results[t] = false;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    for (bool result : thread_results) {
        EXPECT_TRUE(result);
    }
}

// ===== Special Cases Tests =====

TEST_F(StatisticalAnalyzerTest, AllSameValues)
{
    std::vector<double> same_values(100, 42.0);
    container->set_test_data(same_values);

    analyzer->set_parameter("method", "variance");
    analyzer->set_output_granularity(AnalysisGranularity::RAW_VALUES);

    AnalyzerInput input = container;
    auto result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& values = std::get<std::vector<double>>(result);

    ASSERT_EQ(values.size(), 1);
    EXPECT_EQ(values[0], 0.0); // Variance should be 0
}

TEST_F(StatisticalAnalyzerTest, AlternatingValues)
{
    std::vector<double> alternating;
    for (int i = 0; i < 100; ++i) {
        alternating.push_back(i % 2 == 0 ? 1.0 : -1.0);
    }
    container->set_test_data(alternating);

    analyzer->set_parameter("method", "mean");
    analyzer->set_output_granularity(AnalysisGranularity::RAW_VALUES);

    AnalyzerInput input = container;
    auto result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& values = std::get<std::vector<double>>(result);

    ASSERT_EQ(values.size(), 1);
    EXPECT_NEAR(values[0], 0.0, 1e-10); // Mean should be 0
}

TEST_F(StatisticalAnalyzerTest, ExtremePercentiles)
{
    analyzer->set_output_granularity(AnalysisGranularity::RAW_VALUES);

    // 0th percentile (minimum)
    analyzer->set_parameter("method", "percentile");
    analyzer->set_parameter("percentile", 0.0);
    AnalyzerInput input = container;
    auto min_result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(min_result));
    const auto& min_values = std::get<std::vector<double>>(min_result);
    EXPECT_EQ(min_values[0], 1.0);

    // 100th percentile (maximum)
    analyzer->set_parameter("percentile", 100.0);
    auto max_result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(max_result));
    const auto& max_values = std::get<std::vector<double>>(max_result);
    EXPECT_EQ(max_values[0], 10.0);
}

// ===== Integration Tests =====

TEST_F(StatisticalAnalyzerTest, ChainedStatisticalAnalysis)
{
    // Calculate multiple statistics in sequence
    std::vector<std::string> methods = { "mean", "std_dev", "skewness", "kurtosis" };
    std::vector<double> results;

    analyzer->set_output_granularity(AnalysisGranularity::RAW_VALUES);

    for (const auto& method : methods) {
        analyzer->set_parameter("method", method);
        AnalyzerInput input = container;
        auto result = analyzer->apply_operation(input);

        ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
        const auto& values = std::get<std::vector<double>>(result);
        results.push_back(values[0]);
    }

    EXPECT_EQ(results.size(), 4);
    EXPECT_NEAR(results[0], 5.5, 1e-10); // mean
    EXPECT_GT(results[1], 0.0); // std_dev should be positive
}

TEST_F(StatisticalAnalyzerTest, CompleteStatisticalProfile)
{
    // Create a complete statistical profile of the data
    std::unordered_map<std::string, double> profile;

    std::vector<std::string> methods = {
        "mean", "variance", "std_dev", "min", "max", "median",
        "range", "sum", "count", "rms", "cv", "mad"
    };

    analyzer->set_output_granularity(AnalysisGranularity::RAW_VALUES);

    for (const auto& method : methods) {
        analyzer->set_parameter("method", method);
        AnalyzerInput input = container;
        auto result = analyzer->apply_operation(input);

        ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
        const auto& values = std::get<std::vector<double>>(result);
        profile[method] = values[0];
    }

    // Verify relationships between statistics
    EXPECT_NEAR(profile["variance"], profile["std_dev"] * profile["std_dev"], 1e-10);
    EXPECT_EQ(profile["range"], profile["max"] - profile["min"]);
    EXPECT_NEAR(profile["mean"], profile["sum"] / profile["count"], 1e-10);
}

// ===== Robustness Tests =====

TEST_F(StatisticalAnalyzerTest, VeryLargeValues)
{
    std::vector<double> large_values = { 1e15, 2e15, 3e15, 4e15, 5e15 };
    container->set_test_data(large_values);

    analyzer->set_parameter("method", "mean");
    analyzer->set_output_granularity(AnalysisGranularity::RAW_VALUES);

    AnalyzerInput input = container;
    auto result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& values = std::get<std::vector<double>>(result);

    EXPECT_NEAR(values[0], 3e15, 1e5); // Allow for floating point precision
}

TEST_F(StatisticalAnalyzerTest, VerySmallValues)
{
    std::vector<double> small_values = { 1e-15, 2e-15, 3e-15, 4e-15, 5e-15 };
    container->set_test_data(small_values);

    analyzer->set_parameter("method", "mean");
    analyzer->set_output_granularity(AnalysisGranularity::RAW_VALUES);

    AnalyzerInput input = container;
    auto result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& values = std::get<std::vector<double>>(result);

    EXPECT_NEAR(values[0], 3e-15, 1e-25);
}

TEST_F(StatisticalAnalyzerTest, MixedSignValues)
{
    std::vector<double> mixed_values = { -100.0, -50.0, 0.0, 50.0, 100.0 };
    container->set_test_data(mixed_values);

    analyzer->set_parameter("method", "mean");
    analyzer->set_output_granularity(AnalysisGranularity::RAW_VALUES);

    AnalyzerInput input = container;
    auto result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& values = std::get<std::vector<double>>(result);

    EXPECT_NEAR(values[0], 0.0, 1e-10);
}

// ===== Statistical Values Access Tests =====

TEST_F(StatisticalAnalyzerTest, GetStatisticalValuesConvenience)
{
    AnalyzerInput input = container;

    // Test convenience method
    auto mean_values = analyzer->get_statistical_values(input, StatisticalAnalyzer::Method::MEAN);
    ASSERT_EQ(mean_values.size(), 1);
    EXPECT_NEAR(mean_values[0], 5.5, 1e-10);

    // Test with different methods
    auto var_values = analyzer->get_statistical_values(input, StatisticalAnalyzer::Method::VARIANCE);
    ASSERT_EQ(var_values.size(), 1);
    EXPECT_GT(var_values[0], 0.0);
}

// ===== Method String Conversion Tests =====

TEST_F(StatisticalAnalyzerTest, MethodStringConversion)
{
    // Test all method string conversions
    std::vector<std::pair<std::string, StatisticalAnalyzer::Method>> method_pairs = {
        { "mean", StatisticalAnalyzer::Method::MEAN },
        { "variance", StatisticalAnalyzer::Method::VARIANCE },
        { "std_dev", StatisticalAnalyzer::Method::STD_DEV },
        { "skewness", StatisticalAnalyzer::Method::SKEWNESS },
        { "kurtosis", StatisticalAnalyzer::Method::KURTOSIS },
        { "min", StatisticalAnalyzer::Method::MIN },
        { "max", StatisticalAnalyzer::Method::MAX },
        { "median", StatisticalAnalyzer::Method::MEDIAN },
        { "range", StatisticalAnalyzer::Method::RANGE },
        { "percentile", StatisticalAnalyzer::Method::PERCENTILE },
        { "mode", StatisticalAnalyzer::Method::MODE },
        { "mad", StatisticalAnalyzer::Method::MAD },
        { "cv", StatisticalAnalyzer::Method::CV },
        { "sum", StatisticalAnalyzer::Method::SUM },
        { "count", StatisticalAnalyzer::Method::COUNT },
        { "rms", StatisticalAnalyzer::Method::RMS }
    };

    for (const auto& [method_str, method_enum] : method_pairs) {
        EXPECT_EQ(analyzer->method_to_string(method_enum), method_str);
    }
}

// ===== Region-based Statistical Analysis Tests =====

TEST_F(StatisticalAnalyzerTest, RegionAttributesExtraction)
{
    Region region { { 0 }, { 10 }, {} };
    region.attributes["data"] = test_data;
    region.attributes["label"] = std::string("test_region");

    analyzer->set_parameter("method", "mean");
    analyzer->set_output_granularity(AnalysisGranularity::ORGANIZED_GROUPS);

    AnalyzerInput input = region;
    auto result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<RegionGroup>(result));
    const auto& group = std::get<RegionGroup>(result);

    EXPECT_FALSE(group.regions.empty());
    EXPECT_TRUE(group.attributes.find("description") != group.attributes.end());
}

TEST_F(StatisticalAnalyzerTest, MultiRegionStatistics)
{
    RegionGroup group("multi_region_test");

    // Create regions with different statistical properties
    Region r1 { { 0 }, { 5 }, {} };
    r1.attributes["data"] = std::vector<double> { 1.0, 1.0, 1.0, 1.0, 1.0 }; // Constant

    Region r2 { { 5 }, { 10 }, {} };
    r2.attributes["data"] = std::vector<double> { 1.0, 2.0, 3.0, 4.0, 5.0 }; // Linear

    Region r3 { { 10 }, { 15 }, {} };
    r3.attributes["data"] = std::vector<double> { 5.0, 1.0, 4.0, 2.0, 3.0 }; // Random

    group.add_region(r1);
    group.add_region(r2);
    group.add_region(r3);

    analyzer->set_parameter("method", "variance");
    analyzer->set_output_granularity(AnalysisGranularity::RAW_VALUES);

    AnalyzerInput input = group;
    auto result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& values = std::get<std::vector<double>>(result);

    ASSERT_EQ(values.size(), 3);
    EXPECT_EQ(values[0], 0.0); // Constant data has 0 variance
    EXPECT_GT(values[1], 0.0); // Linear data has positive variance
    EXPECT_GT(values[2], 0.0); // Random data has positive variance
}

// ===== Custom Statistical Computations Tests =====

TEST_F(StatisticalAnalyzerTest, HighOrderMoments)
{
    // Test with data that has known skewness and kurtosis
    std::vector<double> asymmetric_data;

    // Create right-skewed distribution
    for (int i = 0; i < 90; ++i) {
        asymmetric_data.push_back(1.0);
    }
    for (int i = 0; i < 10; ++i) {
        asymmetric_data.push_back(10.0);
    }

    container->set_test_data(asymmetric_data);

    analyzer->set_output_granularity(AnalysisGranularity::RAW_VALUES);

    // Check skewness
    analyzer->set_parameter("method", "skewness");
    AnalyzerInput input = container;
    auto skew_result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(skew_result));
    const auto& skew_values = std::get<std::vector<double>>(skew_result);
    EXPECT_GT(skew_values[0], 0.0); // Should be positively skewed

    // Check kurtosis
    analyzer->set_parameter("method", "kurtosis");
    auto kurt_result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(kurt_result));
    const auto& kurt_values = std::get<std::vector<double>>(kurt_result);
    EXPECT_GT(kurt_values[0], 0.0); // Should have positive excess kurtosis
}

TEST_F(StatisticalAnalyzerTest, RobustStatistics)
{
    // Test MAD (Median Absolute Deviation) - robust to outliers
    std::vector<double> outlier_data = { 1.0, 2.0, 3.0, 4.0, 5.0, 100.0 }; // 100 is an outlier
    container->set_test_data(outlier_data);

    analyzer->set_output_granularity(AnalysisGranularity::RAW_VALUES);

    // Compare mean vs median
    analyzer->set_parameter("method", "mean");
    AnalyzerInput input = container;
    auto mean_result = analyzer->apply_operation(input);

    analyzer->set_parameter("method", "median");
    auto median_result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(mean_result));
    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(median_result));

    const auto& mean_values = std::get<std::vector<double>>(mean_result);
    const auto& median_values = std::get<std::vector<double>>(median_result);

    // Mean should be heavily influenced by outlier
    EXPECT_GT(mean_values[0], 15.0);
    // Median should be robust to outlier
    EXPECT_NEAR(median_values[0], 3.5, 1e-10);
}

} // namespace MayaFlux::Test
