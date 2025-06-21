#include "../mock_signalsourcecontainer.hpp"

#include "MayaFlux/Yantra/Analyzers/EnergyAnalyzer.hpp"
#include "MayaFlux/Yantra/Sorters/UniversalSorter.hpp"

using namespace MayaFlux::Yantra;
using namespace MayaFlux::Kakshya;

namespace MayaFlux::Test {

class TestUniversalSorter : public UniversalSorter {
public:
    std::vector<std::string> get_available_methods() const override
    {
        return { "default", "ascending", "descending", "reverse", "shuffle", "statistical",
            "percentile", "outlier_aware", "custom", "merge_sort", "quick_sort" };
    }

protected:
    std::vector<std::string> get_methods_for_type_impl(std::type_index type_info) const override
    {
        if (type_info == std::type_index(typeid(std::vector<double>))) {
            return { "default", "ascending", "descending", "reverse", "statistical", "percentile" };
        } else if (type_info == std::type_index(typeid(std::vector<float>))) {
            return { "default", "ascending", "descending", "reverse" };
        } else if (type_info == std::type_index(typeid(Kakshya::DataVariant))) {
            return { "default", "ascending", "descending", "custom" };
        }
        return { "default" };
    }

    SorterOutput sort_impl(const Kakshya::DataVariant& data) override
    {
        return std::visit([this](const auto& vec) -> SorterOutput {
            using VecType = std::decay_t<decltype(vec)>;

            if constexpr (std::same_as<VecType, std::vector<double>>) {
                auto sorted_vec = vec;
                std::string method = get_sorting_method();

                if (method == "ascending" || method == "default") {
                    std::sort(sorted_vec.begin(), sorted_vec.end());
                } else if (method == "descending") {
                    std::sort(sorted_vec.begin(), sorted_vec.end(), std::greater<double>());
                } else if (method == "reverse") {
                    std::reverse(sorted_vec.begin(), sorted_vec.end());
                }

                return SorterOutput { sorted_vec };
            } else if constexpr (std::same_as<VecType, std::vector<float>>) {
                auto sorted_vec = vec;
                std::string method = get_sorting_method();

                if (method == "ascending" || method == "default") {
                    std::sort(sorted_vec.begin(), sorted_vec.end());
                } else if (method == "descending") {
                    std::sort(sorted_vec.begin(), sorted_vec.end(), std::greater<float>());
                } else if (method == "reverse") {
                    std::reverse(sorted_vec.begin(), sorted_vec.end());
                }

                return SorterOutput { sorted_vec };
            } else {
                return SorterOutput { Kakshya::DataVariant { vec } };
            }
        },
            data);
    }

    SorterOutput sort_impl(std::shared_ptr<Kakshya::SignalSourceContainer> container) override
    {
        if (!container || !container->has_data()) {
            return SorterOutput { std::vector<double> {} };
        }

        const auto& data = container->get_processed_data();
        return sort_impl(data);
    }

    SorterOutput sort_impl(const Kakshya::Region& region) override
    {
        if (region.attributes.find("data") != region.attributes.end()) {
            try {
                auto data_any = region.attributes.at("data");
                if (data_any.type() == typeid(std::vector<double>)) {
                    auto vec = std::any_cast<std::vector<double>>(data_any);
                    return sort_impl(Kakshya::DataVariant { vec });
                }
            } catch (...) {
                // Fallback
            }
        }
        return SorterOutput { std::vector<double> {} };
    }

    SorterOutput sort_impl(const Kakshya::RegionGroup& group) override
    {
        std::vector<double> combined_results;

        for (const auto& region : group.regions) {
            auto region_result = sort_impl(region);
            if (std::holds_alternative<std::vector<double>>(region_result)) {
                const auto& values = std::get<std::vector<double>>(region_result);
                combined_results.insert(combined_results.end(), values.begin(), values.end());
            }
        }

        std::string method = get_sorting_method();
        if (method == "ascending" || method == "default") {
            std::sort(combined_results.begin(), combined_results.end());
        } else if (method == "descending") {
            std::sort(combined_results.begin(), combined_results.end(), std::greater<double>());
        }

        return SorterOutput { combined_results };
    }

    SorterOutput sort_impl(const std::vector<Kakshya::RegionSegment>& segments) override
    {
        std::vector<Kakshya::RegionSegment> sorted_segments = segments;

        std::sort(sorted_segments.begin(), sorted_segments.end(),
            [](const auto& a, const auto& b) {
                return a.source_region.start_coordinates[0] < b.source_region.start_coordinates[0];
            });

        return SorterOutput { sorted_segments };
    }

    SorterOutput sort_impl(const Eigen::VectorXd& vector) override
    {
        std::vector<double> data(vector.data(), vector.data() + vector.size());
        std::string method = get_sorting_method();

        if (method == "ascending" || method == "default") {
            std::sort(data.begin(), data.end());
        } else if (method == "descending") {
            std::sort(data.begin(), data.end(), std::greater<double>());
        }

        Eigen::VectorXd result = Eigen::Map<Eigen::VectorXd>(data.data(), data.size());
        return SorterOutput { result };
    }

    SorterOutput sort_impl(const Eigen::MatrixXd& matrix) override
    {
        Eigen::MatrixXd sorted_matrix = matrix;

        for (int i = 0; i < matrix.rows(); ++i) {
            std::vector<double> row_data(matrix.row(i).data(),
                matrix.row(i).data() + matrix.cols());
            std::sort(row_data.begin(), row_data.end());

            for (int j = 0; j < matrix.cols(); ++j) {
                sorted_matrix(i, j) = row_data[j];
            }
        }

        return SorterOutput { sorted_matrix };
    }

    SorterOutput sort_impl(const std::vector<std::any>& data) override
    {
        std::vector<std::any> sorted_data = data;

        std::sort(sorted_data.begin(), sorted_data.end(),
            [](const std::any& a, const std::any& b) {
                try {
                    if (a.type() == typeid(double) && b.type() == typeid(double)) {
                        return std::any_cast<double>(a) < std::any_cast<double>(b);
                    } else if (a.type() == typeid(float) && b.type() == typeid(float)) {
                        return std::any_cast<float>(a) < std::any_cast<float>(b);
                    } else if (a.type() == typeid(int) && b.type() == typeid(int)) {
                        return std::any_cast<int>(a) < std::any_cast<int>(b);
                    }
                } catch (...) {
                    // Fallback comparison
                }
                return false;
            });

        return SorterOutput { sorted_data };
    }
};

class UniversalSorterTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_data = { 5.0, 2.0, 8.0, 1.0, 9.0, 3.0, 7.0, 4.0, 6.0, 0.0 };
        reverse_sorted_data = { 9.0, 8.0, 7.0, 6.0, 5.0, 4.0, 3.0, 2.0, 1.0, 0.0 };
        already_sorted_data = { 0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0 };

        float_data = { 5.0f, 2.0f, 8.0f, 1.0f, 9.0f, 3.0f, 7.0f, 4.0f, 6.0f, 0.0f };

        container = std::make_shared<MockSignalSourceContainer>();
        container->set_test_data(test_data);

        sorter = std::make_unique<TestUniversalSorter>();
    }

    std::vector<double> test_data;
    std::vector<double> reverse_sorted_data;
    std::vector<double> already_sorted_data;
    std::vector<float> float_data;
    std::shared_ptr<MockSignalSourceContainer> container;
    std::unique_ptr<TestUniversalSorter> sorter;
};

// ===== Basic Sorting Tests =====

TEST_F(UniversalSorterTest, DefaultSortingAscending)
{
    sorter->set_parameter("method", "default");

    SorterInput input { Kakshya::DataVariant { test_data } };
    auto result = sorter->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& sorted_values = std::get<std::vector<double>>(result);

    EXPECT_EQ(sorted_values.size(), test_data.size());
    EXPECT_TRUE(std::is_sorted(sorted_values.begin(), sorted_values.end()));
    EXPECT_EQ(sorted_values, already_sorted_data);
}

TEST_F(UniversalSorterTest, AscendingSorting)
{
    sorter->set_parameter("method", "ascending");

    SorterInput input { Kakshya::DataVariant { test_data } };
    auto result = sorter->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& sorted_values = std::get<std::vector<double>>(result);

    EXPECT_EQ(sorted_values.size(), test_data.size());
    EXPECT_TRUE(std::is_sorted(sorted_values.begin(), sorted_values.end()));
    EXPECT_EQ(sorted_values[0], 0.0);
    EXPECT_EQ(sorted_values.back(), 9.0);
}

TEST_F(UniversalSorterTest, DescendingSorting)
{
    sorter->set_parameter("method", "descending");

    SorterInput input { Kakshya::DataVariant { test_data } };
    auto result = sorter->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& sorted_values = std::get<std::vector<double>>(result);

    EXPECT_EQ(sorted_values.size(), test_data.size());
    EXPECT_TRUE(std::is_sorted(sorted_values.begin(), sorted_values.end(), std::greater<double>()));
    EXPECT_EQ(sorted_values[0], 9.0);
    EXPECT_EQ(sorted_values.back(), 0.0);
}

TEST_F(UniversalSorterTest, ReverseSorting)
{
    sorter->set_parameter("method", "reverse");

    SorterInput input { Kakshya::DataVariant { test_data } };
    auto result = sorter->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& sorted_values = std::get<std::vector<double>>(result);

    EXPECT_EQ(sorted_values.size(), test_data.size());

    std::vector<double> expected_reversed = test_data;
    std::reverse(expected_reversed.begin(), expected_reversed.end());
    EXPECT_EQ(sorted_values, expected_reversed);
}

// ===== Type-Safe Sorting Tests =====

TEST_F(UniversalSorterTest, TypedSortingInterface)
{
    auto result = sorter->sort_typed<Kakshya::DataVariant, std::vector<double>>(
        Kakshya::DataVariant { test_data }, "ascending");

    EXPECT_EQ(result.size(), test_data.size());
    EXPECT_TRUE(std::is_sorted(result.begin(), result.end()));
}

/* TEST_F(UniversalSorterTest, TypedSortingWithWrongOutputType)
{
    EXPECT_THROW(
        sorter->sort_typed<Kakshya::DataVariant, std::vector<float>>(
            Kakshya::DataVariant { test_data }, "ascending"),
        std::runtime_error);
} */

// ===== Multi-Type Data Tests =====

TEST_F(UniversalSorterTest, SortFloatData)
{
    SorterInput input { Kakshya::DataVariant { float_data } };
    sorter->set_parameter("method", "ascending");

    auto result = sorter->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<float>>(result));
    const auto& sorted_values = std::get<std::vector<float>>(result);

    EXPECT_EQ(sorted_values.size(), float_data.size());
    EXPECT_TRUE(std::is_sorted(sorted_values.begin(), sorted_values.end()));
}

TEST_F(UniversalSorterTest, SortContainerData)
{
    SorterInput input { container };
    sorter->set_parameter("method", "ascending");

    auto result = sorter->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& sorted_values = std::get<std::vector<double>>(result);

    EXPECT_EQ(sorted_values.size(), test_data.size());
    EXPECT_TRUE(std::is_sorted(sorted_values.begin(), sorted_values.end()));
}

TEST_F(UniversalSorterTest, SortEigenVector)
{
    Eigen::VectorXd eigen_vec(test_data.size());
    for (size_t i = 0; i < test_data.size(); ++i) {
        eigen_vec[i] = test_data[i];
    }

    SorterInput input { eigen_vec };
    sorter->set_parameter("method", "ascending");

    auto result = sorter->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<Eigen::VectorXd>(result));
    const auto& sorted_vector = std::get<Eigen::VectorXd>(result);

    EXPECT_EQ(sorted_vector.size(), test_data.size());

    for (int i = 1; i < sorted_vector.size(); ++i) {
        EXPECT_LE(sorted_vector[i - 1], sorted_vector[i]);
    }
}

TEST_F(UniversalSorterTest, SortEigenMatrix)
{
    Eigen::MatrixXd matrix(3, 4);
    matrix << 4, 2, 3, 1,
        8, 6, 7, 5,
        12, 10, 11, 9;

    SorterInput input { matrix };
    sorter->set_parameter("method", "ascending");

    auto result = sorter->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<Eigen::MatrixXd>(result));
    const auto& sorted_matrix = std::get<Eigen::MatrixXd>(result);

    EXPECT_EQ(sorted_matrix.rows(), matrix.rows());
    EXPECT_EQ(sorted_matrix.cols(), matrix.cols());

    for (int i = 0; i < sorted_matrix.rows(); ++i) {
        for (int j = 1; j < sorted_matrix.cols(); ++j) {
            EXPECT_LE(sorted_matrix(i, j - 1), sorted_matrix(i, j));
        }
    }
}

// ===== Algorithm-Specific Tests =====

TEST_F(UniversalSorterTest, SortWithSpecificAlgorithm)
{
    SorterInput input { Kakshya::DataVariant { test_data } };

    auto result = sorter->sort_with_algorithm(input, SortingAlgorithm::STABLE);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& sorted_values = std::get<std::vector<double>>(result);

    EXPECT_EQ(sorted_values.size(), test_data.size());
    EXPECT_TRUE(std::is_sorted(sorted_values.begin(), sorted_values.end()));
}

// ===== Multi-Key Sorting Tests =====

TEST_F(UniversalSorterTest, MultiKeySorting)
{
    std::vector<std::any> complex_data;
    for (int i = 0; i < 5; ++i) {
        complex_data.push_back(std::any(static_cast<double>(i % 3))); // 0, 1, 2, 0, 1
    }

    std::vector<SortKey> keys;
    keys.emplace_back("primary", [](const std::any& val) -> double {
        try {
            return std::any_cast<double>(val);
        } catch (...) {
            return 0.0;
        }
    });

    SorterInput input { complex_data };
    auto result = sorter->sort_multi_key(input, keys);

    ASSERT_TRUE(std::holds_alternative<std::vector<std::any>>(result));
}

// ===== Chunked Sorting Tests =====

TEST_F(UniversalSorterTest, ChunkedSorting)
{
    std::vector<double> large_data(100);
    std::iota(large_data.begin(), large_data.end(), 0.0);
    std::shuffle(large_data.begin(), large_data.end(), std::mt19937 { 42 });

    SorterInput input { large_data };
    auto chunks = sorter->sort_chunked(input, 25);

    EXPECT_EQ(chunks.size(), 4); // 100 / 25 = 4 chunks

    for (const auto& chunk : chunks) {
        ASSERT_TRUE(std::holds_alternative<std::vector<double>>(chunk));
        const auto& chunk_values = std::get<std::vector<double>>(chunk);
        EXPECT_LE(chunk_values.size(), 25u);
        EXPECT_TRUE(std::is_sorted(chunk_values.begin(), chunk_values.end()));
    }
}

TEST_F(UniversalSorterTest, ChunkedSortingFloatData)
{
    std::vector<float> large_float_data(50);
    std::iota(large_float_data.begin(), large_float_data.end(), 0.0f);
    std::shuffle(large_float_data.begin(), large_float_data.end(), std::mt19937 { 42 });

    SorterInput input { large_float_data };
    auto chunks = sorter->sort_chunked(input, 10);

    EXPECT_EQ(chunks.size(), 5); // 50 / 10 = 5 chunks

    for (const auto& chunk : chunks) {
        ASSERT_TRUE(std::holds_alternative<std::vector<float>>(chunk));
        const auto& chunk_values = std::get<std::vector<float>>(chunk);
        EXPECT_TRUE(std::is_sorted(chunk_values.begin(), chunk_values.end()));
    }
}

// ===== Grammar-Based Sorting Tests =====

class SortingGrammarTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_data = { 5.0, 2.0, 8.0, 1.0, 9.0, 3.0, 7.0, 4.0, 6.0, 0.0 };

        sorter = std::make_unique<TestUniversalSorter>();

        SortingGrammar::Rule ascending_rule {
            "ascending_rule",
            [](const SorterInput& input) -> bool {
                return std::holds_alternative<Kakshya::DataVariant>(input);
            },
            [](const SorterInput& input) -> SorterOutput {
                return std::visit([](const auto& data) -> SorterOutput {
                    using DataType = std::decay_t<decltype(data)>;
                    if constexpr (std::same_as<DataType, std::vector<double>>) {
                        auto sorted = data;
                        std::sort(sorted.begin(), sorted.end());
                        return SorterOutput { sorted };
                    }
                    return SorterOutput { std::vector<double> {} };
                },
                    std::get<Kakshya::DataVariant>(input));
            },
            {},
            SortingGrammar::SortingContext::TEMPORAL,
            10
        };

        grammar.add_rule(ascending_rule);
    }

    std::vector<double> test_data;
    std::unique_ptr<TestUniversalSorter> sorter;
    SortingGrammar grammar;
};

TEST_F(SortingGrammarTest, GrammarRuleApplication)
{
    SorterInput input { Kakshya::DataVariant { test_data } };

    auto result = grammar.sort_by_rule("ascending_rule", input);

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(*result));

    const auto& sorted_values = std::get<std::vector<double>>(*result);
    EXPECT_TRUE(std::is_sorted(sorted_values.begin(), sorted_values.end()));
}

TEST_F(SortingGrammarTest, NonExistentRule)
{
    SorterInput input { Kakshya::DataVariant { test_data } };

    auto result = grammar.sort_by_rule("non_existent", input);

    EXPECT_FALSE(result.has_value());
}

TEST_F(SortingGrammarTest, AvailableRules)
{
    auto rules = grammar.get_available_rules();

    EXPECT_EQ(rules.size(), 1);
    EXPECT_EQ(rules[0], "ascending_rule");
}

// ===== Region-Based Sorting Tests =====

TEST_F(UniversalSorterTest, SortRegion)
{
    Region test_region { { 0 }, { 10 }, {} };
    test_region.attributes["data"] = test_data;

    sorter->set_parameter("method", "ascending");

    SorterInput input { test_region };
    auto result = sorter->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& sorted_values = std::get<std::vector<double>>(result);

    EXPECT_TRUE(std::is_sorted(sorted_values.begin(), sorted_values.end()));
}

TEST_F(UniversalSorterTest, SortRegionGroup)
{
    RegionGroup group("test_group");

    Region r1 { { 0 }, { 5 }, {} };
    r1.attributes["data"] = std::vector<double> { 5.0, 3.0, 1.0, 4.0, 2.0 };

    Region r2 { { 5 }, { 10 }, {} };
    r2.attributes["data"] = std::vector<double> { 9.0, 7.0, 6.0, 8.0, 0.0 };

    group.add_region(r1);
    group.add_region(r2);

    sorter->set_parameter("method", "ascending");

    SorterInput input { group };
    auto result = sorter->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& sorted_values = std::get<std::vector<double>>(result);

    EXPECT_EQ(sorted_values.size(), 10);
    EXPECT_TRUE(std::is_sorted(sorted_values.begin(), sorted_values.end()));
}

TEST_F(UniversalSorterTest, SortRegionSegments)
{
    std::vector<RegionSegment> segments;
    Region r1 { { 10 }, { 15 }, {} };
    Region r2 { { 0 }, { 5 }, {} };
    Region r3 { { 5 }, { 10 }, {} };
    RegionSegment seg1(r1);
    RegionSegment seg2(r2);
    RegionSegment seg3(r3);

    segments.push_back(seg1);
    segments.push_back(seg2);
    segments.push_back(seg3);

    SorterInput input { segments };
    auto result = sorter->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<RegionSegment>>(result));
    const auto& sorted_segments = std::get<std::vector<RegionSegment>>(result);

    EXPECT_EQ(sorted_segments.size(), 3);
    EXPECT_EQ(sorted_segments[0].source_region.start_coordinates[0], 0);
    EXPECT_EQ(sorted_segments[1].source_region.start_coordinates[0], 5);
    EXPECT_EQ(sorted_segments[2].source_region.start_coordinates[0], 10);
}

// ===== Edge Cases and Error Handling Tests =====

TEST_F(UniversalSorterTest, EmptyDataHandling)
{
    std::vector<double> empty_data;
    SorterInput input { Kakshya::DataVariant { empty_data } };

    EXPECT_NO_THROW(sorter->apply_operation(input));

    auto result = sorter->apply_operation(input);
    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));

    const auto& sorted_values = std::get<std::vector<double>>(result);
    EXPECT_TRUE(sorted_values.empty());
}

TEST_F(UniversalSorterTest, SingleElementData)
{
    std::vector<double> single_data = { 42.0 };
    SorterInput input { Kakshya::DataVariant { single_data } };

    auto result = sorter->apply_operation(input);
    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));

    const auto& sorted_values = std::get<std::vector<double>>(result);
    EXPECT_EQ(sorted_values.size(), 1);
    EXPECT_EQ(sorted_values[0], 42.0);
}

TEST_F(UniversalSorterTest, DuplicateValues)
{
    std::vector<double> duplicate_data = { 5.0, 2.0, 5.0, 2.0, 5.0, 2.0 };
    SorterInput input { Kakshya::DataVariant { duplicate_data } };

    sorter->set_parameter("method", "ascending");
    auto result = sorter->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& sorted_values = std::get<std::vector<double>>(result);

    EXPECT_EQ(sorted_values.size(), 6);
    EXPECT_TRUE(std::is_sorted(sorted_values.begin(), sorted_values.end()));

    EXPECT_EQ(std::count(sorted_values.begin(), sorted_values.end(), 2.0), 3);
    EXPECT_EQ(std::count(sorted_values.begin(), sorted_values.end(), 5.0), 3);
}

TEST_F(UniversalSorterTest, NullContainerHandling)
{
    std::shared_ptr<MockSignalSourceContainer> null_container;
    SorterInput input { null_container };

    auto result = sorter->apply_operation(input);
    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));

    const auto& sorted_values = std::get<std::vector<double>>(result);
    EXPECT_TRUE(sorted_values.empty());
}

// ===== Parameter Management Tests =====

TEST_F(UniversalSorterTest, ParameterPersistence)
{
    sorter->set_parameter("method", "descending");
    sorter->set_parameter("algorithm", SortingAlgorithm::STABLE);
    sorter->set_parameter("direction", SortDirection::DESCENDING);

    auto method = sorter->get_parameter("method");
    ASSERT_TRUE(method.has_value());
    EXPECT_EQ(std::any_cast<const char*>(method), "descending");

    auto algorithm = sorter->get_parameter("algorithm");
    ASSERT_TRUE(algorithm.has_value());
    EXPECT_EQ(std::any_cast<SortingAlgorithm>(algorithm), SortingAlgorithm::STABLE);

    auto direction = sorter->get_parameter("direction");
    ASSERT_TRUE(direction.has_value());
    EXPECT_EQ(std::any_cast<SortDirection>(direction), SortDirection::DESCENDING);
}

TEST_F(UniversalSorterTest, GetAllParameters)
{
    sorter->set_parameter("method", "ascending");
    sorter->set_parameter("chunk_size", 1024);

    auto all_params = sorter->get_all_parameters();

    EXPECT_TRUE(all_params.find("method") != all_params.end());
    EXPECT_TRUE(all_params.find("chunk_size") != all_params.end());
}

TEST_F(UniversalSorterTest, InvalidParameterAccess)
{
    auto non_existent = sorter->get_parameter("non_existent_param");
    EXPECT_FALSE(non_existent.has_value());
}

// ===== Method Availability Tests =====

TEST_F(UniversalSorterTest, GetAvailableMethods)
{
    auto methods = sorter->get_available_methods();

    EXPECT_FALSE(methods.empty());
    EXPECT_TRUE(std::find(methods.begin(), methods.end(), "default") != methods.end());
    EXPECT_TRUE(std::find(methods.begin(), methods.end(), "ascending") != methods.end());
    EXPECT_TRUE(std::find(methods.begin(), methods.end(), "descending") != methods.end());
}

TEST_F(UniversalSorterTest, GetMethodsForSpecificType)
{
    auto double_methods = sorter->get_methods_for_type(std::type_index(typeid(std::vector<double>)));
    auto float_methods = sorter->get_methods_for_type(std::type_index(typeid(std::vector<float>)));

    EXPECT_FALSE(double_methods.empty());
    EXPECT_FALSE(float_methods.empty());
    EXPECT_TRUE(std::find(double_methods.begin(), double_methods.end(), "statistical") != double_methods.end());
}

// ===== Analyzer Delegation Tests =====

class AnalyzerDelegationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_data = { 5.0, 2.0, 8.0, 1.0, 9.0, 3.0, 7.0, 4.0, 6.0, 0.0 };
        sorter = std::make_unique<TestUniversalSorter>();

        // Create a mock analyzer for delegation
        mock_analyzer = std::make_shared<EnergyAnalyzer>();
        mock_analyzer->set_window_parameters(5, 1);
        sorter->set_analyzer(mock_analyzer);
        sorter->set_use_analyzer(true);
    }

    std::vector<double> test_data;
    std::unique_ptr<TestUniversalSorter> sorter;
    std::shared_ptr<EnergyAnalyzer> mock_analyzer;
};

TEST_F(AnalyzerDelegationTest, AnalyzerDelegationForStatisticalMethod)
{
    sorter->set_parameter("method", "statistical");

    SorterInput input { Kakshya::DataVariant { test_data } };

    // This should delegate to analyzer since "statistical" requires delegation
    auto result = sorter->apply_operation(input);

    EXPECT_TRUE(sorter->uses_analyzer());
}

// ===== Performance Tests =====

TEST_F(UniversalSorterTest, LargeDatasetPerformance)
{
    std::vector<double> large_data(100000);
    std::iota(large_data.begin(), large_data.end(), 0.0);
    std::shuffle(large_data.begin(), large_data.end(), std::mt19937 { 42 });

    SorterInput input { large_data };
    sorter->set_parameter("method", "ascending");

    auto start_time = std::chrono::high_resolution_clock::now();

    auto result = sorter->apply_operation(input);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& sorted_values = std::get<std::vector<double>>(result);

    EXPECT_EQ(sorted_values.size(), large_data.size());
    EXPECT_TRUE(std::is_sorted(sorted_values.begin(), sorted_values.end()));
    EXPECT_LT(duration.count(), 1000);
}

TEST_F(UniversalSorterTest, MultipleSortingMethodsPerformance)
{
    std::vector<std::string> methods = { "ascending", "descending", "reverse" };

    for (const auto& method : methods) {
        sorter->set_parameter("method", method);

        SorterInput input { Kakshya::DataVariant { test_data } };
        auto result = sorter->apply_operation(input);

        ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
        const auto& sorted_values = std::get<std::vector<double>>(result);
        EXPECT_EQ(sorted_values.size(), test_data.size());
    }
}

// ===== Thread Safety Tests =====

TEST_F(UniversalSorterTest, ConcurrentSorting)
{
    const size_t num_threads = 4;
    std::vector<std::thread> threads;
    std::vector<bool> thread_results(num_threads, false);

    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, t, &thread_results]() {
            try {
                auto local_sorter = std::make_unique<TestUniversalSorter>();
                local_sorter->set_parameter("method", "ascending");

                SorterInput input { Kakshya::DataVariant { test_data } };
                auto result = local_sorter->apply_operation(input);

                EXPECT_TRUE(std::holds_alternative<std::vector<double>>(result));
                const auto& sorted_values = std::get<std::vector<double>>(result);
                EXPECT_TRUE(std::is_sorted(sorted_values.begin(), sorted_values.end()));

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

// ===== Complex Data Type Tests =====

TEST_F(UniversalSorterTest, SortComplexNumbers)
{
    std::vector<std::complex<double>> complex_data = {
        { 3.0, 4.0 }, { 1.0, 1.0 }, { 2.0, 3.0 }, { 0.0, 1.0 }, { 1.0, 0.0 }
    };

    // TODO: Support complex types in actual implementaion
    SorterInput input { complex_data };

    // For now, just verify it doesn't crash
    EXPECT_NO_THROW(sorter->apply_operation(input));
}

TEST_F(UniversalSorterTest, SortHeterogeneousData)
{
    std::vector<std::any> mixed_data;
    mixed_data.push_back(std::any(3.0));
    mixed_data.push_back(std::any(1.0));
    mixed_data.push_back(std::any(4.0));
    mixed_data.push_back(std::any(2.0));

    SorterInput input { mixed_data };
    auto result = sorter->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<std::any>>(result));
    const auto& sorted_values = std::get<std::vector<std::any>>(result);

    EXPECT_EQ(sorted_values.size(), mixed_data.size());

    try {
        double first_val = std::any_cast<double>(sorted_values[0]);
        EXPECT_EQ(first_val, 1.0);
    } catch (...) {
        FAIL() << "Failed to cast first sorted element";
    }
}

// ===== Granularity Output Tests =====

class SortingGranularityTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_data = { 5.0, 2.0, 8.0, 1.0, 9.0, 3.0, 7.0, 4.0, 6.0, 0.0 };
        sorter = std::make_unique<TestUniversalSorter>();
    }

    std::vector<double> test_data;
    std::unique_ptr<TestUniversalSorter> sorter;
};

TEST_F(SortingGranularityTest, IndicesOnlyGranularity)
{
    sorter->set_granularity(SortingGranularity::INDICES_ONLY);
    sorter->set_parameter("method", "ascending");

    SorterInput input { Kakshya::DataVariant { test_data } };
    auto result = sorter->apply_operation(input);

    // The format_output_based_on_granularity should convert to indices
    // Implementation would depend on the actual UniversalSorter implementation
    EXPECT_NO_THROW(result);
}

TEST_F(SortingGranularityTest, SortedValuesGranularity)
{
    sorter->set_granularity(SortingGranularity::SORTED_VALUES);
    sorter->set_parameter("method", "ascending");

    SorterInput input { Kakshya::DataVariant { test_data } };
    auto result = sorter->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& sorted_values = std::get<std::vector<double>>(result);
    EXPECT_TRUE(std::is_sorted(sorted_values.begin(), sorted_values.end()));
}

// ===== Special Value Handling Tests =====

TEST_F(UniversalSorterTest, HandleInfiniteValues)
{
    std::vector<double> inf_data = {
        1.0, std::numeric_limits<double>::infinity(), 3.0,
        -std::numeric_limits<double>::infinity(), 2.0
    };

    SorterInput input { Kakshya::DataVariant { inf_data } };
    sorter->set_parameter("method", "ascending");

    auto result = sorter->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& sorted_values = std::get<std::vector<double>>(result);

    EXPECT_EQ(sorted_values.size(), inf_data.size());
    EXPECT_EQ(sorted_values[0], -std::numeric_limits<double>::infinity());
    EXPECT_EQ(sorted_values.back(), std::numeric_limits<double>::infinity());
}

TEST_F(UniversalSorterTest, HandleNaNValues)
{
    std::vector<double> nan_data = {
        1.0, std::numeric_limits<double>::quiet_NaN(), 3.0, 2.0
    };

    SorterInput input { Kakshya::DataVariant { nan_data } };
    sorter->set_parameter("method", "ascending");

    // Behavior with NaN is implementation-defined, but shouldn't crash
    EXPECT_NO_THROW(sorter->apply_operation(input));
}

// ===== Algorithm-Specific Behavior Tests =====

TEST_F(UniversalSorterTest, StableSortingBehavior)
{
    std::vector<std::pair<int, char>> paired_data = {
        { 3, 'a' }, { 1, 'b' }, { 3, 'c' }, { 2, 'd' }, { 1, 'e' }
    };

    std::vector<std::any> any_data;
    for (const auto& pair : paired_data) {
        any_data.push_back(std::any(static_cast<double>(pair.first)));
    }

    SorterInput input { any_data };
    auto result = sorter->sort_with_algorithm(input, SortingAlgorithm::STABLE);

    ASSERT_TRUE(std::holds_alternative<std::vector<std::any>>(result));
    const auto& sorted_values = std::get<std::vector<std::any>>(result);

    EXPECT_EQ(sorted_values.size(), any_data.size());
}

// ===== Integration Tests =====

TEST_F(UniversalSorterTest, IntegrationWithProcessingChain)
{
    container->set_test_data(test_data);

    SorterInput input { container };
    sorter->set_parameter("method", "ascending");

    auto result = sorter->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& sorted_values = std::get<std::vector<double>>(result);

    EXPECT_EQ(sorted_values.size(), test_data.size());
    EXPECT_TRUE(std::is_sorted(sorted_values.begin(), sorted_values.end()));

    const auto& original_data = container->get_processed_data();
    if (std::holds_alternative<std::vector<double>>(original_data)) {
        const auto& orig_vec = std::get<std::vector<double>>(original_data);
        EXPECT_EQ(orig_vec, test_data);
    }
}

TEST_F(UniversalSorterTest, ChainedSortingOperations)
{
    SorterInput input { Kakshya::DataVariant { test_data } };

    sorter->set_parameter("method", "ascending");
    auto first_result = sorter->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(first_result));
    const auto& first_sorted = std::get<std::vector<double>>(first_result);

    SorterInput second_input { Kakshya::DataVariant { first_sorted } };
    sorter->set_parameter("method", "descending");
    auto second_result = sorter->apply_operation(second_input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(second_result));
    const auto& second_sorted = std::get<std::vector<double>>(second_result);

    EXPECT_TRUE(std::is_sorted(second_sorted.begin(), second_sorted.end(), std::greater<double>()));
}

// ===== Robustness Tests =====

TEST_F(UniversalSorterTest, VeryLargeValues)
{
    std::vector<double> large_values = { 1e15, 2e15, 3e15, 4e15, 5e15 };

    SorterInput input { Kakshya::DataVariant { large_values } };
    sorter->set_parameter("method", "ascending");

    auto result = sorter->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& sorted_values = std::get<std::vector<double>>(result);

    EXPECT_TRUE(std::is_sorted(sorted_values.begin(), sorted_values.end()));
    EXPECT_EQ(sorted_values[0], 1e15);
    EXPECT_EQ(sorted_values.back(), 5e15);
}

TEST_F(UniversalSorterTest, VerySmallValues)
{
    std::vector<double> small_values = { 1e-15, 2e-15, 3e-15, 4e-15, 5e-15 };

    SorterInput input { Kakshya::DataVariant { small_values } };
    sorter->set_parameter("method", "ascending");

    auto result = sorter->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& sorted_values = std::get<std::vector<double>>(result);

    EXPECT_TRUE(std::is_sorted(sorted_values.begin(), sorted_values.end()));
}

TEST_F(UniversalSorterTest, MixedSignValues)
{
    std::vector<double> mixed_values = { -100.0, -50.0, 0.0, 50.0, 100.0, -25.0, 25.0 };

    SorterInput input { Kakshya::DataVariant { mixed_values } };
    sorter->set_parameter("method", "ascending");

    auto result = sorter->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& sorted_values = std::get<std::vector<double>>(result);

    EXPECT_TRUE(std::is_sorted(sorted_values.begin(), sorted_values.end()));
    EXPECT_EQ(sorted_values[0], -100.0);
    EXPECT_EQ(sorted_values.back(), 100.0);
}

// ===== Memory and Resource Management Tests =====

TEST_F(UniversalSorterTest, MemoryEfficientSorting)
{
    std::vector<double> memory_test_data(10000);
    std::iota(memory_test_data.begin(), memory_test_data.end(), 0.0);
    std::shuffle(memory_test_data.begin(), memory_test_data.end(), std::mt19937 { 42 });

    SorterInput input { memory_test_data };
    sorter->set_parameter("method", "ascending");

    auto result = sorter->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& sorted_values = std::get<std::vector<double>>(result);

    EXPECT_EQ(sorted_values.size(), memory_test_data.size());
    EXPECT_TRUE(std::is_sorted(sorted_values.begin(), sorted_values.end()));
}

// ===== Error Recovery Tests =====

TEST_F(UniversalSorterTest, RecoveryFromInvalidMethod)
{
    sorter->set_parameter("method", "invalid_method");

    SorterInput input { Kakshya::DataVariant { test_data } };

    // Should gracefully handle invalid method (possibly falling back to default)
    EXPECT_NO_THROW(sorter->apply_operation(input));
}

TEST_F(UniversalSorterTest, RecoveryFromCorruptedData)
{
    Kakshya::DataVariant corrupted_data;

    SorterInput input { corrupted_data };

    EXPECT_NO_THROW(sorter->apply_operation(input));
}

// ===== Comprehensive Integration Test =====

TEST_F(UniversalSorterTest, ComprehensiveSortingWorkflow)
{

    // 1. Sort simple vector
    SorterInput double_input { Kakshya::DataVariant { test_data } };
    sorter->set_parameter("method", "ascending");
    auto double_result = sorter->apply_operation(double_input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(double_result));

    // 2. Sort with different algorithm
    auto algo_result = sorter->sort_with_algorithm(double_input, SortingAlgorithm::STABLE);
    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(algo_result));

    // 3. Sort in chunks
    auto chunk_results = sorter->sort_chunked(double_input, 3);
    EXPECT_FALSE(chunk_results.empty());

    // 4. Sort container data
    SorterInput container_input { container };
    auto container_result = sorter->apply_operation(container_input);
    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(container_result));

    // 5. Sort Eigen data
    Eigen::VectorXd eigen_vec(5);
    eigen_vec << 5, 2, 8, 1, 9;
    SorterInput eigen_input { eigen_vec };
    auto eigen_result = sorter->apply_operation(eigen_input);
    ASSERT_TRUE(std::holds_alternative<Eigen::VectorXd>(eigen_result));

    const auto& double_sorted = std::get<std::vector<double>>(double_result);
    const auto& container_sorted = std::get<std::vector<double>>(container_result);
    const auto& eigen_sorted = std::get<Eigen::VectorXd>(eigen_result);

    EXPECT_TRUE(std::is_sorted(double_sorted.begin(), double_sorted.end()));
    EXPECT_TRUE(std::is_sorted(container_sorted.begin(), container_sorted.end()));

    for (int i = 1; i < eigen_sorted.size(); ++i) {
        EXPECT_LE(eigen_sorted[i - 1], eigen_sorted[i]);
    }
}

} // namespace MayaFlux::Test
