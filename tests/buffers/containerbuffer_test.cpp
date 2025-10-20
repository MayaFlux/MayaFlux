#include "../test_config.h"

#include "MayaFlux/Buffers/Container/ContainerBuffer.hpp"
#include "MayaFlux/Kakshya/Processors/ContiguousAccessProcessor.hpp"
#include "MayaFlux/Kakshya/Source/SoundFileContainer.hpp"

using namespace MayaFlux::Buffers;
using namespace MayaFlux::Kakshya;

namespace MayaFlux::Test {

class TestContainerToBufferAdapter : public ContainerToBufferAdapter {
public:
    using ContainerToBufferAdapter::ContainerToBufferAdapter;
};

class ContainerBufferTest : public ::testing::Test {
protected:
    std::shared_ptr<SoundFileContainer> container;
    std::shared_ptr<ContainerBuffer> buffer;

    void SetUp() override
    {
        container = std::make_shared<SoundFileContainer>();
        container->setup(4, 48000, 2);
        container->get_structure().organization = OrganizationStrategy::INTERLEAVED;

        std::vector<double> test_data = { 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8 };
        container->set_raw_data({ test_data });

        auto processor = std::make_shared<MayaFlux::Kakshya::ContiguousAccessProcessor>();
        std::vector<uint64_t> shape { 4, 2 };

        processor->set_auto_advance(false);
        container->set_default_processor(processor);
        container->mark_ready_for_processing(true);
        container->set_read_position({ 0, 0 });
        processor->set_output_size(shape);

        buffer = std::make_shared<ContainerBuffer>(0, 4, container, 0);
        buffer->initialize();
    }
};

TEST_F(ContainerBufferTest, AttachAndDetachDoesNotThrow)
{
    auto adapter = std::make_shared<ContainerToBufferAdapter>(container);
    EXPECT_NO_THROW(adapter->on_attach(buffer));
    EXPECT_NO_THROW(adapter->on_detach(buffer));
}

TEST_F(ContainerBufferTest, ProcessFillsBufferWithCorrectChannel)
{
    container->set_read_position({ 0, 0 });

    auto adapter = std::make_shared<ContainerToBufferAdapter>(container);
    adapter->set_source_channel(0);
    adapter->set_auto_advance(false);
    adapter->on_attach(buffer);
    adapter->processing_function(buffer);

    auto& data = buffer->get_data();
    ASSERT_EQ(data.size(), buffer->get_num_samples());

    EXPECT_DOUBLE_EQ(data[0], 0.1);
    EXPECT_DOUBLE_EQ(data[1], 0.3);
    EXPECT_DOUBLE_EQ(data[2], 0.5);
    EXPECT_DOUBLE_EQ(data[3], 0.7);
}

TEST_F(ContainerBufferTest, ProcessFillsBufferWithOtherChannel)
{
    container->set_read_position({ 0, 0 });

    auto adapter = std::make_shared<ContainerToBufferAdapter>(container);
    adapter->set_source_channel(1);
    adapter->set_auto_advance(false);
    adapter->on_attach(buffer);
    adapter->processing_function(buffer);

    auto& data = buffer->get_data();
    ASSERT_EQ(data.size(), buffer->get_num_samples());

    EXPECT_DOUBLE_EQ(data[0], 0.2);
    EXPECT_DOUBLE_EQ(data[1], 0.4);
    EXPECT_DOUBLE_EQ(data[2], 0.6);
    EXPECT_DOUBLE_EQ(data[3], 0.8);
}

TEST_F(ContainerBufferTest, ThrowsOnInvalidChannel)
{
    auto adapter = std::make_shared<ContainerToBufferAdapter>(container);
    EXPECT_THROW(adapter->set_source_channel(2), std::out_of_range);
}

TEST_F(ContainerBufferTest, ZeroCopyModeIsFalseByDefault)
{
    EXPECT_FALSE(buffer->is_zero_copy());
}

TEST_F(ContainerBufferTest, AutoAdvanceAdvancesReadPosition)
{
    container->set_read_position({ 0, 0 });

    auto adapter = std::dynamic_pointer_cast<ContainerToBufferAdapter>(buffer->get_default_processor());
    ASSERT_NE(adapter, nullptr) << "Buffer should have a ContainerToBufferAdapter as default processor";

    adapter->set_auto_advance(true);

    EXPECT_TRUE(container->has_active_readers()) << "Container should have active readers after buffer initialization";

    auto pos_before = container->get_read_position();
    std::cout << "Position before: [";
    for (size_t i = 0; i < pos_before.size(); ++i) {
        std::cout << pos_before[i] << (i + 1 < pos_before.size() ? ", " : "");
    }
    std::cout << "]" << std::endl;
    std::cout << "Has active readers: " << container->has_active_readers() << std::endl;

    adapter->processing_function(buffer);

    auto pos_after = container->get_read_position();
    std::cout << "Position after: [";
    for (size_t i = 0; i < pos_after.size(); ++i) {
        std::cout << pos_after[i] << (i + 1 < pos_after.size() ? ", " : "");
    }
    std::cout << "]" << std::endl;
    std::cout << "All dimensions consumed: " << container->all_dimensions_consumed() << std::endl;

    EXPECT_GT(pos_after[0], pos_before[0]) << "Position should advance from " << pos_before[0] << " to " << pos_after[0];
}

TEST_F(ContainerBufferTest, MultipleSequentialProcessCallsAreConsistent)
{
    container->set_read_position({ 0, 0 });

    auto adapter = std::make_shared<ContainerToBufferAdapter>(container);
    adapter->set_source_channel(0);
    adapter->set_auto_advance(false);
    adapter->on_attach(buffer);

    std::vector<std::vector<double>> results;
    for (int i = 0; i < 2; ++i) {
        adapter->processing_function(buffer);
        results.push_back(buffer->get_data());
    }

    EXPECT_DOUBLE_EQ(results[0][0], 0.1);
    EXPECT_DOUBLE_EQ(results[0][1], 0.3);
    EXPECT_DOUBLE_EQ(results[0][2], 0.5);
    EXPECT_DOUBLE_EQ(results[0][3], 0.7);

    EXPECT_DOUBLE_EQ(results[1][0], 0.1);
    EXPECT_DOUBLE_EQ(results[1][1], 0.3);
    EXPECT_DOUBLE_EQ(results[1][2], 0.5);
    EXPECT_DOUBLE_EQ(results[1][3], 0.7);
}

TEST_F(ContainerBufferTest, BufferWrapsCorrectlyWithLooping)
{
    container->set_looping(true);
    container->set_loop_region(Region(std::vector<uint64_t>({ 0, 0 }), std::vector<uint64_t>({ 3, 1 })));
    container->set_read_position({ 0, 0 });

    auto adapter = std::make_shared<ContainerToBufferAdapter>(container);
    adapter->set_source_channel(0);
    adapter->set_auto_advance(true);
    adapter->on_attach(buffer);

    std::vector<std::vector<double>> expected = {
        { 0.1, 0.3, 0.5, 0.7 },
        { 0.1, 0.3, 0.5, 0.7 },
        { 0.1, 0.3, 0.5, 0.7 },
        { 0.1, 0.3, 0.5, 0.7 },
        { 0.1, 0.3, 0.5, 0.7 }
    };

    for (int i = 0; i < 5; ++i) {
        adapter->processing_function(buffer);
        auto& data = buffer->get_data();
        std::cout << "Call " << i << ": ";
        for (auto v : data)
            std::cout << v << " ";

        auto pos = container->get_read_position();
        std::cout << " (position: [";
        for (size_t j = 0; j < pos.size(); ++j) {
            std::cout << pos[j] << (j + 1 < pos.size() ? ", " : "");
        }
        std::cout << "])" << std::endl;

        for (size_t j = 0; j < data.size(); ++j) {
            EXPECT_DOUBLE_EQ(data[j], expected[i][j])
                << "Mismatch at call " << i << ", index " << j;
        }
    }
}

TEST_F(ContainerBufferTest, PartialBufferAtEndDoesNotCrash)
{
    container->set_read_position({ 0, 0 });
    buffer = std::make_shared<ContainerBuffer>(0, 10, container, 0);
    buffer->initialize();
    auto adapter = std::make_shared<ContainerToBufferAdapter>(container);
    adapter->set_source_channel(0);
    adapter->set_auto_advance(false);
    adapter->on_attach(buffer);
    EXPECT_NO_THROW(adapter->processing_function(buffer));
    auto& data = buffer->get_data();

    EXPECT_DOUBLE_EQ(data[0], 0.1);
    EXPECT_DOUBLE_EQ(data[1], 0.3);
    EXPECT_DOUBLE_EQ(data[2], 0.5);
    EXPECT_DOUBLE_EQ(data[3], 0.7);

    for (size_t i = 4; i < data.size(); ++i)
        EXPECT_DOUBLE_EQ(data[i], 0.0);
}

} // namespace MayaFlux::Test
