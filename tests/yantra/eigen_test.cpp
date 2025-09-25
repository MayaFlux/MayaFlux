#include "../mock_signalsourcecontainer.hpp"

#include "MayaFlux/Yantra/Analyzers/StatisticalAnalyzer.hpp"
#include "MayaFlux/Yantra/Transformers/helpers/MatrixHelper.hpp"

using namespace MayaFlux::Yantra;
using namespace MayaFlux::Kakshya;

namespace MayaFlux::Test {

class MatrixHelperTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_data = { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0 };

        multi_channel_data = {
            Kakshya::DataVariant { std::vector<double> { 1.0, 2.0, 3.0, 4.0 } },
            Kakshya::DataVariant { std::vector<double> { 5.0, 6.0, 7.0, 8.0 } }
        };

        container = std::make_shared<MockSignalSourceContainer>();
        container->set_test_data(test_data);

        Region r1 { { 0 }, { 4 }, {} };
        Region r2 { { 4 }, { 8 }, {} };
        test_regions = { r1, r2 };

        transformed_matrix = Eigen::MatrixXd::Identity(2, 2);

        scaling_matrix = Eigen::MatrixXd::Zero(2, 2);
        scaling_matrix(0, 0) = 2.0;
        scaling_matrix(1, 1) = 0.5;
    }

    std::vector<double> test_data;
    std::vector<Kakshya::DataVariant> multi_channel_data;
    std::shared_ptr<MockSignalSourceContainer> container;
    std::vector<Region> test_regions;
    Eigen::MatrixXd transformed_matrix;
    Eigen::MatrixXd scaling_matrix;
};

TEST_F(MatrixHelperTest, BasicMatrixTransformInPlace)
{
    auto data_copy = multi_channel_data;
    auto result = Yantra::transform_matrix(data_copy, transformed_matrix);

    ASSERT_EQ(result.size(), 2);

    const auto& channel1 = std::get<std::vector<double>>(result[0]);
    const auto& channel2 = std::get<std::vector<double>>(result[1]);

    EXPECT_EQ(channel1.size(), 4);
    EXPECT_EQ(channel2.size(), 4);
}

TEST_F(MatrixHelperTest, BasicMatrixTransformOutOfPlace)
{
    auto data_copy = multi_channel_data;
    std::vector<std::vector<double>> working_buffer;

    auto result = Yantra::transform_matrix(data_copy, transformed_matrix, working_buffer);

    ASSERT_EQ(result.size(), 2);
    EXPECT_FALSE(working_buffer.empty());
}

TEST_F(MatrixHelperTest, MultiChannelMatrixTransformInPlace)
{
    auto data_copy = multi_channel_data;
    auto result = transform_matrix_multichannel(data_copy, scaling_matrix, 2);

    ASSERT_EQ(result.size(), 2);

    const auto& channel1 = std::get<std::vector<double>>(result[0]);
    const auto& channel2 = std::get<std::vector<double>>(result[1]);

    EXPECT_GT(channel1[0], 1.0);
    EXPECT_LT(channel2[0], 5.0);
}

TEST_F(MatrixHelperTest, MultiChannelMatrixTransformOutOfPlace)
{
    auto data_copy = multi_channel_data;
    std::vector<std::vector<double>> working_buffer;

    auto result = transform_matrix_multichannel(data_copy, scaling_matrix, 2, working_buffer);

    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(working_buffer.size(), 2);
}

TEST_F(MatrixHelperTest, EnergyBasedTransformInPlace)
{
    auto data_copy = multi_channel_data;
    auto transform_func = [](double x) { return x * 2.0; };

    auto result = transform_by_energy(data_copy, 0.1, transform_func, 4, 2);

    ASSERT_EQ(result.size(), 2);

    const auto& channel1 = std::get<std::vector<double>>(result[0]);
    const auto& channel2 = std::get<std::vector<double>>(result[1]);

    EXPECT_EQ(channel1.size(), 4);
    EXPECT_EQ(channel2.size(), 4);
}

TEST_F(MatrixHelperTest, EnergyBasedTransformOutOfPlace)
{
    auto data_copy = multi_channel_data;
    std::vector<std::vector<double>> working_buffer;
    auto transform_func = [](double x) { return x * 0.5; };

    auto result = transform_by_energy(data_copy, 0.1, transform_func, 4, 2, working_buffer);

    ASSERT_EQ(result.size(), 2);
    EXPECT_FALSE(working_buffer.empty());
}

TEST_F(MatrixHelperTest, OutlierTransformInPlace)
{
    std::vector<Kakshya::DataVariant> outlier_data = {
        Kakshya::DataVariant { std::vector<double> { 1.0, 2.0, 100.0, 3.0 } },
        Kakshya::DataVariant { std::vector<double> { 4.0, 5.0, 6.0, 7.0 } }
    };

    auto transform_func = [](double x) { return 0.0; };
    auto result = transform_outliers(outlier_data, 2.0, transform_func);

    ASSERT_EQ(result.size(), 2);

    const auto& channel1 = std::get<std::vector<double>>(result[0]);

    bool has_zero = std::ranges::any_of(channel1, [](double x) { return x == 0.0; });
    EXPECT_TRUE(has_zero);
}

TEST_F(MatrixHelperTest, OutlierTransformOutOfPlace)
{
    std::vector<Kakshya::DataVariant> outlier_data = {
        Kakshya::DataVariant { std::vector<double> { 1.0, 2.0, 100.0, 3.0 } },
        Kakshya::DataVariant { std::vector<double> { 4.0, 5.0, 6.0, 7.0 } }
    };

    std::vector<std::vector<double>> working_buffer;
    auto transform_func = [](double x) { return x * 0.1; };

    auto result = transform_outliers(outlier_data, 2.0, transform_func, working_buffer);

    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(working_buffer.size(), 2);
}

TEST_F(MatrixHelperTest, RegionTransformInPlace)
{
    auto data_copy = multi_channel_data;
    auto transform_func = [](const std::vector<Kakshya::DataVariant>& data) {
        return data;
    };

    EXPECT_NO_THROW({
        auto result = transform_regions(data_copy, container, test_regions, transform_func);
    });
}

TEST_F(MatrixHelperTest, RegionTransformOutOfPlace)
{
    auto data_copy = multi_channel_data;
    std::vector<std::vector<double>> working_buffer;
    auto transform_func = [](const std::vector<Kakshya::DataVariant>& data) {
        return data;
    };

    EXPECT_NO_THROW({
        auto result = transform_regions(data_copy, container, test_regions, transform_func, working_buffer);
    });
}

TEST_F(MatrixHelperTest, CrossfadeRegionsInPlace)
{
    auto data_copy = multi_channel_data;
    std::vector<std::pair<Region, Region>> fade_pairs;

    Region fade_a { { 0 }, { 2 }, {} };
    Region fade_b { { 2 }, { 4 }, {} };
    fade_pairs.emplace_back(fade_a, fade_b);

    auto result = transform_crossfade_regions(data_copy, fade_pairs, 1);

    ASSERT_EQ(result.size(), 2);

    const auto& channel1 = std::get<std::vector<double>>(result[0]);
    EXPECT_EQ(channel1.size(), 4);
}

TEST_F(MatrixHelperTest, CrossfadeRegionsOutOfPlace)
{
    auto data_copy = multi_channel_data;
    std::vector<std::vector<double>> working_buffer;
    std::vector<std::pair<Region, Region>> fade_pairs;

    Region fade_a { { 0 }, { 2 }, {} };
    Region fade_b { { 2 }, { 4 }, {} };
    fade_pairs.emplace_back(fade_a, fade_b);

    auto result = transform_crossfade_regions(data_copy, fade_pairs, 1, working_buffer);

    ASSERT_EQ(result.size(), 2);
    EXPECT_FALSE(working_buffer.empty());
}

TEST_F(MatrixHelperTest, ChannelOperationsInPlace)
{
    auto data_copy = multi_channel_data;

    auto result = transform_channel_operation(data_copy, 2, true);

    ASSERT_EQ(result.size(), 2);

    const auto& channel1 = std::get<std::vector<double>>(result[0]);
    const auto& channel2 = std::get<std::vector<double>>(result[1]);

    EXPECT_EQ(channel1.size(), 4);
    EXPECT_EQ(channel2.size(), 4);
}

TEST_F(MatrixHelperTest, ChannelOperationsOutOfPlace)
{
    auto data_copy = multi_channel_data;
    std::vector<std::vector<double>> working_buffer;

    auto result = transform_channel_operation(data_copy, 2, false, working_buffer);

    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(working_buffer.size(), 2);
}

TEST_F(MatrixHelperTest, MatrixDimensionMismatch)
{
    auto data_copy = multi_channel_data;
    Eigen::MatrixXd wrong_size = Eigen::MatrixXd::Identity(3, 3);

    EXPECT_THROW({ transform_matrix_multichannel(data_copy, wrong_size, 2); }, std::invalid_argument);
}

TEST_F(MatrixHelperTest, EmptyDataHandling)
{
    std::vector<Kakshya::DataVariant> empty_data;

    EXPECT_NO_THROW({
        auto result = Yantra::transform_matrix(empty_data, transformed_matrix);
        EXPECT_TRUE(result.empty());
    });
}

TEST_F(MatrixHelperTest, UtilityMatrixCreation)
{
    auto rotation = create_rotation_matrix(M_PI / 4, 2, 2);
    EXPECT_EQ(rotation.rows(), 2);
    EXPECT_EQ(rotation.cols(), 2);

    std::vector<double> scales = { 2.0, 0.5 };
    auto scaling = create_scaling_matrix(scales);
    EXPECT_EQ(scaling.rows(), 2);
    EXPECT_EQ(scaling.cols(), 2);
    EXPECT_DOUBLE_EQ(scaling(0, 0), 2.0);
    EXPECT_DOUBLE_EQ(scaling(1, 1), 0.5);
}

TEST_F(MatrixHelperTest, DetectRegionsByEnergy)
{
    std::vector<double> spike_data(1024, 0.1);
    for (size_t i = 100; i < 200; ++i)
        spike_data[i] = 0.8;
    for (size_t i = 500; i < 600; ++i)
        spike_data[i] = 0.9;

    std::vector<Kakshya::DataVariant> spike_variants = {
        Kakshya::DataVariant { spike_data }
    };

    auto regions = detect_regions_by_energy(spike_variants, 0.5, 50, 64, 32);

    EXPECT_GE(regions.size(), 1);

    for (const auto& region : regions) {
        EXPECT_LT(region.start_coordinates[0], region.end_coordinates[0]);
    }
}

} // namespace MayaFlux::Test
