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
        container->set_raw_data(test_data);

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

    const DataVariant& processed = container->get_processed_data();

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(processed));
    auto vec = std::get<std::vector<double>>(processed);
    ASSERT_EQ(vec.size(), 8);
}

TEST_F(ContiguousAccessProcessorTest, ProcessWithDifferentOutputSize)
{
    processor->on_attach(container);

    processor->set_output_size({ 4, 2 });
    processor->process(container);

    const DataVariant& processed = container->get_processed_data();
    auto vec = std::get<std::vector<double>>(processed);
    ASSERT_EQ(vec.size(), 8);
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

TEST_F(ContiguousAccessProcessorTest, SetActiveDimensionsDoesNotThrow)
{
    processor->on_attach(container);
    EXPECT_NO_THROW(processor->set_active_dimensions({ 0, 1 }));
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
    processor->set_output_size({ 4 }); // Only 1 dimension, but container is 2D
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

    const DataVariant& processed = container->get_processed_data();
    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(processed));
    auto vec = std::get<std::vector<double>>(processed);
    ASSERT_EQ(vec.size(), 2);
    EXPECT_DOUBLE_EQ(vec[0], 0.1);
    EXPECT_DOUBLE_EQ(vec[1], 0.3);
}

TEST_F(ContiguousAccessProcessorTest, ProcessMultipleTimesAdvancesPosition)
{
    processor->set_output_size({ 2, 2 });
    processor->set_auto_advance(true);
    processor->on_attach(container);

    processor->process(container);
    auto first = std::get<std::vector<double>>(container->get_processed_data());

    processor->process(container);
    auto second = std::get<std::vector<double>>(container->get_processed_data());

    ASSERT_EQ(first.size(), 4);
    ASSERT_EQ(second.size(), 4);
    EXPECT_NE(first, second);

    // first: [0.1, 0.3, 0.2, 0.4] (frames 0-1, channels 0-1)
    // second: [0.5, 0.7, 0.6, 0.8] (frames 2-3, channels 0-1)
    EXPECT_DOUBLE_EQ(second[0], 0.5);
    EXPECT_DOUBLE_EQ(second[1], 0.7);
    EXPECT_DOUBLE_EQ(second[2], 0.6);
    EXPECT_DOUBLE_EQ(second[3], 0.8);
}

TEST_F(ContiguousAccessProcessorTest, ProcessWithLoopingRegion)
{
    processor->set_output_size({ 2, 2 });
    processor->on_attach(container);

    container->set_looping(true);
    container->set_loop_region(RegionPoint(std::vector<u_int64_t>({ 1, 0 }), std::vector<u_int64_t>({ 2, 1 })));

    processor->set_current_position({ 1, 0 });

    processor->process(container);
    const DataVariant& processed = container->get_processed_data();
    auto vec = std::get<std::vector<double>>(processed);
    ASSERT_EQ(vec.size(), 4);
    EXPECT_DOUBLE_EQ(vec[0], 0.3);
    EXPECT_DOUBLE_EQ(vec[1], 0.5);
    EXPECT_DOUBLE_EQ(vec[2], 0.4);
    EXPECT_DOUBLE_EQ(vec[3], 0.6);
}

} // namespace MayaFlux::Test
