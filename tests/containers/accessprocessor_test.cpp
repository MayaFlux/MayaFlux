#include "../test_config.h"
#include "MayaFlux/Kakshya/Processors/ContiguousAccessProcessor.hpp"
#include "MayaFlux/Kakshya/Source/SoundFileContainer.hpp"

using namespace MayaFlux::Kakshya;

namespace MayaFlux::Test {

class ContiguousAccessProcessorTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_data = { 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8 };
        container = std::make_shared<SoundFileContainer>();
        container->setup(4, 48000, 2);
        container->get_structure().organization = OrganizationStrategy::INTERLEAVED;
        container->set_raw_data({ test_data });

        processor = std::make_shared<ContiguousAccessProcessor>();
    }

    std::vector<double> test_data;
    std::shared_ptr<SoundFileContainer> container;
    std::shared_ptr<ContiguousAccessProcessor> processor;
};

TEST_F(ContiguousAccessProcessorTest, AttachAndDetachDoesNotThrow)
{
    EXPECT_NO_THROW(processor->on_attach(container));
    EXPECT_NO_THROW(processor->on_detach(container));
}

TEST_F(ContiguousAccessProcessorTest, ProcessWritesToProcessedData)
{
    processor->on_attach(container);

    processor->set_output_size({ 4, 2 });
    processor->process(container);

    const std::vector<DataVariant>& processed = container->get_processed_data();

    EXPECT_FALSE(processed.empty()) << "Processed data should not be empty";

    if (!processed.empty()) {
        EXPECT_TRUE(std::holds_alternative<std::vector<double>>(processed[0]));
        auto vec = std::get<std::vector<double>>(processed[0]);
        EXPECT_EQ(vec.size(), 8); // 4 frames * 2 channels
    }
}

TEST_F(ContiguousAccessProcessorTest, ProcessWithDifferentOutputSize)
{
    processor->on_attach(container);

    processor->set_output_size({ 2, 2 });
    processor->process(container);

    const std::vector<DataVariant>& processed = container->get_processed_data();

    EXPECT_FALSE(processed.empty()) << "Processed data should not be empty";

    if (!processed.empty()) {
        EXPECT_TRUE(std::holds_alternative<std::vector<double>>(processed[0]));
        auto vec = std::get<std::vector<double>>(processed[0]);
        EXPECT_EQ(vec.size(), 4); // 2 frames * 2 channels
    }
}

TEST_F(ContiguousAccessProcessorTest, IsProcessingReflectsState)
{
    processor->on_attach(container);
    processor->set_output_size({ 2, 2 });
    EXPECT_FALSE(processor->is_processing());
    processor->process(container);
    EXPECT_FALSE(processor->is_processing());
}

TEST_F(ContiguousAccessProcessorTest, ProcessAfterDetachDoesNotThrow)
{
    processor->on_attach(container);
    processor->on_detach(container);
    EXPECT_NO_THROW(processor->process(container));
}

TEST_F(ContiguousAccessProcessorTest, SetAutoAdvanceDoesNotThrow)
{
    processor->on_attach(container);
    EXPECT_NO_THROW(processor->set_auto_advance(false));
}

TEST_F(ContiguousAccessProcessorTest, OutputShapeLargerThanContainerThrows)
{
    processor->set_output_size({ 5, 2 }); // 5 frames, but container only has 4
    EXPECT_THROW(processor->on_attach(container), std::runtime_error);
}

TEST_F(ContiguousAccessProcessorTest, OutputShapeWrongRankThrows)
{
    processor->set_output_size({ 4 }); // Only 1 dimension, but audio expects 2D
    EXPECT_THROW(processor->on_attach(container), std::runtime_error);
}

TEST_F(ContiguousAccessProcessorTest, ZeroOutputShapeThrows)
{
    processor->set_output_size({ 0, 2 });
    EXPECT_THROW(processor->on_attach(container), std::runtime_error);
}

TEST_F(ContiguousAccessProcessorTest, ProcessWithPartialRegion)
{
    processor->set_output_size({ 2, 1 }); // 2 frames, 1 channel
    processor->on_attach(container);
    processor->process(container);

    const std::vector<DataVariant>& processed = container->get_processed_data();

    EXPECT_FALSE(processed.empty()) << "Processed data should not be empty";

    if (!processed.empty()) {
        EXPECT_TRUE(std::holds_alternative<std::vector<double>>(processed[0]));
        auto vec = std::get<std::vector<double>>(processed[0]);
        EXPECT_EQ(vec.size(), 2); // 2 frames * 1 channel

        // For interleaved data [0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8]
        // 2 frames, 1 channel (channel 0) should give us frames 0 and 1 of channel 0
        EXPECT_DOUBLE_EQ(vec[0], 0.1); // Frame 0, channel 0
        EXPECT_DOUBLE_EQ(vec[1], 0.3); // Frame 1, channel 0
    }
}

TEST_F(ContiguousAccessProcessorTest, ProcessMultipleTimesAdvancesPosition)
{
    processor->set_output_size({ 2, 2 });
    processor->set_auto_advance(true);
    processor->on_attach(container);

    processor->process(container);
    const auto& processed_data1 = container->get_processed_data();
    EXPECT_FALSE(processed_data1.empty());
    auto first = std::get<std::vector<double>>(processed_data1[0]);

    processor->process(container);
    const auto& processed_data2 = container->get_processed_data();
    EXPECT_FALSE(processed_data2.empty());
    auto second = std::get<std::vector<double>>(processed_data2[0]);

    ASSERT_EQ(first.size(), 4); // 2 frames * 2 channels
    ASSERT_EQ(second.size(), 4); // 2 frames * 2 channels
    EXPECT_NE(first, second);

    // Based on actual processor behavior - appears to extract channels sequentially
    // For interleaved data [0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8]
    // First processing: 2 frames, 2 channels -> [0.1, 0.3, 0.2, 0.4]
    EXPECT_DOUBLE_EQ(first[0], 0.1); // Frame 0, channel 0
    EXPECT_DOUBLE_EQ(first[1], 0.2); // Frame 0, channel 1
    EXPECT_DOUBLE_EQ(first[2], 0.3); // Frame 1, channel 0
    EXPECT_DOUBLE_EQ(first[3], 0.4); // Frame 1, channel 1

    // Second processing: frames 2-3 -> [0.5, 0.7, 0.6, 0.8]
    EXPECT_DOUBLE_EQ(second[0], 0.5); // Frame 2, channel 0
    EXPECT_DOUBLE_EQ(second[1], 0.6); // Frame 2, channel 1
    EXPECT_DOUBLE_EQ(second[2], 0.7); // Frame 3, channel 0
    EXPECT_DOUBLE_EQ(second[3], 0.8); // Frame 3, channel 1
}

TEST_F(ContiguousAccessProcessorTest, ProcessWithLoopingRegion)
{
    processor->set_output_size({ 2, 2 });
    processor->on_attach(container);

    container->set_looping(true);
    container->set_loop_region(Region(std::vector<uint64_t>({ 1, 0 }), std::vector<uint64_t>({ 2, 1 })));

    processor->process(container);
    const auto& processed_data = container->get_processed_data();
    EXPECT_FALSE(processed_data.empty());

    if (!processed_data.empty()) {
        auto vec = std::get<std::vector<double>>(processed_data[0]);
        EXPECT_EQ(vec.size(), 4); // 2 frames * 2 channels

        EXPECT_DOUBLE_EQ(vec[0], 0.1); // Frame 0, channel 0
        EXPECT_DOUBLE_EQ(vec[1], 0.2); // Frame 0, channel 1
        EXPECT_DOUBLE_EQ(vec[2], 0.3); // Frame 1, channel 0
        EXPECT_DOUBLE_EQ(vec[3], 0.4); // Frame 1, channel 1
    }
}

TEST_F(ContiguousAccessProcessorTest, ProcessWithPlanarOrganization)
{
    auto& structure = container->get_structure();
    structure.organization = OrganizationStrategy::PLANAR;
    container->set_structure(structure);

    std::vector<DataVariant> planar_data;
    planar_data.emplace_back(std::vector<double> { 0.1, 0.3, 0.5, 0.7 });
    planar_data.emplace_back(std::vector<double> { 0.2, 0.4, 0.6, 0.8 });
    container->set_raw_data(planar_data);

    processor->set_output_size({ 2, 2 });
    processor->on_attach(container);
    processor->process(container);

    const auto& processed = container->get_processed_data();
    EXPECT_FALSE(processed.empty());

    if (structure.organization == OrganizationStrategy::PLANAR) {
        EXPECT_EQ(processed.size(), 2);

        auto left_channel = std::get<std::vector<double>>(processed[0]);
        auto right_channel = std::get<std::vector<double>>(processed[1]);

        EXPECT_EQ(left_channel.size(), 2);
        EXPECT_EQ(right_channel.size(), 2);

        EXPECT_DOUBLE_EQ(left_channel[0], 0.1);
        EXPECT_DOUBLE_EQ(left_channel[1], 0.3);
        EXPECT_DOUBLE_EQ(right_channel[0], 0.2);
        EXPECT_DOUBLE_EQ(right_channel[1], 0.4);
    }
}

} // namespace MayaFlux::Test
