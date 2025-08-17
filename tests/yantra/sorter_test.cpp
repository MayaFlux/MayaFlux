#include "../mock_signalsourcecontainer.hpp"
#include "MayaFlux/Yantra/Sorters/SortingHelper.hpp"
#include "MayaFlux/Yantra/Sorters/StandardSorter.hpp"

#include <random>

using namespace MayaFlux::Yantra;
using namespace MayaFlux::Kakshya;

namespace MayaFlux::Test {

template <ComputeData InputType = Kakshya::DataVariant, ComputeData OutputType = InputType>
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

        float_data = { 5.0, 2.0, 8.0, 1.0, 9.0 };

        container = std::make_shared<MockSignalSourceContainer>();
        container->set_test_data(test_data);

        data_sorter = std::make_shared<TestStandardSorter<Kakshya::DataVariant>>();
        vector_sorter = std::make_shared<TestStandardSorter<std::vector<double>>>();
        matrix_sorter = std::make_shared<TestStandardSorter<Eigen::MatrixXd>>();
        index_sorter = std::make_shared<TestStandardSorter<Kakshya::DataVariant, Kakshya::DataVariant>>();

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
        vector_sorter.reset();
        matrix_sorter.reset();
        index_sorter.reset();
    }

    std::vector<double> test_data;
    std::vector<double> sorted_ascending;
    std::vector<double> sorted_descending;
    std::vector<float> float_data;

    std::shared_ptr<MockSignalSourceContainer> container;
    RegionGroup test_group;

    std::shared_ptr<TestStandardSorter<Kakshya::DataVariant>> data_sorter;
    std::shared_ptr<TestStandardSorter<std::vector<double>>> vector_sorter;
    std::shared_ptr<TestStandardSorter<Eigen::MatrixXd>> matrix_sorter;
    std::shared_ptr<TestStandardSorter<Kakshya::DataVariant, Kakshya::DataVariant>> index_sorter;
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

TEST_F(ModernSorterTest, BasicDataVariantSorting)
{
    IO<Kakshya::DataVariant> input { Kakshya::DataVariant { test_data } };

    data_sorter->set_direction(SortingDirection::ASCENDING);
    auto result = data_sorter->apply_operation(input);

    EXPECT_EQ(result.data, Kakshya::DataVariant { sorted_ascending });

    data_sorter->set_direction(SortingDirection::DESCENDING);
    result = data_sorter->apply_operation(input);

    EXPECT_EQ(result.data, Kakshya::DataVariant { sorted_descending });
}

TEST_F(ModernSorterTest, VectorSorting)
{
    IO<std::vector<double>> input { test_data };

    vector_sorter->set_direction(SortingDirection::ASCENDING);
    auto result = vector_sorter->apply_operation(input);

    EXPECT_EQ(result.data, sorted_ascending);
    EXPECT_TRUE(std::ranges::is_sorted(result.data));
}

TEST_F(ModernSorterTest, EigenMatrixSorting)
{
    Eigen::MatrixXd test_matrix(2, 3);
    test_matrix << 3.0, 1.0, 2.0,
        6.0, 4.0, 5.0;

    IO<Eigen::MatrixXd> input { test_matrix };
    auto result = matrix_sorter->apply_operation(input);

    for (int i = 0; i < result.data.rows(); ++i) {
        for (int j = 1; j < result.data.cols(); ++j) {
            EXPECT_LE(result.data(i, j - 1), result.data(i, j));
        }
    }
}

TEST_F(ModernSorterTest, IndexOnlySorting)
{
    IO<Kakshya::DataVariant> input { Kakshya::DataVariant { test_data } };

    index_sorter->set_strategy(SortingStrategy::INDEX_ONLY);
    index_sorter->set_direction(SortingDirection::ASCENDING);

    EXPECT_NO_THROW(index_sorter->apply_operation(input));
}

TEST_F(ModernSorterTest, FloatDataSorting)
{
    Kakshya::DataVariant float_variant { float_data };
    IO<Kakshya::DataVariant> input { float_variant };

    auto result = data_sorter->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<float>>(result.data));
    const auto& sorted_floats = std::get<std::vector<float>>(result.data);
    EXPECT_TRUE(std::ranges::is_sorted(sorted_floats));
}

TEST_F(ModernSorterTest, HelperFunctionInPlace)
{
    std::vector<double> data_copy = test_data;

    sort_compute_data_inplace(data_copy, SortingDirection::ASCENDING, SortingAlgorithm::STANDARD);

    EXPECT_EQ(data_copy, sorted_ascending);
    EXPECT_TRUE(std::ranges::is_sorted(data_copy));
}

TEST_F(ModernSorterTest, HelperFunctionExtract)
{
    auto sorted_data = sort_compute_data_extract(test_data, SortingDirection::ASCENDING, SortingAlgorithm::STANDARD);

    EXPECT_NE(test_data, sorted_ascending);

    EXPECT_EQ(sorted_data, sorted_ascending);
    EXPECT_TRUE(std::ranges::is_sorted(sorted_data));
}

TEST_F(ModernSorterTest, DataVariantSortable)
{
    Kakshya::DataVariant sortable_data { test_data };
    Kakshya::DataVariant unsortable_data { std::vector<u_int8_t> { 1, 2, 3 } };

    EXPECT_TRUE(is_data_variant_sortable(sortable_data));
    EXPECT_TRUE(is_data_variant_sortable(unsortable_data));
}

TEST_F(ModernSorterTest, GenerateIndices)
{
    auto indices = generate_compute_data_indices(test_data, SortingDirection::ASCENDING);

    EXPECT_EQ(indices.size(), test_data.size());

    std::vector<double> sorted_by_indices;
    sorted_by_indices.reserve(indices.size());

    for (size_t idx : indices) {
        sorted_by_indices.push_back(test_data[idx]);
    }
    EXPECT_EQ(sorted_by_indices, sorted_ascending);
}

TEST_F(ModernSorterTest, RegionGroupSorting)
{
    auto region_sorter = std::make_shared<TestStandardSorter<RegionGroup>>();
    IO<RegionGroup> input { test_group };

    EXPECT_NO_THROW(region_sorter->apply_operation(input));

    auto result = region_sorter->apply_operation(input);
    EXPECT_EQ(result.data.regions.size(), test_group.regions.size());
}

TEST_F(ModernSorterTest, DifferentAlgorithms)
{
    IO<std::vector<double>> input { test_data };

    std::vector<SortingAlgorithm> algorithms = {
        SortingAlgorithm::STANDARD,
        SortingAlgorithm::STABLE,
        SortingAlgorithm::HEAP
    };

    for (auto algorithm : algorithms) {
        vector_sorter->set_algorithm(algorithm);
        auto result = vector_sorter->apply_operation(input);

        EXPECT_TRUE(std::ranges::is_sorted(result.data))
            << "Algorithm failed: " << static_cast<int>(algorithm);
    }
}

TEST_F(ModernSorterTest, SortingStrategies)
{
    IO<std::vector<double>> input { test_data };

    vector_sorter->set_strategy(SortingStrategy::COPY_SORT);
    auto copy_result = vector_sorter->apply_operation(input);
    EXPECT_TRUE(std::ranges::is_sorted(copy_result.data));

    vector_sorter->set_strategy(SortingStrategy::IN_PLACE);
    auto inplace_result = vector_sorter->apply_operation(input);
    EXPECT_TRUE(std::ranges::is_sorted(inplace_result.data));
}

TEST_F(ModernSorterTest, EmptyData)
{
    std::vector<double> empty_data;
    IO<std::vector<double>> input { empty_data };

    EXPECT_NO_THROW(vector_sorter->apply_operation(input));

    auto result = vector_sorter->apply_operation(input);
    EXPECT_TRUE(result.data.empty());
}

TEST_F(ModernSorterTest, SingleElement)
{
    std::vector<double> single_data { 42.0 };
    IO<std::vector<double>> input { single_data };

    auto result = vector_sorter->apply_operation(input);

    EXPECT_EQ(result.data.size(), 1);
    EXPECT_EQ(result.data[0], 42.0);
}

TEST_F(ModernSorterTest, DuplicateValues)
{
    std::vector<double> duplicate_data { 3.0, 1.0, 3.0, 1.0, 2.0, 2.0 };
    IO<std::vector<double>> input { duplicate_data };

    auto result = vector_sorter->apply_operation(input);

    EXPECT_TRUE(std::ranges::is_sorted(result.data));
    EXPECT_EQ(std::count(result.data.begin(), result.data.end(), 1.0), 2);
    EXPECT_EQ(std::count(result.data.begin(), result.data.end(), 2.0), 2);
    EXPECT_EQ(std::count(result.data.begin(), result.data.end(), 3.0), 2);
}

TEST_F(ModernSorterTest, LargeDataPerformance)
{
    std::vector<double> large_data(10000);
    std::iota(large_data.begin(), large_data.end(), 0.0);
    std::shuffle(large_data.begin(), large_data.end(), std::mt19937 { 42 });

    IO<std::vector<double>> input { large_data };

    auto start = std::chrono::high_resolution_clock::now();
    auto result = vector_sorter->apply_operation(input);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_TRUE(std::ranges::is_sorted(result.data));
    EXPECT_EQ(result.data.size(), large_data.size());
    EXPECT_LT(duration.count(), 100);
}

TEST_F(ModernSorterTest, EndToEndWorkflow)
{
    IO<Kakshya::DataVariant> input { Kakshya::DataVariant { test_data } };

    data_sorter->set_direction(SortingDirection::ASCENDING);
    data_sorter->set_strategy(SortingStrategy::COPY_SORT);
    data_sorter->set_granularity(SortingGranularity::RAW_DATA);

    auto result = data_sorter->apply_operation(input);

    EXPECT_EQ(result.data, Kakshya::DataVariant { sorted_ascending });

    data_sorter->set_direction(SortingDirection::DESCENDING);
    result = data_sorter->apply_operation(input);

    EXPECT_EQ(result.data, Kakshya::DataVariant { sorted_descending });
}

} // namespace MayaFlux::Test
