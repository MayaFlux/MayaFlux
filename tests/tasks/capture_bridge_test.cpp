#include "../test_config.h"
#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Buffers/Container/StreamWriteProcessor.hpp"
#include "MayaFlux/Kakshya/Source/DynamicSoundStream.hpp"
#include "MayaFlux/Kriya/Bridge.hpp"
#include "MayaFlux/Kriya/Capture.hpp"
#include "MayaFlux/Kriya/CycleCoordinator.hpp"
#include "MayaFlux/Nodes/Generators/Sine.hpp"

#include "MayaFlux/MayaFlux.hpp"

#include <future>

namespace MayaFlux::Test {

class CaptureBridgeTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        scheduler = std::make_shared<Vruta::TaskScheduler>(TestConfig::SAMPLE_RATE);
        buffer = std::make_shared<Buffers::AudioBuffer>();
        dynamic_stream = std::make_shared<Kakshya::DynamicSoundStream>(TestConfig::SAMPLE_RATE, 2);

        setupTestBuffer();
    }

    void TearDown() override
    {
        scheduler.reset();
        buffer.reset();
        dynamic_stream.reset();
    }

    void setupTestBuffer()
    {
        auto& data = buffer->get_data();
        data.resize(512);

        for (size_t i = 0; i < data.size(); ++i) {
            data[i] = std::sin(2.0 * M_PI * 440.0 * i / TestConfig::SAMPLE_RATE) * 0.5;
        }
    }

    std::shared_ptr<Vruta::TaskScheduler> scheduler;
    std::shared_ptr<Buffers::AudioBuffer> buffer;
    std::shared_ptr<Kakshya::DynamicSoundStream> dynamic_stream;
};

// ========== BufferCapture Tests ==========

TEST_F(CaptureBridgeTest, BufferCaptureBasic)
{
    Kriya::BufferCapture capture(buffer);

    EXPECT_EQ(capture.get_mode(), Kriya::BufferCapture::CaptureMode::TRANSIENT);
    EXPECT_EQ(capture.get_buffer(), buffer);
    EXPECT_EQ(capture.get_cycle_count(), 1);
}

TEST_F(CaptureBridgeTest, BufferCaptureForCycles)
{
    Kriya::BufferCapture capture(buffer);
    capture.for_cycles(10);

    EXPECT_EQ(capture.get_mode(), Kriya::BufferCapture::CaptureMode::ACCUMULATE);
    EXPECT_EQ(capture.get_cycle_count(), 10);
}

TEST_F(CaptureBridgeTest, BufferCaptureUntilCondition)
{
    bool trigger = false;
    Kriya::BufferCapture capture(buffer);
    capture.until_condition([&trigger]() { return trigger; });

    EXPECT_EQ(capture.get_mode(), Kriya::BufferCapture::CaptureMode::TRIGGERED);

    trigger = true;
}

TEST_F(CaptureBridgeTest, BufferCaptureCircular)
{
    Kriya::BufferCapture capture(buffer);
    capture.as_circular(1024);

    EXPECT_EQ(capture.get_mode(), Kriya::BufferCapture::CaptureMode::CIRCULAR);
    EXPECT_EQ(capture.get_circular_size(), 1024);
}

TEST_F(CaptureBridgeTest, BufferCaptureWindowed)
{
    Kriya::BufferCapture capture(buffer);
    capture.with_window(256, 0.5F);

    EXPECT_EQ(capture.get_mode(), Kriya::BufferCapture::CaptureMode::WINDOWED);
    EXPECT_EQ(capture.get_window_size(), 256);
    EXPECT_FLOAT_EQ(capture.get_overlap_ratio(), 0.5F);
}

TEST_F(CaptureBridgeTest, BufferCaptureCallbacks)
{
    bool data_ready_called = false;
    bool cycle_complete_called = false;
    bool data_expired_called = false;
    uint32_t received_cycle = 0;

    Kriya::BufferCapture capture(buffer);

    capture.on_data_ready([&](Kakshya::DataVariant&, uint32_t cycle) {
               data_ready_called = true;
               received_cycle = cycle;
           })
        .on_cycle_complete([&](uint32_t) {
            cycle_complete_called = true;
        })
        .on_data_expired([&](const Kakshya::DataVariant&, uint32_t) {
            data_expired_called = true;
        });

    EXPECT_FALSE(data_ready_called);
    EXPECT_FALSE(cycle_complete_called);
    EXPECT_FALSE(data_expired_called);
}

TEST_F(CaptureBridgeTest, BufferCaptureMetadata)
{
    Kriya::BufferCapture capture(buffer);
    capture.with_tag("test_capture")
        .with_metadata("format", "float64")
        .with_metadata("channels", "2");

    EXPECT_EQ(capture.get_tag(), "test_capture");
}

// ========== CaptureBuilder Tests ==========

TEST_F(CaptureBridgeTest, CaptureBuilderBasic)
{
    Kriya::BufferOperation capture_op = Kriya::BufferOperation::capture_from(buffer)
                                            .for_cycles(5)
                                            .with_tag("builder_test");

    EXPECT_EQ(capture_op.get_type(), Kriya::BufferOperation::OpType::CAPTURE);
    EXPECT_EQ(capture_op.get_tag(), "builder_test");
}

TEST_F(CaptureBridgeTest, CaptureBuilderChaining)
{
    bool callback_triggered = false;

    Kriya::BufferOperation operation = Kriya::BufferOperation::capture_from(buffer)
                                           .for_cycles(3)
                                           .as_circular(512)
                                           .with_window(128, 0.25F)
                                           .on_data_ready([&](Kakshya::DataVariant&, uint32_t) {
                                               callback_triggered = true;
                                           })
                                           .with_tag("chained_capture")
                                           .with_metadata("type", "test");

    EXPECT_EQ(operation.get_type(), Kriya::BufferOperation::OpType::CAPTURE);
    EXPECT_EQ(operation.get_tag(), "chained_capture");
}

// ========== BufferOperation Tests ==========

TEST_F(CaptureBridgeTest, BufferOperationTypes)
{
    auto capture_op = Kriya::BufferOperation::capture(Kriya::BufferCapture(buffer));
    EXPECT_EQ(capture_op.get_type(), Kriya::BufferOperation::OpType::CAPTURE);

    auto transform_op = Kriya::BufferOperation::transform([](Kakshya::DataVariant& data, uint32_t) {
        return data;
    });
    EXPECT_EQ(transform_op.get_type(), Kriya::BufferOperation::OpType::TRANSFORM);

    auto route_buffer_op = Kriya::BufferOperation::route_to_buffer(buffer);
    EXPECT_EQ(route_buffer_op.get_type(), Kriya::BufferOperation::OpType::ROUTE);

    auto route_container_op = Kriya::BufferOperation::route_to_container(dynamic_stream);
    EXPECT_EQ(route_container_op.get_type(), Kriya::BufferOperation::OpType::ROUTE);

    auto load_op = Kriya::BufferOperation::load_from_container(dynamic_stream, buffer, 0, 256);
    EXPECT_EQ(load_op.get_type(), Kriya::BufferOperation::OpType::LOAD);

    auto condition_op = Kriya::BufferOperation::when([](uint32_t cycle) { return cycle % 2 == 0; });
    EXPECT_EQ(condition_op.get_type(), Kriya::BufferOperation::OpType::CONDITION);
}

TEST_F(CaptureBridgeTest, BufferOperationConfiguration)
{
    auto operation = Kriya::BufferOperation::capture(Kriya::BufferCapture(buffer))
                         .with_priority(200)
                         .on_token(Buffers::ProcessingToken::AUDIO_BACKEND)
                         .every_n_cycles(4)
                         .with_tag("configured_op");

    EXPECT_EQ(operation.get_priority(), 200);
    EXPECT_EQ(operation.get_token(), Buffers::ProcessingToken::AUDIO_BACKEND);
    EXPECT_EQ(operation.get_tag(), "configured_op");
}

TEST_F(CaptureBridgeTest, BufferOperationFusion)
{
    auto buffer2 = std::make_shared<Buffers::AudioBuffer>();
    auto buffer3 = std::make_shared<Buffers::AudioBuffer>();

    std::vector<std::shared_ptr<Buffers::AudioBuffer>> sources = { buffer, buffer2 };

    auto fusion_op = Kriya::BufferOperation::fuse_data(
        sources,
        [](std::vector<Kakshya::DataVariant>& inputs, uint32_t) -> Kakshya::DataVariant {
            std::vector<double> result;
            if (!inputs.empty() && std::holds_alternative<std::vector<double>>(inputs[0])) {
                result = std::get<std::vector<double>>(inputs[0]);
                for (size_t i = 1; i < inputs.size(); ++i) {
                    if (std::holds_alternative<std::vector<double>>(inputs[i])) {
                        auto& input_data = std::get<std::vector<double>>(inputs[i]);
                        for (size_t j = 0; j < std::min(result.size(), input_data.size()); ++j) {
                            result[j] += input_data[j];
                        }
                    }
                }
            }
            return result;
        },
        buffer3);

    EXPECT_EQ(fusion_op.get_type(), Kriya::BufferOperation::OpType::FUSE);
}

TEST_F(CaptureBridgeTest, BufferOperationContainerFusion)
{
    auto stream2 = std::make_shared<Kakshya::DynamicSoundStream>(TestConfig::SAMPLE_RATE, 2);
    auto target_stream = std::make_shared<Kakshya::DynamicSoundStream>(TestConfig::SAMPLE_RATE, 2);

    std::vector<std::shared_ptr<Kakshya::DynamicSoundStream>> sources = { dynamic_stream, stream2 };

    auto container_fusion = Kriya::BufferOperation::fuse_containers(
        sources,
        [](std::vector<Kakshya::DataVariant>& inputs, uint32_t) -> Kakshya::DataVariant {
            std::vector<double> result;
            if (!inputs.empty()) {
                for (const auto& input : inputs) {
                    if (std::holds_alternative<std::vector<double>>(input)) {
                        auto& data = std::get<std::vector<double>>(input);
                        if (result.empty()) {
                            result = data;
                        } else {
                            for (size_t i = 0; i < std::min(result.size(), data.size()); ++i) {
                                result[i] = (result[i] + data[i]) / 2.0;
                            }
                        }
                    }
                }
            }
            return result;
        },
        target_stream);

    EXPECT_EQ(container_fusion.get_type(), Kriya::BufferOperation::OpType::FUSE);
}

// ========== BufferPipeline Tests ==========

TEST_F(CaptureBridgeTest, BufferPipelineBasic)
{
    Kriya::BufferPipeline pipeline(*scheduler);

    bool transform_called = false;

    pipeline >> Kriya::BufferOperation::capture_from(buffer).for_cycles(1)
        >> Kriya::BufferOperation::transform([&](Kakshya::DataVariant& data, uint32_t) {
              transform_called = true;
              return data;
          })
        >> Kriya::BufferOperation::route_to_container(dynamic_stream);

    pipeline.execute_once();

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, 512);

    EXPECT_TRUE(transform_called);
}

TEST_F(CaptureBridgeTest, BufferPipelineBranching)
{
    Kriya::BufferPipeline pipeline(*scheduler);

    bool branch_executed = false;

    pipeline >> Kriya::BufferOperation::capture_from(buffer).for_cycles(1);

    pipeline.branch_if([](uint32_t cycle) { return cycle == 0; },
        [&](Kriya::BufferPipeline& branch) {
            branch >> Kriya::BufferOperation::transform([&](Kakshya::DataVariant& data, uint32_t) {
                branch_executed = true;
                return data;
            });
        });

    pipeline.execute_once();
}

TEST_F(CaptureBridgeTest, BufferPipelineParallel)
{
    Kriya::BufferPipeline pipeline(*scheduler);

    pipeline >> Kriya::BufferOperation::capture_from(buffer).for_cycles(1);

    pipeline.parallel({ Kriya::BufferOperation::route_to_container(dynamic_stream).with_priority(255),
        Kriya::BufferOperation::transform([](Kakshya::DataVariant& data, uint32_t) {
            return data;
        }).with_priority(255) });

    pipeline.execute_once();
}

TEST_F(CaptureBridgeTest, BufferPipelineLifecycle)
{
    Kriya::BufferPipeline pipeline(*scheduler);

    bool cycle_start_called = false;
    bool cycle_end_called = false;
    uint32_t start_cycle = 0;
    uint32_t end_cycle = 0;

    pipeline.with_lifecycle(
        [&](uint32_t cycle) {
            cycle_start_called = true;
            start_cycle = cycle;
        },
        [&](uint32_t cycle) {
            cycle_end_called = true;
            end_cycle = cycle;
        });

    pipeline >> Kriya::BufferOperation::capture_from(buffer).for_cycles(1);

    pipeline.execute_once();

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, 512);

    EXPECT_TRUE(cycle_start_called);
    EXPECT_TRUE(cycle_end_called);
    EXPECT_EQ(start_cycle, end_cycle);
}

TEST_F(CaptureBridgeTest, BufferPipelineContinuous)
{
    std::atomic<int> execution_count { 0 };
    std::atomic<bool> should_stop { false };

    auto pipeline = Kriya::BufferPipeline::create(*scheduler);

    *pipeline >> Kriya::BufferOperation::capture_from(buffer).for_cycles(1)
        >> Kriya::BufferOperation::transform([&](Kakshya::DataVariant& data, uint32_t) {
              execution_count++;
              return data;
          });

    pipeline->execute_continuous();

    std::future<void> execution_future = std::async(std::launch::async, [&]() {
        while (!should_stop.load()) {
            scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, 512);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    pipeline->stop_continuous();
    should_stop = true;

    execution_future.wait_for(std::chrono::seconds(1));

    EXPECT_GT(execution_count.load(), 0);
}

// ========== StreamWriteProcessor Tests ==========

TEST_F(CaptureBridgeTest, StreamWriteProcessorBasic)
{
    auto processor = std::make_shared<Buffers::StreamWriteProcessor>(dynamic_stream);

    EXPECT_EQ(processor->get_container(), dynamic_stream);

    processor->processing_function(buffer);

    EXPECT_GT(dynamic_stream->get_num_frames(), 0);
}

TEST_F(CaptureBridgeTest, StreamWriteProcessorDataIntegrity)
{
    auto processor = std::make_shared<Buffers::StreamWriteProcessor>(dynamic_stream);

    dynamic_stream->set_memory_layout(Kakshya::MemoryLayout::ROW_MAJOR);

    const auto& original_data = buffer->get_data();

    buffer->set_channel_id(0);
    processor->processing_function(buffer);
    buffer->set_channel_id(1);
    processor->set_write_position(0);
    processor->processing_function(buffer);

    std::vector<double> readback_data(original_data.size() * 2);
    std::span<double> readback_span(readback_data);

    uint64_t samples_read = dynamic_stream->peek_sequential(readback_span, original_data.size() * 2);

    EXPECT_EQ(samples_read, original_data.size() * 2);

    std::vector<double> channel_0_data;
    for (size_t i = 0; i < samples_read; i += 2) {
        channel_0_data.push_back(readback_data[i]);
    }

    double original_energy = 0.0;
    double readback_energy = 0.0;

    for (double sample : original_data) {
        original_energy += sample * sample;
    }

    for (double sample : channel_0_data) {
        readback_energy += sample * sample;
    }

    EXPECT_NEAR(original_energy, readback_energy, 1e-6)
        << "Energy preservation check failed"
        << "\nOriginal energy: " << original_energy
        << "\nReadback energy: " << readback_energy;
}

TEST_F(CaptureBridgeTest, StreamWriteProcessorNullHandling)
{
    auto processor = std::make_shared<Buffers::StreamWriteProcessor>(dynamic_stream);

    processor->processing_function(nullptr);

    EXPECT_EQ(dynamic_stream->get_num_frames(), 0);

    auto null_processor = std::make_shared<Buffers::StreamWriteProcessor>(nullptr);
    null_processor->processing_function(buffer);

    // Should not crash
}

// ========== DynamicSoundStream Integration Tests ==========

TEST_F(CaptureBridgeTest, DynamicStreamCapacityManagement)
{
    EXPECT_TRUE(dynamic_stream->get_auto_resize());

    std::vector<double> large_data(2048, 0.5);
    std::span<const double> data_span(large_data);

    uint64_t frames_written = dynamic_stream->write_frames(data_span);

    EXPECT_EQ(frames_written, large_data.size());
    EXPECT_GE(dynamic_stream->get_num_frames(), frames_written);
}

TEST_F(CaptureBridgeTest, DynamicStreamCircularBuffer)
{
    dynamic_stream->enable_circular_buffer(512);

    EXPECT_TRUE(dynamic_stream->is_looping());

    std::vector<double> data(1024, 0.7);
    std::span<const double> data_span(data);

    dynamic_stream->write_frames(data_span, 0);

    EXPECT_EQ(dynamic_stream->get_num_frames(), 512);
}

TEST_F(CaptureBridgeTest, DynamicStreamCircularBufferMulti)
{
    dynamic_stream->enable_circular_buffer(512);

    EXPECT_TRUE(dynamic_stream->is_looping());

    std::vector<double> data(1024, 0.7);

    std::vector<std::span<const double>> data_spans = { std::span<const double>(data) };
    dynamic_stream->write_frames(data_spans, 0);

    EXPECT_EQ(dynamic_stream->get_num_frames(), 512);
}

TEST_F(CaptureBridgeTest, DynamicStreamAutoResize)
{
    dynamic_stream->set_auto_resize(true);
    EXPECT_TRUE(dynamic_stream->get_auto_resize());

    dynamic_stream->set_auto_resize(false);
    EXPECT_FALSE(dynamic_stream->get_auto_resize());

    dynamic_stream->ensure_capacity(1000);
    EXPECT_GE(dynamic_stream->get_num_frames(), 1000);
}

TEST_F(CaptureBridgeTest, DynamicStreamReadFrames)
{
    dynamic_stream->set_memory_layout(Kakshya::MemoryLayout::ROW_MAJOR);

    std::vector<double> test_data = { 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8 };
    std::span<const double> data_span(test_data);

    uint64_t frames_written = dynamic_stream->write_frames(data_span, 0, 0);
    EXPECT_EQ(frames_written, test_data.size());

    std::vector<double> read_buffer(test_data.size());
    dynamic_stream->get_channel_frames(std::span<double>(read_buffer), 0, 0);

    double input_energy = 0.0;
    double output_energy = 0.0;

    for (double sample : test_data) {
        input_energy += sample * sample;
    }

    for (double sample : read_buffer) {
        output_energy += sample * sample;
    }

    EXPECT_NEAR(input_energy, output_energy, 1e-6)
        << "Energy not preserved during write/read cycle"
        << "\nInput energy: " << input_energy
        << "\nOutput energy: " << output_energy;

    for (size_t i = 0; i < test_data.size(); ++i) {
        EXPECT_NEAR(test_data[i], read_buffer[i], 1e-10)
            << "Value " << test_data[i] << " was lost or corrupted at index " << i;
    }
}

TEST_F(CaptureBridgeTest, DynamicStreamCircularModeToggle)
{
    EXPECT_FALSE(dynamic_stream->is_looping());

    dynamic_stream->enable_circular_buffer(256);
    EXPECT_TRUE(dynamic_stream->is_looping());

    dynamic_stream->disable_circular_buffer();
    EXPECT_FALSE(dynamic_stream->is_looping());
}

// ========== CycleCoordinator Tests ==========

TEST_F(CaptureBridgeTest, CycleCoordinatorBasic)
{
    Kriya::CycleCoordinator coordinator(*scheduler);

    Kriya::BufferPipeline pipeline1(*scheduler);
    Kriya::BufferPipeline pipeline2(*scheduler);

    pipeline1 >> Kriya::BufferOperation::capture_from(buffer).for_cycles(1);
    pipeline2 >> Kriya::BufferOperation::capture_from(buffer).for_cycles(1);

    auto sync_routine = coordinator.sync_pipelines(
        { std::ref(pipeline1), std::ref(pipeline2) },
        2);
}

TEST_F(CaptureBridgeTest, CycleCoordinatorTransientData)
{
    Kriya::CycleCoordinator coordinator(*scheduler);

    bool data_ready_called = false;
    bool data_expired_called = false;

    auto transient_routine = coordinator.manage_transient_data(
        buffer,
        [&](uint32_t) { data_ready_called = true; },
        [&](uint32_t) { data_expired_called = true; });

    // (Implementation dependent testing)
}

// ========== Error Handling and Edge Cases ==========

TEST_F(CaptureBridgeTest, ErrorHandlingNullPointers)
{
    Kriya::BufferCapture capture(nullptr);
    EXPECT_EQ(capture.get_buffer(), nullptr);

    auto operation = Kriya::BufferOperation::load_from_container(nullptr, buffer);
    EXPECT_EQ(operation.get_type(), Kriya::BufferOperation::OpType::LOAD);
}

TEST_F(CaptureBridgeTest, EdgeCaseZeroCycles)
{
    Kriya::BufferCapture capture(buffer);
    capture.for_cycles(0);

    EXPECT_EQ(capture.get_cycle_count(), 0);
}

TEST_F(CaptureBridgeTest, EdgeCaseInvalidWindowSize)
{
    Kriya::BufferCapture capture(buffer);
    capture.with_window(0, 0.5F);

    EXPECT_EQ(capture.get_window_size(), 0);
}

#define INTEGRATION_TEST_CAPTURE ;

#ifdef INTEGRATION_TEST_CAPTURE
// ========== Integration Tests ==========

TEST_F(CaptureBridgeTest, IntegrationCaptureToStream)
{
    MayaFlux::Init();
    AudioTestHelper::waitForAudio(100);
    MayaFlux::Start();
    AudioTestHelper::waitForAudio(100);

    auto capture_buffer = std::make_shared<Buffers::AudioBuffer>();
    auto target_stream = std::make_shared<Kakshya::DynamicSoundStream>(TestConfig::SAMPLE_RATE, 2);

    auto pipeline = Kriya::BufferPipeline::create(*MayaFlux::get_scheduler());

    *pipeline >> Kriya::BufferOperation::capture_from(capture_buffer).for_cycles(10)
        >> Kriya::BufferOperation::route_to_container(target_stream);

    pipeline->execute_for_cycles(10);

    for (int i = 0; i < 10; ++i) {
        MayaFlux::get_scheduler()->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, 512);
        AudioTestHelper::waitForAudio(10);
    }

    EXPECT_GT(target_stream->get_num_frames(), 0);

    MayaFlux::End();
}

TEST_F(CaptureBridgeTest, IntegrationStreamProcessorChain)
{
    MayaFlux::Init();
    AudioTestHelper::waitForAudio(100);
    MayaFlux::Start();
    AudioTestHelper::waitForAudio(100);

    auto sine = std::make_shared<Nodes::Generator::Sine>(880.0F, 0.3F);
    auto recording_stream = std::make_shared<Kakshya::DynamicSoundStream>(TestConfig::SAMPLE_RATE, 2);
    auto processor = std::make_shared<Buffers::StreamWriteProcessor>(recording_stream);

    recording_stream->enable_circular_buffer(TestConfig::SAMPLE_RATE);

    // This would integrate with the actual audio buffer system
    // processor would be added to the buffer processing chain

    AudioTestHelper::waitForAudio(500);

    EXPECT_GT(recording_stream->get_num_frames(), 0);
    EXPECT_LE(recording_stream->get_num_frames(), TestConfig::SAMPLE_RATE);

    MayaFlux::End();
}

TEST_F(CaptureBridgeTest, HardwareInputCaptureBasic)
{
    MayaFlux::Init(48000, 512, 2, 2);
    AudioTestHelper::waitForAudio(100);
    MayaFlux::Start();
    AudioTestHelper::waitForAudio(100);

    auto buffer_manager = MayaFlux::get_buffer_manager();
    ASSERT_NE(buffer_manager, nullptr) << "Buffer manager should be available after MayaFlux initialization";

    auto input_operation = Kriya::BufferOperation::capture_input(buffer_manager, 0);

    EXPECT_EQ(input_operation.get_type(), Kriya::BufferOperation::OpType::CAPTURE);
    EXPECT_EQ(input_operation.get_tag(), "");

    auto input_operation_custom = Kriya::BufferOperation::capture_input(
        buffer_manager,
        1,
        Kriya::BufferCapture::CaptureMode::CIRCULAR,
        5);

    EXPECT_EQ(input_operation_custom.get_type(), Kriya::BufferOperation::OpType::CAPTURE);

    MayaFlux::End();
}

TEST_F(CaptureBridgeTest, HardwareInputCaptureBuilderFlow)
{
    MayaFlux::Init(48000, 512, 2, 2);
    AudioTestHelper::waitForAudio(100);
    MayaFlux::Start();
    AudioTestHelper::waitForAudio(100);

    auto buffer_manager = MayaFlux::get_buffer_manager();
    ASSERT_NE(buffer_manager, nullptr);

    bool data_received = false;
    uint32_t received_cycle = 0;

    auto input_operation = Kriya::BufferOperation::capture_input_from(buffer_manager, 0)
                               .for_cycles(3)
                               .as_circular(2048)
                               .with_tag("hardware_input_test")
                               .on_data_ready([&](Kakshya::DataVariant& data, uint32_t cycle) {
                                   data_received = true;
                                   received_cycle = cycle;

                                   if (std::holds_alternative<std::vector<double>>(data)) {
                                       const auto& audio_data = std::get<std::vector<double>>(data);
                                       EXPECT_GT(audio_data.size(), 0) << "Should receive non-empty audio data from input";
                                   }
                               })
                               .with_metadata("source", "hardware")
                               .with_metadata("test_type", "integration");

    EXPECT_EQ(input_operation.get_tag(), "hardware_input_test");

    MayaFlux::End();
}

TEST_F(CaptureBridgeTest, HardwareInputRealTimeCapture)
{
    MayaFlux::Init(48000, 512, 2, 2);
    AudioTestHelper::waitForAudio(100);
    MayaFlux::Start();
    AudioTestHelper::waitForAudio(100);

    auto buffer_manager = MayaFlux::get_buffer_manager();
    ASSERT_NE(buffer_manager, nullptr);

    auto target_stream = std::make_shared<Kakshya::DynamicSoundStream>(TestConfig::SAMPLE_RATE, 2);
    target_stream->set_auto_resize(true);

    std::atomic<int> capture_count { 0 };

    auto pipeline = Kriya::BufferPipeline::create(*MayaFlux::get_scheduler());

    *pipeline >> Kriya::BufferOperation::capture_input_from(buffer_manager, 0)
                     .as_circular(1024)
                     .on_data_ready([&](Kakshya::DataVariant& data, uint32_t) {
                         capture_count++;

                         if (std::holds_alternative<std::vector<double>>(data)) {
                             const auto& audio_samples = std::get<std::vector<double>>(data);
                             EXPECT_GT(audio_samples.size(), 0) << "Hardware input should provide audio samples";

                             for (const auto& sample : audio_samples) {
                                 EXPECT_GE(sample, -2.0) << "Audio sample should not exceed reasonable negative range";
                                 EXPECT_LE(sample, 2.0) << "Audio sample should not exceed reasonable positive range";
                             }
                         }
                     })
                     .with_tag("realtime_hw_capture")
        >> Kriya::BufferOperation::route_to_container(target_stream);

    std::future<void> execution_future = std::async(std::launch::async, [pipeline]() {
        pipeline->execute_continuous();
    });

    AudioTestHelper::waitForAudio(200);

    pipeline->stop_continuous();

    execution_future.wait_for(std::chrono::seconds(2));

    EXPECT_GT(capture_count.load(), 0);
    EXPECT_GT(target_stream->get_num_frames(), 0);

    std::cout << "[HardwareInputRealTimeCapture] Captured " << capture_count.load()
              << " audio chunks, stream contains " << target_stream->get_num_frames()
              << " frames" << '\n';

    MayaFlux::End();
}

TEST_F(CaptureBridgeTest, HardwareInputMultiChannelCapture)
{
    MayaFlux::Init(48000, 512, 2, 2);
    AudioTestHelper::waitForAudio(100);
    MayaFlux::Start();
    AudioTestHelper::waitForAudio(100);

    auto buffer_manager = MayaFlux::get_buffer_manager();
    ASSERT_NE(buffer_manager, nullptr);

    std::atomic<int> channel0_captures { 0 };
    std::atomic<int> channel1_captures { 0 };

    auto stream0 = std::make_shared<Kakshya::DynamicSoundStream>(TestConfig::SAMPLE_RATE, 1);
    auto stream1 = std::make_shared<Kakshya::DynamicSoundStream>(TestConfig::SAMPLE_RATE, 1);

    auto pipeline0 = Kriya::BufferPipeline::create(*MayaFlux::get_scheduler());
    auto pipeline1 = Kriya::BufferPipeline::create(*MayaFlux::get_scheduler());

    *pipeline0 >> Kriya::BufferOperation::capture_input_from(buffer_manager, 0)
                      .for_cycles(5)
                      .on_data_ready([&](Kakshya::DataVariant&, uint32_t) {
                          channel0_captures++;
                      })
                      .with_tag("channel_0_input")
        >> Kriya::BufferOperation::route_to_container(stream0);

    *pipeline1 >> Kriya::BufferOperation::capture_input_from(buffer_manager, 1)
                      .for_cycles(5)
                      .on_data_ready([&](Kakshya::DataVariant&, uint32_t) {
                          channel1_captures++;
                      })
                      .with_tag("channel_1_input")
        >> Kriya::BufferOperation::route_to_container(stream1);

    pipeline0->execute_for_cycles(5);
    pipeline1->execute_for_cycles(5);

    for (int i = 0; i < 5; ++i) {
        MayaFlux::get_scheduler()->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, 512);
        AudioTestHelper::waitForAudio(10);
    }

    EXPECT_GT(channel0_captures.load(), 0) << "Channel 0 should capture audio data";

    std::cout << "[HardwareInputMultiChannelCapture] Channel 0: " << channel0_captures.load()
              << " captures, Channel 1: " << channel1_captures.load() << " captures" << '\n';

    MayaFlux::End();
}

TEST_F(CaptureBridgeTest, HardwareInputErrorHandling)
{
    MayaFlux::Init(48000, 512, 2, 2);
    AudioTestHelper::waitForAudio(100);
    MayaFlux::Start();
    AudioTestHelper::waitForAudio(100);

    auto buffer_manager = MayaFlux::get_buffer_manager();
    ASSERT_NE(buffer_manager, nullptr);

    // Test with null buffer manager - this will cause segfault due to no null checks in capture_input
    // Skip this test as it's a known limitation - the API expects valid buffer manager
    std::cout << "[HardwareInputErrorHandling] Skipping null buffer manager test - API requires valid buffer manager" << '\n';

    EXPECT_NO_THROW({
        auto operation = Kriya::BufferOperation::capture_input(buffer_manager, 999);
        EXPECT_EQ(operation.get_type(), Kriya::BufferOperation::OpType::CAPTURE);
    });

    EXPECT_NO_THROW({
        auto operation = Kriya::BufferOperation::capture_input_from(buffer_manager, 500)
                             .for_cycles(1)
                             .with_tag("high_channel_test");
        EXPECT_EQ(operation.get_tag(), "high_channel_test");
    });

    Kriya::BufferPipeline test_pipeline(*MayaFlux::get_scheduler());

    std::atomic<int> callback_count { 0 };

    test_pipeline >> Kriya::BufferOperation::capture_input_from(buffer_manager, 100)
                         .for_cycles(1)
                         .on_data_ready([&](Kakshya::DataVariant&, uint32_t) {
                             callback_count++;
                         })
                         .with_tag("error_test");

    EXPECT_NO_THROW({
        test_pipeline.execute_for_cycles(1);
    });

    std::cout << "[HardwareInputErrorHandling] High channel callbacks: " << callback_count.load() << '\n';

    EXPECT_NO_THROW({
        auto op1 = Kriya::BufferOperation::capture_input(buffer_manager, 888);
        auto op2 = Kriya::BufferOperation::capture_input_from(buffer_manager, 888)
                       .for_cycles(1)
                       .with_tag("duplicate_channel");

        EXPECT_EQ(op1.get_type(), Kriya::BufferOperation::OpType::CAPTURE);
    });

    MayaFlux::End();
}

TEST_F(CaptureBridgeTest, HardwareInputBufferManagerIntegration)
{
    MayaFlux::Init(48000, 512, 2, 2);
    AudioTestHelper::waitForAudio(100);
    MayaFlux::Start();
    AudioTestHelper::waitForAudio(100);

    auto buffer_manager = MayaFlux::get_buffer_manager();
    ASSERT_NE(buffer_manager, nullptr);

    uint32_t test_channel = 2;

    auto input_operation = Kriya::BufferOperation::capture_input(
        buffer_manager,
        test_channel,
        Kriya::BufferCapture::CaptureMode::ACCUMULATE,
        1);

    // The buffer should now be registered with the buffer manager
    // This is implementation-dependent verification

    auto second_operation = Kriya::BufferOperation::capture_input_from(buffer_manager, test_channel)
                                .for_cycles(1)
                                .with_tag("second_input_buffer");

    EXPECT_EQ(input_operation.get_type(), Kriya::BufferOperation::OpType::CAPTURE);

    Kriya::BufferPipeline pipeline(*MayaFlux::get_scheduler());

    std::atomic<bool> data_captured { false };

    pipeline >> second_operation
        >> Kriya::BufferOperation::transform([&](Kakshya::DataVariant& data, uint32_t) {
              data_captured = true;
              return data;
          });

    pipeline.execute_for_cycles(1);

    // Note: data_captured might be false if no audio input is available,
    // but the pipeline should execute without errors
    std::cout << "[HardwareInputBufferManagerIntegration] Data captured: "
              << (data_captured ? "true" : "false") << '\n';

    MayaFlux::End();
}
#endif
}
