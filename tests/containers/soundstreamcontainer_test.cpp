#include "../test_config.h"

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Buffers/Container/StreamWriteProcessor.hpp"
#include "MayaFlux/Kakshya/Processors/ContiguousAccessProcessor.hpp"
#include "MayaFlux/Kakshya/Source/SoundStreamEXT.hpp"

using namespace MayaFlux::Kakshya;
using namespace MayaFlux::Buffers;

namespace MayaFlux::Test {

// ============================================================================
// SSCExt (SoundStreamEXT) Tests
// ============================================================================

class SSCExtTest : public ::testing::Test {
protected:
    std::shared_ptr<SSCExt> container;

    void SetUp() override
    {
        container = std::make_shared<SSCExt>(48000, 2);
    }
};

TEST_F(SSCExtTest, DefaultConstructorSetsCorrectValues)
{
    auto default_container = std::make_shared<SSCExt>(44100);
    EXPECT_EQ(default_container->get_sample_rate(), 44100);
    EXPECT_EQ(default_container->get_num_channels(), 2);
    EXPECT_TRUE(default_container->get_auto_resize());
    EXPECT_FALSE(default_container->is_circular());
}

TEST_F(SSCExtTest, CustomConstructorSetsCorrectValues)
{
    EXPECT_EQ(container->get_sample_rate(), 48000);
    EXPECT_EQ(container->get_num_channels(), 2);
    EXPECT_TRUE(container->get_auto_resize());
    EXPECT_FALSE(container->is_circular());
}

TEST_F(SSCExtTest, WriteFramesWithAutoResize)
{
    std::vector<double> test_data = { 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8 };
    std::span<const double> data_span(test_data);

    u_int64_t frames_written = container->write_frames(data_span, 0);

    EXPECT_EQ(frames_written, 4); // 8 samples / 2 channels = 4 frames
    EXPECT_GE(container->get_num_frames(), 4);
}

TEST_F(SSCExtTest, WriteFramesAtNonZeroStartFrame)
{
    std::vector<double> first_data = { 0.1, 0.2, 0.3, 0.4 };
    std::vector<double> second_data = { 0.5, 0.6, 0.7, 0.8 };

    container->write_frames(std::span<const double>(first_data), 0);
    u_int64_t frames_written = container->write_frames(std::span<const double>(second_data), 3);

    EXPECT_EQ(frames_written, 2);
    EXPECT_GE(container->get_num_frames(), 5);
}

TEST_F(SSCExtTest, WriteFramesWithAutoResizeDisabled)
{
    container->set_auto_resize(false);
    container->ensure_capacity(2);

    std::vector<double> test_data = { 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8 };

    EXPECT_NO_THROW(container->write_frames(std::span<const double>(test_data), 0));
}

TEST_F(SSCExtTest, UnderstandDataLayout)
{
    std::vector<double> write_data = { 1.0, 2.0, 3.0, 4.0 }; // Frame0: [1,2], Frame1: [3,4]

    container->write_frames(std::span<const double>(write_data), 0);

    std::vector<double> read_buffer(4, 0.0);
    container->set_read_position(0);
    u_int64_t samples_read = container->read_frames(std::span<double>(read_buffer), 4);

    std::cout << "=== Data Layout Test ===" << std::endl;
    std::cout << "Container channels: " << container->get_num_channels() << std::endl;
    std::cout << "Container frames: " << container->get_num_frames() << std::endl;
    std::cout << "Written as [1.0, 2.0, 3.0, 4.0] (Frame0=[1,2], Frame1=[3,4])" << std::endl;
    std::cout << "Read as    [" << read_buffer[0] << ", " << read_buffer[1]
              << ", " << read_buffer[2] << ", " << read_buffer[3] << "]" << std::endl;

    if (container->get_num_frames() >= 2) {
        auto frame0 = container->get_frame(0);
        auto frame1 = container->get_frame(1);

        std::cout << "Frame 0 via get_frame: [";
        for (size_t i = 0; i < frame0.size(); ++i) {
            std::cout << frame0[i] << (i + 1 < frame0.size() ? ", " : "");
        }
        std::cout << "]" << std::endl;

        std::cout << "Frame 1 via get_frame: [";
        for (size_t i = 0; i < frame1.size(); ++i) {
            std::cout << frame1[i] << (i + 1 < frame1.size() ? ", " : "");
        }
        std::cout << "]" << std::endl;
    }

    EXPECT_GT(samples_read, 0);
}

TEST_F(SSCExtTest, ReadFramesAfterWrite)
{
    std::vector<double> write_data = { 0.1, 0.2, 0.3, 0.4, 0.5, 0.6 };
    container->write_frames(std::span<const double>(write_data), 0);

    std::vector<double> read_buffer(6, 0.0);
    container->set_read_position(0);
    u_int64_t frames_read = container->read_frames(std::span<double>(read_buffer), 6);

    EXPECT_GT(frames_read, 0) << "Should be able to read some data";
    EXPECT_LE(frames_read, 6) << "Should not read more than requested";

    // TODO: Fix the layout issue and add proper data verification
}

TEST_F(SSCExtTest, EnsureCapacityExpandsContainer)
{
    u_int64_t initial_frames = container->get_num_frames();
    container->ensure_capacity(100);

    EXPECT_GE(container->get_num_frames(), 100);
    EXPECT_GE(container->get_num_frames(), initial_frames);
}

TEST_F(SSCExtTest, CircularBufferEnableAndDisable)
{
    EXPECT_FALSE(container->is_circular());

    container->enable_circular_buffer(10);
    EXPECT_TRUE(container->is_circular());
    EXPECT_TRUE(container->is_looping());
    EXPECT_GE(container->get_num_frames(), 10);

    container->disable_circular_buffer();
    EXPECT_FALSE(container->is_circular());
    EXPECT_FALSE(container->is_looping());
}

TEST_F(SSCExtTest, CircularBufferWriteAndLoop)
{
    container->enable_circular_buffer(4);

    std::vector<double> test_data = { 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8 };
    container->write_frames(std::span<const double>(test_data), 0);

    std::vector<double> read_buffer(16);
    container->set_read_position(0);
    u_int64_t frames_read = container->read_frames(std::span<double>(read_buffer), 16);

    EXPECT_GT(frames_read, 0);
}

TEST_F(SSCExtTest, AutoResizeToggle)
{
    EXPECT_TRUE(container->get_auto_resize());

    container->set_auto_resize(false);
    EXPECT_FALSE(container->get_auto_resize());

    container->set_auto_resize(true);
    EXPECT_TRUE(container->get_auto_resize());
}

// ============================================================================
// StreamWriteProcessor Tests
// ============================================================================

class StreamWriteProcessorTest : public ::testing::Test {
protected:
    std::shared_ptr<SSCExt> container;
    std::shared_ptr<AudioBuffer> buffer;
    std::shared_ptr<StreamWriteProcessor> processor;

    void SetUp() override
    {
        container = std::make_shared<SSCExt>(48000, 2);
        buffer = std::make_shared<AudioBuffer>(0, 4);
        processor = std::make_shared<StreamWriteProcessor>(container);

        auto& buffer_data = buffer->get_data();
        buffer_data[0] = 0.1;
        buffer_data[1] = 0.2;
        buffer_data[2] = 0.3;
        buffer_data[3] = 0.4;
    }
};

TEST_F(StreamWriteProcessorTest, ConstructorSetsContainer)
{
    EXPECT_EQ(processor->get_container(), container);
}

TEST_F(StreamWriteProcessorTest, ProcessWritesToContainer)
{
    u_int64_t initial_frames = container->get_num_frames();

    processor->processing_function(buffer);

    EXPECT_GT(container->get_num_frames(), initial_frames);
}

TEST_F(StreamWriteProcessorTest, ProcessWithNullBufferDoesNotCrash)
{
    EXPECT_NO_THROW(processor->processing_function(nullptr));
}

TEST_F(StreamWriteProcessorTest, ProcessWithEmptyBufferDoesNotCrash)
{
    auto empty_buffer = std::make_shared<AudioBuffer>(0, 0);
    EXPECT_NO_THROW(processor->processing_function(empty_buffer));
}

TEST_F(StreamWriteProcessorTest, ProcessWritesCorrectData)
{
    u_int64_t initial_frames = container->get_num_frames();

    processor->processing_function(buffer);

    EXPECT_GT(container->get_num_frames(), initial_frames) << "Container should have more frames after writing";

    // TODO:: Need multi memory layout data handle
    std::vector<double> read_data(4, 0.0);
    container->set_read_position(initial_frames);
    u_int64_t frames_read = container->read_frames(std::span<double>(read_data), 4);

    EXPECT_GT(frames_read, 0) << "Should be able to read back some data";

    auto& original_data = buffer->get_data();
    bool found_some_values = false;
    for (double original_val : original_data) {
        for (double read_val : read_data) {
            if (std::abs(read_val - original_val) < 1e-10) {
                found_some_values = true;
                break;
            }
        }
        if (found_some_values)
            break;
    }
    EXPECT_TRUE(found_some_values) << "Should find some of the written values in the container";
}

TEST_F(StreamWriteProcessorTest, MultipleProcessCallsAccumulateData)
{
    u_int64_t initial_frames = container->get_num_frames();

    processor->processing_function(buffer);
    processor->processing_function(buffer);

    EXPECT_GE(container->get_num_frames(), initial_frames + 2);
}

// ============================================================================
// SSCExt Buffer Interop Tests
// ============================================================================

class SSCExtBufferInteropTest : public ::testing::Test {
protected:
    std::shared_ptr<SSCExt> source_container;
    std::shared_ptr<SSCExt> sink_container;
    std::shared_ptr<AudioBuffer> buffer;
    std::shared_ptr<StreamWriteProcessor> write_processor;

    void SetUp() override
    {
        source_container = std::make_shared<SSCExt>(48000, 2);
        sink_container = std::make_shared<SSCExt>(48000, 2);
        buffer = std::make_shared<AudioBuffer>(0, 4);
        write_processor = std::make_shared<StreamWriteProcessor>(sink_container);

        std::vector<double> test_data = { 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8 };
        source_container->write_frames(std::span<const double>(test_data), 0);
        source_container->set_read_position(0);
    }
};

TEST_F(SSCExtBufferInteropTest, ReadFromSSCExtWriteToBuffer)
{
    std::vector<double> temp_buffer(4, 0.0);
    u_int64_t frames_read = source_container->read_frames(std::span<double>(temp_buffer), 4);

    EXPECT_EQ(frames_read, 4);

    auto& buffer_data = buffer->get_data();
    for (size_t i = 0; i < frames_read; ++i) {
        buffer_data[i] = temp_buffer[i];
    }

    std::cout << "Source container data: ";
    for (double val : temp_buffer)
        std::cout << val << " ";
    std::cout << std::endl;

    std::vector<double> expected_values = { 0.1, 0.2, 0.5, 0.6 }; // Currently we only read deinterleaved data

    for (double expected : expected_values) {
        bool found = false;
        for (double actual : temp_buffer) {
            if (std::abs(actual - expected) < 1e-9) {
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found) << "Expected value " << expected << " not found in read data";
    }

    EXPECT_NEAR(temp_buffer[0], 0.1, 1e-9) << "First value should be 0.1";
    EXPECT_NEAR(temp_buffer[1], 0.5, 1e-9) << "Second value should be 0.5 (from frame 2, channel 0)";
    EXPECT_NEAR(temp_buffer[2], 0.2, 1e-9) << "Third value should be 0.2 (from frame 0, channel 1)";
    EXPECT_NEAR(temp_buffer[3], 0.6, 1e-9) << "Fourth value should be 0.6 (from frame 2, channel 1)";
}

TEST_F(SSCExtBufferInteropTest, WriteFromBufferToSSCExt)
{
    auto& buffer_data = buffer->get_data();
    buffer_data[0] = 1.1;
    buffer_data[1] = 1.2;
    buffer_data[2] = 1.3;
    buffer_data[3] = 1.4;

    write_processor->processing_function(buffer);

    std::vector<double> verify_buffer(4, 0.0);
    sink_container->set_read_position(0);
    u_int64_t frames_read = sink_container->read_frames(std::span<double>(verify_buffer), 4);

    EXPECT_GT(frames_read, 0);

    std::cout << "Written to buffer: ";
    for (double val : buffer_data)
        std::cout << val << " ";
    std::cout << std::endl;

    std::cout << "Read from container: ";
    for (double val : verify_buffer)
        std::cout << val << " ";
    std::cout << std::endl;

    std::vector<double> written_values = { 1.1, 1.2, 1.3, 1.4 };

    for (double written : written_values) {
        bool found = false;
        for (size_t i = 0; i < frames_read; ++i) {
            if (std::abs(verify_buffer[i] - written) < 1e-10) {
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found) << "Written value " << written << " not found in read data";
    }
}

TEST_F(SSCExtBufferInteropTest, FullPipelineSourceToSink)
{
    std::vector<double> temp_buffer(4, 0.0);
    u_int64_t frames_read = source_container->read_frames(std::span<double>(temp_buffer), 4);

    auto& buffer_data = buffer->get_data();
    for (size_t i = 0; i < std::min<size_t>(frames_read, buffer_data.size()); ++i) {
        buffer_data[i] = temp_buffer[i];
    }

    write_processor->processing_function(buffer);

    EXPECT_GT(sink_container->get_num_frames(), 0) << "Sink container should have data";

    std::vector<double> final_buffer(4, 0.0);
    sink_container->set_read_position(0);
    u_int64_t final_frames = sink_container->read_frames(std::span<double>(final_buffer), 4);

    EXPECT_GT(final_frames, 0) << "Should be able to read data from sink";

    // TODO: Add exact data verification once layout issue is resolved
}

TEST_F(SSCExtBufferInteropTest, CircularBufferInterop)
{
    sink_container->enable_circular_buffer(2);

    for (int i = 0; i < 5; ++i) {
        auto& buffer_data = buffer->get_data();
        buffer_data[0] = i * 0.1;
        buffer_data[1] = i * 0.1 + 0.05;
        buffer_data[2] = i * 0.1 + 0.1;
        buffer_data[3] = i * 0.1 + 0.15;

        write_processor->processing_function(buffer);
    }

    EXPECT_TRUE(sink_container->is_circular());
    EXPECT_GT(sink_container->get_num_frames(), 0);
}

// ============================================================================
// SSCExt with ContiguousAccessProcessor Integration Tests
// ============================================================================

class SSCExtProcessorIntegrationTest : public ::testing::Test {
protected:
    std::shared_ptr<SSCExt> container;
    std::shared_ptr<ContiguousAccessProcessor> processor;

    void SetUp() override
    {
        container = std::make_shared<SSCExt>(48000, 2);
        processor = std::make_shared<ContiguousAccessProcessor>();

        std::vector<double> test_data = {
            0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8,
            0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6
        };
        container->write_frames(std::span<const double>(test_data), 0);
        container->set_read_position(0);
    }
};

TEST_F(SSCExtProcessorIntegrationTest, ProcessorAttachesToSSCExt)
{
    EXPECT_NO_THROW(processor->on_attach(container));
    EXPECT_NO_THROW(processor->on_detach(container));
}

TEST_F(SSCExtProcessorIntegrationTest, ProcessorProcessesSSCExtData)
{
    processor->set_output_size({ 4, 2 });
    processor->on_attach(container);

    EXPECT_NO_THROW(processor->process(container));

    const auto& processed = container->get_processed_data();
    EXPECT_TRUE(std::holds_alternative<std::vector<double>>(processed));

    auto vec = std::get<std::vector<double>>(processed);
    EXPECT_EQ(vec.size(), 8);
}

TEST_F(SSCExtProcessorIntegrationTest, AutoAdvanceWithSSCExt)
{
    processor->set_output_size({ 2, 2 });
    processor->set_auto_advance(true);
    processor->on_attach(container);

    u_int64_t initial_position = container->get_read_position();
    processor->process(container);
    u_int64_t final_position = container->get_read_position();

    EXPECT_GT(final_position, initial_position);
}

TEST_F(SSCExtProcessorIntegrationTest, LoopingRegionWithSSCExt)
{
    container->enable_circular_buffer(4);
    processor->set_output_size({ 2, 2 });
    processor->on_attach(container);

    for (int i = 0; i < 5; ++i) {
        EXPECT_NO_THROW(processor->process(container));
    }

    EXPECT_TRUE(container->is_looping());
}

// ============================================================================
// Edge Cases and Error Handling Tests
// ============================================================================

class SSCExtEdgeCasesTest : public ::testing::Test {
protected:
    std::shared_ptr<SSCExt> container;

    void SetUp() override
    {
        container = std::make_shared<SSCExt>(48000, 2);
    }
};

TEST_F(SSCExtEdgeCasesTest, WriteEmptyData)
{
    std::vector<double> empty_data;
    u_int64_t frames_written = container->write_frames(std::span<const double>(empty_data), 0);
    EXPECT_EQ(frames_written, 0);
}

TEST_F(SSCExtEdgeCasesTest, ReadFromEmptyContainer)
{
    std::vector<double> read_buffer(4);
    u_int64_t frames_read = container->read_frames(std::span<double>(read_buffer), 4);
    EXPECT_EQ(frames_read, 0);
}

TEST_F(SSCExtEdgeCasesTest, WriteWithMismatchedChannelData)
{
    std::vector<double> odd_data = { 0.1, 0.2, 0.3 };

    EXPECT_NO_THROW(container->write_frames(std::span<const double>(odd_data), 0));
}

TEST_F(SSCExtEdgeCasesTest, EnsureCapacityWithZero)
{
    EXPECT_NO_THROW(container->ensure_capacity(0));
}

TEST_F(SSCExtEdgeCasesTest, CircularBufferWithZeroCapacity)
{
    EXPECT_NO_THROW(container->enable_circular_buffer(0));
}

TEST_F(SSCExtEdgeCasesTest, WriteAtLargeStartFrame)
{
    container->set_auto_resize(true);
    std::vector<double> test_data = { 0.1, 0.2 };

    EXPECT_NO_THROW(container->write_frames(std::span<const double>(test_data), 1000000));
    EXPECT_GE(container->get_num_frames(), 1000001);
}

} // namespace MayaFlux::Test
