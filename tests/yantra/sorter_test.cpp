#include "../mock_signalsourcecontainer.hpp"
#include "MayaFlux/Yantra/Sorters/SortingHelper.hpp"
#include "MayaFlux/Yantra/Sorters/StandardSorter.hpp"

#include <random>

using namespace MayaFlux::Yantra;
using namespace MayaFlux::Kakshya;

namespace MayaFlux::Test {

template <ComputeData InputType = std::vector<Kakshya::DataVariant>, ComputeData OutputType = InputType>
class TestStandardSorter : public StandardSorter<InputType, OutputType> {
public:
    using base_type = StandardSorter<InputType, OutputType>;
    using input_type = typename base_type::input_type;
    using output_type = typename base_type::output_type;

    TestStandardSorter()
    {
        this->set_direction(SortingDirection::ASCENDING);
        this->set_strategy(SortingStrategy::COPY_SORT);
        this->set_granularity(SortingGranularity::RAW_DATA);
    }

    [[nodiscard]] SortingType get_sorting_type() const override
    {
        return SortingType::STANDARD;
    }

protected:
    [[nodiscard]] std::string get_sorter_name() const override
    {
        return "TestStandardSorter";
    }
};

class ModernSorterTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_data = { 5.0, 2.0, 8.0, 1.0, 9.0, 3.0, 7.0, 4.0, 6.0, 0.0 };
        sorted_ascending = { 0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0 };
        sorted_descending = { 9.0, 8.0, 7.0, 6.0, 5.0, 4.0, 3.0, 2.0, 1.0, 0.0 };

        multi_channel_data = {
            Kakshya::DataVariant { std::vector<double> { 5.0, 2.0, 8.0, 1.0, 9.0 } },
            Kakshya::DataVariant { std::vector<double> { 3.0, 7.0, 4.0, 6.0, 0.0 } }
        };

        sorted_multi_channel = {
            Kakshya::DataVariant { std::vector<double> { 1.0, 2.0, 5.0, 8.0, 9.0 } },
            Kakshya::DataVariant { std::vector<double> { 0.0, 3.0, 4.0, 6.0, 7.0 } }
        };

        float_data = { 5.0, 2.0, 8.0, 1.0, 9.0 };

        container = std::make_shared<MockSignalSourceContainer>();
        container->set_test_data(test_data);

        data_sorter = std::make_shared<TestStandardSorter<std::vector<Kakshya::DataVariant>>>();
        eigen_sorter = std::make_shared<TestStandardSorter<Eigen::MatrixXd>>();

        test_group.name = "test_group";
        Region r1 { { 0 }, { 5 }, {} };
        Region r2 { { 5 }, { 10 }, {} };
        test_group.add_region(r1);
        test_group.add_region(r2);
    }

    void TearDown() override
    {
        container.reset();
        data_sorter.reset();
        eigen_sorter.reset();
    }

    std::vector<double> test_data;
    std::vector<double> sorted_ascending;
    std::vector<double> sorted_descending;
    std::vector<Kakshya::DataVariant> multi_channel_data;
    std::vector<Kakshya::DataVariant> sorted_multi_channel;

    std::shared_ptr<TestStandardSorter<std::vector<Kakshya::DataVariant>>> data_sorter;
    std::shared_ptr<TestStandardSorter<Eigen::MatrixXd>> eigen_sorter;
    std::vector<float> float_data;

    std::shared_ptr<MockSignalSourceContainer> container;
    RegionGroup test_group;
};

TEST_F(ModernSorterTest, BasicParameterSetGet)
{
    data_sorter->set_parameter("direction", SortingDirection::DESCENDING);
    auto direction = data_sorter->get_parameter("direction");
    auto direction_result = safe_any_cast<SortingDirection>(direction);
    ASSERT_TRUE(direction_result.value.has_value());
    EXPECT_EQ(*direction_result.value, SortingDirection::DESCENDING);

    data_sorter->set_parameter("strategy", std::string("in_place"));
    auto strategy = data_sorter->get_parameter("strategy");
    auto strategy_result = safe_any_cast<SortingStrategy>(strategy);
    ASSERT_TRUE(strategy_result.value.has_value());
    EXPECT_EQ(*strategy_result.value, SortingStrategy::IN_PLACE);

    data_sorter->set_parameter("granularity", std::string("raw_data"));
    auto granularity = data_sorter->get_parameter("granularity");
    auto granularity_result = safe_any_cast<SortingGranularity>(granularity);
    ASSERT_TRUE(granularity_result.value.has_value());
    EXPECT_EQ(*granularity_result.value, SortingGranularity::RAW_DATA);
}

TEST_F(ModernSorterTest, ParameterOrDefault)
{
    data_sorter->set_parameter("chunk_size", size_t(512));
    auto chunk_size = data_sorter->get_parameter_or_default<size_t>("chunk_size", 1024);
    EXPECT_EQ(chunk_size, 512);

    auto non_existent = data_sorter->get_parameter_or_default<int>("non_existent", 42);
    EXPECT_EQ(non_existent, 42);
}

TEST_F(ModernSorterTest, GetAllParameters)
{
    data_sorter->set_parameter("direction", SortingDirection::ASCENDING);
    data_sorter->set_parameter("strategy", SortingStrategy::COPY_SORT);
    data_sorter->set_parameter("test_param", std::string("test_value"));

    auto all_params = data_sorter->get_all_parameters();

    EXPECT_TRUE(all_params.find("direction") != all_params.end());
    EXPECT_TRUE(all_params.find("strategy") != all_params.end());
    EXPECT_TRUE(all_params.find("granularity") != all_params.end());
    EXPECT_TRUE(all_params.find("test_param") != all_params.end());
}

TEST_F(ModernSorterTest, SorterProperties)
{
    EXPECT_EQ(data_sorter->get_sorting_type(), SortingType::STANDARD);
    EXPECT_EQ(data_sorter->get_name(), "TestStandardSorter");

    EXPECT_EQ(data_sorter->get_direction(), SortingDirection::ASCENDING);
    EXPECT_EQ(data_sorter->get_strategy(), SortingStrategy::COPY_SORT);
    EXPECT_EQ(data_sorter->get_granularity(), SortingGranularity::RAW_DATA);
}

TEST_F(ModernSorterTest, BasicMultiChannelSorting)
{
    IO<std::vector<Kakshya::DataVariant>> input { multi_channel_data };

    data_sorter->set_direction(SortingDirection::ASCENDING);
    auto result = data_sorter->apply_operation(input);

    ASSERT_EQ(result.data.size(), 2);

    const auto& channel1 = std::get<std::vector<double>>(result.data[0]);
    const auto& channel2 = std::get<std::vector<double>>(result.data[1]);

    EXPECT_TRUE(std::ranges::is_sorted(channel1));
    EXPECT_TRUE(std::ranges::is_sorted(channel2));

    EXPECT_EQ(channel1, std::get<std::vector<double>>(sorted_multi_channel[0]));
    EXPECT_EQ(channel2, std::get<std::vector<double>>(sorted_multi_channel[1]));
}

TEST_F(ModernSorterTest, BasicSingleChannelSorting)
{
    std::vector<Kakshya::DataVariant> single_channel = { Kakshya::DataVariant { test_data } };
    IO<std::vector<Kakshya::DataVariant>> input { single_channel };

    data_sorter->set_direction(SortingDirection::ASCENDING);
    auto result = data_sorter->apply_operation(input);

    ASSERT_EQ(result.data.size(), 1);
    const auto& sorted_channel = std::get<std::vector<double>>(result.data[0]);
    EXPECT_EQ(sorted_channel, sorted_ascending);

    data_sorter->set_direction(SortingDirection::DESCENDING);
    result = data_sorter->apply_operation(input);

    const auto& des_sorted_channel = std::get<std::vector<double>>(result.data[0]);
    EXPECT_EQ(des_sorted_channel, sorted_descending);
}

TEST_F(ModernSorterTest, RegionGroupSortingWithContainer)
{
    auto region_sorter = std::make_shared<TestStandardSorter<RegionGroup>>();
    IO<RegionGroup> input { test_group, container };

    EXPECT_NO_THROW(region_sorter->apply_operation(input));

    auto result = region_sorter->apply_operation(input);
    // TODO:: Update mock container to support region data retrieval
    // EXPECT_EQ(result.data.regions.size(), test_group.regions.size());
    EXPECT_TRUE(result.has_container());
}

TEST_F(ModernSorterTest, HelperFunctionMultiChannel)
{
    auto sorted_data = sort_compute_data_extract(multi_channel_data, SortingDirection::ASCENDING, SortingAlgorithm::STANDARD);

    ASSERT_EQ(sorted_data.size(), 2);

    const auto& channel1 = std::get<std::vector<double>>(sorted_data[0]);
    const auto& channel2 = std::get<std::vector<double>>(sorted_data[1]);

    EXPECT_TRUE(std::ranges::is_sorted(channel1));
    EXPECT_TRUE(std::ranges::is_sorted(channel2));
}

TEST_F(ModernSorterTest, GenerateIndicesMultiChannel)
{
    IO<std::vector<Kakshya::DataVariant>> input { multi_channel_data, container };
    auto indices = generate_compute_data_indices(input, SortingDirection::ASCENDING);

    EXPECT_EQ(indices.size(), 2);
    EXPECT_EQ(indices[0].size(), 5);
    EXPECT_EQ(indices[1].size(), 5);

    const auto& original_ch1 = std::get<std::vector<double>>(multi_channel_data[0]);
    const auto& original_ch2 = std::get<std::vector<double>>(multi_channel_data[1]);

    std::vector<double> sorted_by_indices_ch1, sorted_by_indices_ch2;
    for (size_t idx : indices[0]) {
        sorted_by_indices_ch1.push_back(original_ch1[idx]);
    }
    for (size_t idx : indices[1]) {
        sorted_by_indices_ch2.push_back(original_ch2[idx]);
    }

    EXPECT_TRUE(std::ranges::is_sorted(sorted_by_indices_ch1));
    EXPECT_TRUE(std::ranges::is_sorted(sorted_by_indices_ch2));
}

TEST_F(ModernSorterTest, MixedDataTypes)
{
    std::vector<Kakshya::DataVariant> mixed_data = {
        Kakshya::DataVariant { std::vector<double> { 3.0, 1.0, 2.0 } },
        Kakshya::DataVariant { std::vector<float> { 6.0F, 4.0F, 5.0F } }
    };

    IO<std::vector<Kakshya::DataVariant>> input { mixed_data };
    auto result = data_sorter->apply_operation(input);

    ASSERT_EQ(result.data.size(), 2);

    const auto& double_channel = std::get<std::vector<double>>(result.data[0]);
    // TODO: Fix reconversion
    // const auto& float_channel = std::get<std::vector<float>>(result.data[1]);
    const auto& float_channel = std::get<std::vector<double>>(result.data[1]);

    EXPECT_TRUE(std::ranges::is_sorted(double_channel));
    EXPECT_TRUE(std::ranges::is_sorted(float_channel));
}

TEST_F(ModernSorterTest, EmptyMultiChannelData)
{
    std::vector<Kakshya::DataVariant> empty_multi_channel;
    IO<std::vector<Kakshya::DataVariant>> input { empty_multi_channel };

    EXPECT_NO_THROW(data_sorter->apply_operation(input));

    auto result = data_sorter->apply_operation(input);
    EXPECT_TRUE(result.data.empty());
}

TEST_F(ModernSorterTest, ContainerPreservation)
{
    auto region_sorter = std::make_shared<TestStandardSorter<RegionGroup>>();
    IO<RegionGroup> input { test_group, container };

    auto result = region_sorter->apply_operation(input);

    EXPECT_TRUE(result.has_container());
    EXPECT_EQ(result.container.value(), container);
}

TEST_F(ModernSorterTest, EigenMatrixSorting)
{
    Eigen::MatrixXd test_matrix(2, 3);
    test_matrix << 3.0, 1.0, 2.0,
        6.0, 4.0, 5.0;

    IO<Eigen::MatrixXd> input { test_matrix };
    auto result = eigen_sorter->apply_operation(input);

    for (int col = 0; col < result.data.cols(); ++col) {
        for (int row = 1; row < result.data.rows(); ++row) {
            EXPECT_LE(result.data(row - 1, col), result.data(row, col));
        }
    }
}

TEST_F(ModernSorterTest, IndexOnlySorting)
{
    IO<std::vector<Kakshya::DataVariant>> input { multi_channel_data };

    auto indices = generate_compute_data_indices(input, SortingDirection::ASCENDING);

    ASSERT_EQ(indices.size(), multi_channel_data.size());
    for (size_t ch = 0; ch < indices.size(); ++ch) {
        auto channels = OperationHelper::extract_numeric_data(multi_channel_data);
        ASSERT_EQ(indices[ch].size(), channels[ch].size());

        std::vector<double> sorted_by_indices;
        sorted_by_indices.reserve(indices[ch].size());
        for (size_t idx : indices[ch]) {
            sorted_by_indices.push_back(channels[ch][idx]);
        }
        EXPECT_TRUE(std::ranges::is_sorted(sorted_by_indices));
    }
}

TEST_F(ModernSorterTest, HelperFunctionInPlace)
{
    std::vector<Kakshya::DataVariant> data_copy = { Kakshya::DataVariant { test_data } };
    IO<std::vector<Kakshya::DataVariant>> input { data_copy };

    sort_compute_data_inplace(input, SortingDirection::ASCENDING, SortingAlgorithm::STANDARD);

    const auto& sorted = std::get<std::vector<double>>(input.data[0]);
    EXPECT_EQ(sorted, sorted_ascending);
    EXPECT_TRUE(std::ranges::is_sorted(sorted));
}

TEST_F(ModernSorterTest, HelperFunctionExtract)
{
    std::vector<Kakshya::DataVariant> data_vec = { Kakshya::DataVariant { test_data } };
    auto sorted_data = sort_compute_data_extract(data_vec, SortingDirection::ASCENDING, SortingAlgorithm::STANDARD);

    const auto& sorted = std::get<std::vector<double>>(sorted_data[0]);
    EXPECT_EQ(sorted, sorted_ascending);
    EXPECT_TRUE(std::ranges::is_sorted(sorted));
}

TEST_F(ModernSorterTest, GenerateIndices)
{
    std::vector<Kakshya::DataVariant> single_channel = { Kakshya::DataVariant { test_data } };
    IO<std::vector<Kakshya::DataVariant>> input { single_channel };
    auto indices = generate_compute_data_indices(input, SortingDirection::ASCENDING);

    ASSERT_EQ(indices.size(), 1);
    EXPECT_EQ(indices[0].size(), test_data.size());

    std::vector<double> sorted_by_indices;
    sorted_by_indices.reserve(indices[0].size());

    for (size_t idx : indices[0]) {
        sorted_by_indices.push_back(test_data[idx]);
    }
    EXPECT_EQ(sorted_by_indices, sorted_ascending);
}

TEST_F(ModernSorterTest, DifferentAlgorithms)
{
    std::vector<Kakshya::DataVariant> single_channel = { Kakshya::DataVariant { test_data } };
    IO<std::vector<Kakshya::DataVariant>> input { single_channel };

    std::vector<SortingAlgorithm> algorithms = {
        SortingAlgorithm::STANDARD,
        SortingAlgorithm::STABLE,
        SortingAlgorithm::HEAP
    };

    for (auto algorithm : algorithms) {
        data_sorter->set_algorithm(algorithm);
        auto result = data_sorter->apply_operation(input);

        const auto& sorted = std::get<std::vector<double>>(result.data[0]);
        EXPECT_TRUE(std::ranges::is_sorted(sorted))
            << "Algorithm failed: " << static_cast<int>(algorithm);
    }
}

TEST_F(ModernSorterTest, SortingStrategies)
{
    std::vector<Kakshya::DataVariant> single_channel = { Kakshya::DataVariant { test_data } };
    IO<std::vector<Kakshya::DataVariant>> input { single_channel };

    data_sorter->set_strategy(SortingStrategy::COPY_SORT);
    auto copy_result = data_sorter->apply_operation(input);
    EXPECT_TRUE(std::ranges::is_sorted(std::get<std::vector<double>>(copy_result.data[0])));

    data_sorter->set_strategy(SortingStrategy::IN_PLACE);
    auto inplace_result = data_sorter->apply_operation(input);
    EXPECT_TRUE(std::ranges::is_sorted(std::get<std::vector<double>>(inplace_result.data[0])));
}

TEST_F(ModernSorterTest, DuplicateValues)
{
    std::vector<double> duplicate_data { 3.0, 1.0, 3.0, 1.0, 2.0, 2.0 };
    std::vector<Kakshya::DataVariant> single_channel = { Kakshya::DataVariant { duplicate_data } };
    IO<std::vector<Kakshya::DataVariant>> input { single_channel };

    auto result = data_sorter->apply_operation(input);

    const auto& sorted = std::get<std::vector<double>>(result.data[0]);
    EXPECT_TRUE(std::ranges::is_sorted(sorted));
    EXPECT_EQ(std::count(sorted.begin(), sorted.end(), 1.0), 2);
    EXPECT_EQ(std::count(sorted.begin(), sorted.end(), 2.0), 2);
    EXPECT_EQ(std::count(sorted.begin(), sorted.end(), 3.0), 2);
}

TEST_F(ModernSorterTest, LargeDataPerformance)
{
    std::vector<double> large_data(10000);
    std::iota(large_data.begin(), large_data.end(), 0.0);
    std::shuffle(large_data.begin(), large_data.end(), std::mt19937 { 42 });

    std::vector<Kakshya::DataVariant> single_channel = { Kakshya::DataVariant { large_data } };
    IO<std::vector<Kakshya::DataVariant>> input { single_channel };

    auto start = std::chrono::high_resolution_clock::now();
    auto result = data_sorter->apply_operation(input);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    const auto& sorted = std::get<std::vector<double>>(result.data[0]);
    EXPECT_TRUE(std::ranges::is_sorted(sorted));
    EXPECT_EQ(sorted.size(), large_data.size());
    EXPECT_LT(duration.count(), 100);
}

} // namespace MayaFlux::Test
