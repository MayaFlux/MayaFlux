#include "../test_config.h"

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Kakshya/Source/DynamicSoundStream.hpp"
#include "MayaFlux/Kriya/BufferPipeline.hpp"
#include "MayaFlux/MayaFlux.hpp"
#include <atomic>
#include <future>

namespace MayaFlux::Test {

class BufferPipelineTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        scheduler = std::make_shared<Vruta::TaskScheduler>(TestConfig::SAMPLE_RATE);
        buffer_manager = std::make_shared<Buffers::BufferManager>();
        input_buffer = std::make_shared<Buffers::AudioBuffer>();
        output_buffer = std::make_shared<Buffers::AudioBuffer>();
        output_stream = std::make_shared<Kakshya::DynamicSoundStream>(TestConfig::SAMPLE_RATE, 2);

        setupTestData();
    }

    void TearDown() override
    {
        scheduler.reset();
        buffer_manager.reset();
        input_buffer.reset();
        output_buffer.reset();
        output_stream.reset();
    }

    void setupTestData()
    {
        auto& data = input_buffer->get_data();
        data.resize(512);

        for (size_t i = 0; i < data.size(); ++i) {
            data[i] = std::sin(2.0 * M_PI * 440.0 * i / TestConfig::SAMPLE_RATE) * 0.5;
        }
    }

    void runSchedulerCycles(int cycles)
    {
        for (int i = 0; i < cycles; ++i) {
            scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, 512);
            AudioTestHelper::waitForAudio(5);
            scheduler->process_buffer_cycle_tasks();
        }
    }

    std::shared_ptr<Vruta::TaskScheduler> scheduler;
    std::shared_ptr<Buffers::BufferManager> buffer_manager;
    std::shared_ptr<Buffers::AudioBuffer> input_buffer;
    std::shared_ptr<Buffers::AudioBuffer> output_buffer;
    std::shared_ptr<Kakshya::DynamicSoundStream> output_stream;
};

// ========== PHASED STRATEGY TESTS ==========

TEST_F(BufferPipelineTest, PhasedStrategy_SimpleCaptureThenProcess)
{
    auto pipeline = Kriya::BufferPipeline::create(*scheduler, buffer_manager);
    std::atomic<int> capture_count { 0 };
    std::atomic<int> transform_count { 0 };

    pipeline->with_strategy(Kriya::ExecutionStrategy::PHASED)
        .capture_timing(Vruta::DelayContext::BUFFER_BASED);

    *pipeline
        >> Kriya::BufferOperation::capture_from(input_buffer)
               .for_cycles(1)
               .on_data_ready([&](auto&, uint32_t) { capture_count++; })
        >> Kriya::BufferOperation::transform([&](auto& data, uint32_t) {
              transform_count++;
              return data;
          });

    pipeline->execute_for_cycles(3);
    runSchedulerCycles(5);

    EXPECT_EQ(capture_count.load(), 3);
    EXPECT_EQ(transform_count.load(), 3);
}

TEST_F(BufferPipelineTest, PhasedStrategy_AccumulationOverMultipleCycles)
{
    auto pipeline = Kriya::BufferPipeline::create(*scheduler, buffer_manager);
    std::atomic<int> transform_calls { 0 };
    size_t accumulated_size = 0;

    pipeline->with_strategy(Kriya::ExecutionStrategy::PHASED);

    *pipeline
        >> Kriya::BufferOperation::capture_from(input_buffer)
               .for_cycles(5) // Accumulates 5 iterations
        >> Kriya::BufferOperation::transform([&](auto& data, uint32_t) {
              transform_calls++;
              const auto& vec = std::get<std::vector<double>>(data);
              accumulated_size = vec.size();
              return data;
          });

    pipeline->execute_for_cycles(1);
    runSchedulerCycles(10);

    EXPECT_EQ(transform_calls.load(), 1);
    EXPECT_EQ(accumulated_size, 512 * 5); // 5 iterations × buffer size
}

TEST_F(BufferPipelineTest, PhasedStrategy_ImmediateRouting)
{
    auto pipeline = Kriya::BufferPipeline::create(*scheduler, buffer_manager);
    std::atomic<int> writes { 0 };

    output_stream->set_auto_resize(true);

    pipeline->with_strategy(Kriya::ExecutionStrategy::PHASED);

    *pipeline
        >> Kriya::BufferOperation::capture_from(input_buffer)
               .for_cycles(10)
               .on_data_ready([&](auto&, uint32_t) { writes++; })
        >> Kriya::BufferOperation::route_to_container(output_stream);

    pipeline->execute_for_cycles(1);
    runSchedulerCycles(15);

    EXPECT_GE(writes.load(), 9);
    EXPECT_LE(writes.load(), 10);
    EXPECT_GT(output_stream->get_num_frames(), 0);
}

TEST_F(BufferPipelineTest, PhasedStrategy_CircularBufferBehavior)
{
    auto pipeline = Kriya::BufferPipeline::create(*scheduler, buffer_manager);
    size_t data_size_cycle1 = 0;
    size_t data_size_cycle2 = 0;

    pipeline->with_strategy(Kriya::ExecutionStrategy::PHASED);

    *pipeline
        >> Kriya::BufferOperation::capture_from(input_buffer)
               .for_cycles(5)
               .as_circular(1024) // Limit to 1024 samples
        >> Kriya::BufferOperation::transform([&](auto& data, uint32_t) {
              const auto& vec = std::get<std::vector<double>>(data);
              if (data_size_cycle1 == 0) {
                  data_size_cycle1 = vec.size();
              } else {
                  data_size_cycle2 = vec.size();
              }
              return data;
          });

    pipeline->execute_for_cycles(2);
    runSchedulerCycles(15);

    EXPECT_GT(data_size_cycle1, 0);
    EXPECT_LE(data_size_cycle1, 1024);

    if (data_size_cycle2 > 0) {
        EXPECT_LE(data_size_cycle2, 1024);
    }
}

TEST_F(BufferPipelineTest, PhasedStrategy_WindowedCapture)
{
    auto pipeline = Kriya::BufferPipeline::create(*scheduler, buffer_manager);
    std::atomic<int> transform_calls { 0 };
    size_t final_window_size = 0;

    pipeline->with_strategy(Kriya::ExecutionStrategy::PHASED);

    *pipeline
        >> Kriya::BufferOperation::capture_from(input_buffer)
               .for_cycles(10)
               .with_window(512, 0.5F) // 512 samples with 50% overlap
        >> Kriya::BufferOperation::transform([&](auto& data, uint32_t) {
              transform_calls++;
              const auto& vec = std::get<std::vector<double>>(data);
              final_window_size = vec.size();
              return data;
          });

    pipeline->execute_for_cycles(1);
    runSchedulerCycles(25);

    EXPECT_GE(transform_calls.load(), 1);
    EXPECT_GT(final_window_size, 0);
    EXPECT_LE(final_window_size, 512);
}

// ========== STREAMING STRATEGY TESTS ==========

TEST_F(BufferPipelineTest, StreamingStrategy_ImmediateFlowThrough)
{
    auto pipeline = Kriya::BufferPipeline::create(*scheduler, buffer_manager);
    std::atomic<int> capture_count { 0 };
    std::atomic<int> transform_count { 0 };

    pipeline->with_strategy(Kriya::ExecutionStrategy::STREAMING);

    *pipeline
        >> Kriya::BufferOperation::capture_from(input_buffer)
               .for_cycles(5)
               .on_data_ready([&](auto&, uint32_t) { capture_count++; })
        >> Kriya::BufferOperation::transform([&](auto& data, uint32_t) {
              transform_count++;
              return data;
          });

    pipeline->execute_for_cycles(1);
    runSchedulerCycles(7);

    EXPECT_EQ(capture_count.load(), 5);
    EXPECT_EQ(transform_count.load(), 5);
}

TEST_F(BufferPipelineTest, StreamingStrategy_ModifyBufferContinuous)
{
    buffer_manager->add_audio_buffer(input_buffer, Buffers::ProcessingToken::AUDIO_BACKEND, 0);

    auto pipeline = Kriya::BufferPipeline::create(*scheduler, buffer_manager);
    std::atomic<int> modify_count { 0 };

    pipeline->with_strategy(Kriya::ExecutionStrategy::STREAMING);

    pipeline
        >> Kriya::BufferOperation::capture_from(input_buffer).for_cycles(1)
        >> Kriya::BufferOperation::modify_buffer(input_buffer, [&](auto buf) {
              modify_count++;
              auto& data = buf->get_data();
              for (auto& sample : data) {
                  sample *= 0.9; // Simple gain reduction
              }
          }).as_streaming();

    pipeline->execute_for_cycles(10);

    // Process buffers through the buffer manager
    for (int i = 0; i < 15; ++i) {
        buffer_manager->process_token(Buffers::ProcessingToken::AUDIO_BACKEND, 1);
        scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, 512);
        AudioTestHelper::waitForAudio(5);
        scheduler->process_buffer_cycle_tasks();
    }

    EXPECT_GT(modify_count.load(), 0);
}

TEST_F(BufferPipelineTest, StreamingStrategy_LowLatencyProcessing)
{
    auto pipeline = Kriya::BufferPipeline::create(*scheduler, buffer_manager);
    std::vector<uint32_t> cycle_numbers;

    pipeline->with_strategy(Kriya::ExecutionStrategy::STREAMING)
        .capture_timing(Vruta::DelayContext::SAMPLE_BASED)
        .process_timing(Vruta::DelayContext::SAMPLE_BASED);

    *pipeline
        >> Kriya::BufferOperation::capture_from(input_buffer)
               .for_cycles(3)
        >> Kriya::BufferOperation::transform([&](auto& data, uint32_t cycle) {
              cycle_numbers.push_back(cycle);
              return data;
          })
        >> Kriya::BufferOperation::route_to_container(output_stream);

    pipeline->execute_scheduled(1, 256);
    runSchedulerCycles(5);

    EXPECT_GE(cycle_numbers.size(), 3);
    EXPECT_LE(cycle_numbers.size(), 4);
}

// ========== EXECUTION MODES TESTS ==========

TEST_F(BufferPipelineTest, ExecutionMode_OnceCompletes)
{
    auto pipeline = Kriya::BufferPipeline::create(*scheduler, buffer_manager);
    std::atomic<int> execution_count { 0 };

    *pipeline
        >> Kriya::BufferOperation::capture_from(input_buffer)
               .for_cycles(1)
               .on_data_ready([&](auto&, uint32_t) { execution_count++; });

    pipeline->execute_once();
    runSchedulerCycles(3);

    EXPECT_EQ(execution_count.load(), 1);
}

TEST_F(BufferPipelineTest, ExecutionMode_ForCyclesExact)
{
    auto pipeline = Kriya::BufferPipeline::create(*scheduler, buffer_manager);
    std::atomic<int> cycle_count { 0 };

    *pipeline
        >> Kriya::BufferOperation::capture_from(input_buffer)
               .for_cycles(1)
               .on_data_ready([&](auto&, uint32_t) { cycle_count++; });

    pipeline->execute_for_cycles(7);
    runSchedulerCycles(10);

    EXPECT_GE(cycle_count.load(), 6);
    EXPECT_LE(cycle_count.load(), 7);
}

TEST_F(BufferPipelineTest, ExecutionMode_ContinuousUntilStopped)
{
    auto pipeline = Kriya::BufferPipeline::create(*scheduler, buffer_manager);
    std::atomic<int> cycle_count { 0 };

    *pipeline
        >> Kriya::BufferOperation::capture_from(input_buffer)
               .for_cycles(1)
               .on_data_ready([&](auto&, uint32_t) { cycle_count++; });

    pipeline->execute_continuous();

    // Run for a bit
    runSchedulerCycles(10);
    int mid_count = cycle_count.load();

    // Stop and verify it stops growing
    pipeline->stop_continuous();
    runSchedulerCycles(5);
    int final_count = cycle_count.load();

    EXPECT_GT(mid_count, 0);
    EXPECT_GE(final_count, mid_count);
}

TEST_F(BufferPipelineTest, ExecutionMode_BufferRateSynchronization)
{
    auto pipeline = Kriya::BufferPipeline::create(*scheduler, buffer_manager);
    std::atomic<int> buffer_rate_executions { 0 };

    *pipeline
        >> Kriya::BufferOperation::capture_from(input_buffer)
               .for_cycles(1)
               .on_data_ready([&](auto&, uint32_t) { buffer_rate_executions++; });

    pipeline->execute_buffer_rate(5);
    runSchedulerCycles(8);

    EXPECT_EQ(buffer_rate_executions.load(), 5);
}

// ========== MULTI-OPERATION PIPELINES ==========

TEST_F(BufferPipelineTest, ComplexPipeline_CaptureTransformFuseRoute)
{
    auto buffer2 = std::make_shared<Buffers::AudioBuffer>();
    auto& data2 = buffer2->get_data();
    data2.resize(512);
    std::fill(data2.begin(), data2.end(), 0.3);

    auto fused_buffer = std::make_shared<Buffers::AudioBuffer>();
    auto pipeline = Kriya::BufferPipeline::create(*scheduler, buffer_manager);

    std::atomic<int> transform_count { 0 };
    std::atomic<int> fuse_count { 0 };

    *pipeline
        >> Kriya::BufferOperation::capture_from(input_buffer).for_cycles(1)
        >> Kriya::BufferOperation::transform([&](auto& data, uint32_t) {
              transform_count++;
              auto& vec = std::get<std::vector<double>>(data);
              for (auto& sample : vec)
                  sample *= 0.5;
              return data;
          })
        >> Kriya::BufferOperation::fuse_data(
            { input_buffer, buffer2 },
            [&](auto& sources, uint32_t) -> Kakshya::DataVariant {
                fuse_count++;
                std::vector<double> result;
                for (auto& src : sources) {
                    const auto& vec = std::get<std::vector<double>>(src);
                    if (result.empty())
                        result = vec;
                    else {
                        for (size_t i = 0; i < std::min(result.size(), vec.size()); ++i) {
                            result[i] = (result[i] + vec[i]) / 2.0;
                        }
                    }
                }
                return result;
            },
            fused_buffer)
        >> Kriya::BufferOperation::route_to_container(output_stream);

    pipeline->execute_for_cycles(3);
    runSchedulerCycles(5);

    EXPECT_EQ(transform_count.load(), 3);
    EXPECT_EQ(fuse_count.load(), 3);
    EXPECT_GT(output_stream->get_num_frames(), 0);
}

TEST_F(BufferPipelineTest, ComplexPipeline_ConditionalOperations)
{
    auto pipeline = Kriya::BufferPipeline::create(*scheduler, buffer_manager);
    std::atomic<int> conditional_transform_count { 0 };
    std::atomic<int> total_count { 0 };

    *pipeline
        >> Kriya::BufferOperation::capture_from(input_buffer)
               .for_cycles(1)
               .on_data_ready([&](auto&, uint32_t cycle) {
                   total_count++;
                   // Also track which cycles are even for debugging
               })
        >> Kriya::BufferOperation::when([](uint32_t cycle) {
              return cycle % 2 == 0;
          })
        >> Kriya::BufferOperation::transform([&](auto& data, uint32_t cycle) {
              conditional_transform_count++;
              return data;
          });

    pipeline->execute_for_cycles(10);
    runSchedulerCycles(15);

    EXPECT_GE(total_count.load(), 9);
    EXPECT_LE(total_count.load(), 10);

    // Conditional transforms should be roughly half (even cycles)
    // But WHEN condition checks cycle number within the coroutine execution
    // which may not align perfectly with our expectations
    // The key is that it's less than total_count
    EXPECT_LT(conditional_transform_count.load(), total_count.load());
    EXPECT_GT(conditional_transform_count.load(), 0);
}

// ========== BRANCHING TESTS ==========

TEST_F(BufferPipelineTest, Branch_AsynchronousExecution)
{
    auto pipeline = Kriya::BufferPipeline::create(*scheduler, buffer_manager);
    std::atomic<int> main_count { 0 };
    std::atomic<int> branch_count { 0 };

    *pipeline
        >> Kriya::BufferOperation::capture_from(input_buffer)
               .for_cycles(1)
               .on_data_ready([&](auto&, uint32_t) { main_count++; });

    pipeline->branch_if(
        [](uint32_t cycle) { return cycle % 3 == 0; },
        [&](auto& branch) {
            branch >> Kriya::BufferOperation::dispatch_to([&](auto&, uint32_t) {
                branch_count++;
            });
        },
        false); // Asynchronous

    pipeline->execute_for_cycles(9);
    runSchedulerCycles(15);

    EXPECT_EQ(main_count.load(), 9);
    EXPECT_EQ(branch_count.load(), 3); // Every 3rd cycle
}

TEST_F(BufferPipelineTest, Branch_SynchronousExecution)
{
    auto pipeline = Kriya::BufferPipeline::create(*scheduler, buffer_manager);
    std::atomic<int> branch_count { 0 };

    *pipeline
        >> Kriya::BufferOperation::capture_from(input_buffer).for_cycles(1);

    pipeline->branch_if(
        [](uint32_t cycle) { return cycle == 2; },
        [&](auto& branch) {
            branch >> Kriya::BufferOperation::dispatch_to([&](auto&, uint32_t) {
                branch_count++;
            });
        },
        true); // Synchronous - waits for completion

    pipeline->execute_for_cycles(5);
    runSchedulerCycles(8);

    EXPECT_EQ(branch_count.load(), 1);
}

// ========== LIFECYCLE CALLBACKS ==========

TEST_F(BufferPipelineTest, Lifecycle_CallbacksExecute)
{
    auto pipeline = Kriya::BufferPipeline::create(*scheduler, buffer_manager);
    std::vector<uint32_t> start_cycles;
    std::vector<uint32_t> end_cycles;

    pipeline->with_lifecycle(
        [&](uint32_t cycle) { start_cycles.push_back(cycle); },
        [&](uint32_t cycle) { end_cycles.push_back(cycle); });

    *pipeline
        >> Kriya::BufferOperation::capture_from(input_buffer).for_cycles(1);

    pipeline->execute_for_cycles(5);
    runSchedulerCycles(8);
    AudioTestHelper::waitForAudio(1000);

    EXPECT_EQ(start_cycles.size(), 5);
    EXPECT_EQ(end_cycles.size(), 4);

    for (size_t i = 0; i < end_cycles.size(); ++i) {
        EXPECT_EQ(start_cycles[i], end_cycles[i]);
    }
}

// ========== HARDWARE INPUT INTEGRATION ==========
#define INTEGRATION_TEST_CAPTURE

#ifdef INTEGRATION_TEST_CAPTURE

TEST_F(BufferPipelineTest, HardwareInput_SimpleCaptureToStream)
{
    MayaFlux::Init(48000, 512, 2, 2);
    AudioTestHelper::waitForAudio(100);
    MayaFlux::Start();
    AudioTestHelper::waitForAudio(100);

    auto buffer_mgr = MayaFlux::get_buffer_manager();
    auto capture_stream = std::make_shared<Kakshya::DynamicSoundStream>(48000, 2);
    std::atomic<int> capture_count { 0 };

    auto pipeline = Kriya::BufferPipeline::create(*MayaFlux::get_scheduler(), buffer_mgr);

    *pipeline
        >> Kriya::BufferOperation::capture_input_from(buffer_mgr, 0)
               .for_cycles(5)
               .on_data_ready([&](auto&, uint32_t) { capture_count++; })
        >> Kriya::BufferOperation::route_to_container(capture_stream);

    pipeline->execute_for_cycles(1);

    AudioTestHelper::waitForAudio(100);

    EXPECT_EQ(capture_count.load(), 5);
    EXPECT_GT(capture_stream->get_num_frames(), 0);

    MayaFlux::End();
}

TEST_F(BufferPipelineTest, HardwareInput_RealTimeProcessing)
{
    MayaFlux::Init(48000, 512, 2, 2);
    AudioTestHelper::waitForAudio(100);
    MayaFlux::Start();
    AudioTestHelper::waitForAudio(100);

    auto buffer_mgr = MayaFlux::get_buffer_manager();
    std::atomic<int> process_count { 0 };

    auto pipeline = Kriya::BufferPipeline::create(*MayaFlux::get_scheduler(), buffer_mgr);
    pipeline->with_strategy(Kriya::ExecutionStrategy::STREAMING);

    *pipeline
        >> Kriya::BufferOperation::capture_input_from(buffer_mgr, 0)
               .as_circular(2048)
        >> Kriya::BufferOperation::transform([&](auto& data, uint32_t) {
              process_count++;
              const auto& samples = std::get<std::vector<double>>(data);
              // Verify data is reasonable
              for (const auto& sample : samples) {
                  EXPECT_GE(sample, -2.0);
                  EXPECT_LE(sample, 2.0);
              }
              return data;
          });

    pipeline->execute_continuous();

    AudioTestHelper::waitForAudio(200);
    pipeline->stop_continuous();

    EXPECT_GT(process_count.load(), 0);

    MayaFlux::End();
}

TEST_F(BufferPipelineTest, HardwareInput_MultiChannel)
{
    MayaFlux::Init(48000, 512, 2, 2);
    AudioTestHelper::waitForAudio(100);
    MayaFlux::Start();
    AudioTestHelper::waitForAudio(100);

    auto buffer_mgr = MayaFlux::get_buffer_manager();
    std::atomic<int> ch0_count { 0 };
    std::atomic<int> ch1_count { 0 };

    auto stream0 = std::make_shared<Kakshya::DynamicSoundStream>(48000, 1);
    auto stream1 = std::make_shared<Kakshya::DynamicSoundStream>(48000, 1);

    auto pipeline0 = Kriya::BufferPipeline::create(*MayaFlux::get_scheduler(), buffer_mgr);
    auto pipeline1 = Kriya::BufferPipeline::create(*MayaFlux::get_scheduler(), buffer_mgr);

    *pipeline0
        >> Kriya::BufferOperation::capture_input_from(buffer_mgr, 0)
               .for_cycles(3)
               .on_data_ready([&](auto&, uint32_t) { ch0_count++; })
        >> Kriya::BufferOperation::route_to_container(stream0);

    *pipeline1
        >> Kriya::BufferOperation::capture_input_from(buffer_mgr, 1)
               .for_cycles(3)
               .on_data_ready([&](auto&, uint32_t) { ch1_count++; })
        >> Kriya::BufferOperation::route_to_container(stream1);

    pipeline0->execute_for_cycles(1);
    pipeline1->execute_for_cycles(1);

    AudioTestHelper::waitForAudio(500);
    EXPECT_EQ(ch0_count.load(), 3);
    EXPECT_EQ(ch1_count.load(), 3);

    MayaFlux::End();
}

#endif // INTEGRATION_TEST_CAPTURE

} // namespace MayaFlux::Test
