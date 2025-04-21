#include "../test_config.h"

#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Core/Engine.hpp"
#include "MayaFlux/Core/Stream.hpp"
#include "MayaFlux/Nodes/Generators/Sine.hpp"
#include "MayaFlux/Nodes/Generators/Stochastic.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"
#include "MayaFlux/Tasks/Tasks.hpp"

#define INTEGRATION_TEST

namespace MayaFlux::Test {

class EngineTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        engine = AudioTestHelper::createTestEngine();
    }

    void TearDown() override
    {
        if (engine) {
            engine->End();
        }
        engine.reset();
    }

    std::unique_ptr<Core::Engine> engine;
};

#ifdef INTEGRATION_TEST

TEST_F(EngineTest, ConstructorInitializesBasicComponents)
{
    EXPECT_NE(engine->get_handle(), nullptr) << "RtAudio context not initialized";
    EXPECT_NE(engine->get_random_engine(), nullptr) << "Random engine not initialized";
}

TEST_F(EngineTest, InitializationCreatesAllComponents)
{
    EXPECT_NE(engine->get_stream_manager(), nullptr) << "Stream manager not created";
    EXPECT_NE(engine->get_scheduler(), nullptr) << "Scheduler not created";
    EXPECT_NE(engine->get_buffer_manager(), nullptr) << "Buffer manager not created";
    EXPECT_NE(engine->get_node_graph_manager(), nullptr) << "Node graph manager not created";

    EXPECT_EQ(engine->get_scheduler()->task_sample_rate(), TestConfig::SAMPLE_RATE);

    EXPECT_EQ(engine->get_buffer_manager()->get_num_channels(), TestConfig::NUM_CHANNELS);
    EXPECT_EQ(engine->get_buffer_manager()->get_num_frames(), TestConfig::BUFFER_SIZE);

    EXPECT_NE(&(engine->get_node_graph_manager()->get_root_node()), nullptr);
}

TEST_F(EngineTest, EngineStateTransitions)
{
    EXPECT_FALSE(engine->is_running()) << "Engine should not be running initially";

    engine->Start();
    EXPECT_TRUE(engine->is_running()) << "Engine should be running after Start";

    engine->End();
    EXPECT_FALSE(engine->is_running()) << "Engine should not be running after End";
}

TEST_F(EngineTest, StreamInfoConfiguration)
{
    const auto& stream_info = engine->get_stream_info();
    EXPECT_EQ(stream_info.sample_rate, TestConfig::SAMPLE_RATE);
    EXPECT_EQ(stream_info.buffer_size, TestConfig::BUFFER_SIZE);
    EXPECT_EQ(stream_info.num_channels, TestConfig::NUM_CHANNELS);
}

TEST_F(EngineTest, BufferManagerConfiguration)
{
    auto buffer_manager = engine->get_buffer_manager();
    EXPECT_EQ(buffer_manager->get_num_frames(), TestConfig::BUFFER_SIZE);
    EXPECT_EQ(buffer_manager->get_num_channels(), TestConfig::NUM_CHANNELS);

    for (unsigned int i = 0; i < TestConfig::NUM_CHANNELS; i++) {
        auto channel = buffer_manager->get_channel(i);
        EXPECT_NE(channel, nullptr);
        EXPECT_EQ(channel->get_channel_id(), i);
        EXPECT_EQ(channel->get_num_samples(), TestConfig::BUFFER_SIZE);
    }
}

TEST_F(EngineTest, EngineLifecycle)
{
    EXPECT_NO_THROW({
        engine->Start();
        EXPECT_TRUE(engine->get_stream_manager()->is_running());
    });

    EXPECT_NO_THROW({
        engine->End();
        EXPECT_FALSE(engine->get_stream_manager()->is_running());
    });

    for (int i = 0; i < 3; i++) {
        EXPECT_NO_THROW({
            engine->Start();
            EXPECT_TRUE(engine->get_stream_manager()->is_running());

            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            engine->End();
            EXPECT_FALSE(engine->get_stream_manager()->is_running());
        });
    }
}

TEST_F(EngineTest, MultipleInitializationHandling)
{
    EXPECT_NO_THROW(engine->Init({ .sample_rate = 44100,
        .buffer_size = 256,
        .num_channels = 1 }));

    auto& stream_info = engine->get_stream_info();
    EXPECT_EQ(stream_info.sample_rate, 44100);
    EXPECT_EQ(stream_info.buffer_size, 256);
    EXPECT_EQ(stream_info.num_channels, 1);

    EXPECT_NE(engine->get_stream_manager(), nullptr);
    EXPECT_NE(engine->get_buffer_manager(), nullptr);

    EXPECT_EQ(engine->get_buffer_manager()->get_num_channels(), 1);
    EXPECT_EQ(engine->get_buffer_manager()->get_num_frames(), 256);
}

TEST_F(EngineTest, ComponentCleanupOnDestruction)
{
    engine->Start();

    engine.reset();

    EXPECT_NO_THROW({
        engine = std::make_unique<Core::Engine>();
        engine->Init({ .sample_rate = TestConfig::SAMPLE_RATE,
            .buffer_size = TestConfig::BUFFER_SIZE,
            .num_channels = TestConfig::NUM_CHANNELS });
    });

    EXPECT_FALSE(engine->is_running());
    EXPECT_NO_THROW(engine->Start());
    EXPECT_TRUE(engine->is_running());
}

TEST_F(EngineTest, AudioProcessingMethods)
{
    std::vector<double> input_buffer(TestConfig::BUFFER_SIZE * TestConfig::NUM_CHANNELS, 0.5);
    std::vector<double> output_buffer(TestConfig::BUFFER_SIZE * TestConfig::NUM_CHANNELS, 0.0);

    EXPECT_NO_THROW({
        engine->process_input(input_buffer.data(), TestConfig::BUFFER_SIZE);
    });

    EXPECT_NO_THROW({
        engine->process_output(output_buffer.data(), TestConfig::BUFFER_SIZE);
    });

    EXPECT_NO_THROW({
        engine->process_audio(input_buffer.data(), output_buffer.data(), TestConfig::BUFFER_SIZE);
    });

    AudioTestHelper::waitForAudio(100);

    bool has_audio = false;
    for (const auto& sample : output_buffer) {
        if (std::abs(sample) > 0.0001) {
            has_audio = true;
            break;
        }
    }
    EXPECT_FALSE(has_audio);
}

TEST_F(EngineTest, NodeProcessing)
{
    engine->Start();

    auto sine = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f);

    engine->get_node_graph_manager()->add_to_root(sine);

    std::vector<double> output_buffer(TestConfig::BUFFER_SIZE * TestConfig::NUM_CHANNELS, 0.0);
    engine->process_output(output_buffer.data(), TestConfig::BUFFER_SIZE);

    bool has_audio = false;
    for (const auto& sample : output_buffer) {
        if (std::abs(sample) > 0.01) {
            has_audio = true;
            break;
        }
    }

    EXPECT_TRUE(has_audio) << "No audio output detected from sine oscillator";

    engine->get_node_graph_manager()->get_root_node().unregister_node(sine);
}

TEST_F(EngineTest, TaskScheduling)
{
    float start_value = 0.0f;
    float end_value = 1.0f;
    float duration = 0.01f; // Short duration for quick test

    auto line_task = Tasks::line(*engine->get_scheduler(), start_value, end_value, duration, 5, false);
    engine->schedule_task("test_line", std::move(line_task), true);

    float* value_ptr = engine->get_line_value("test_line");
    ASSERT_NE(value_ptr, nullptr);

    EXPECT_FLOAT_EQ(*value_ptr, start_value);

    auto value_func = engine->line_value("test_line");
    EXPECT_FLOAT_EQ(value_func(), start_value);

    std::vector<double> output_buffer(TestConfig::BUFFER_SIZE * TestConfig::NUM_CHANNELS, 0.0);
    engine->process_output(output_buffer.data(), TestConfig::BUFFER_SIZE);

    EXPECT_GT(*value_ptr, start_value);
    EXPECT_LE(*value_ptr, end_value);

    // Process several more buffers to ensure task completes
    int buffers_needed = 10; // Use a fixed, reasonable number
    for (int i = 0; i < buffers_needed; i++) {
        engine->process_output(output_buffer.data(), TestConfig::BUFFER_SIZE);
    }

    EXPECT_NEAR(*value_ptr, end_value, 0.01);

    auto metro_task = Tasks::metro(*engine->get_scheduler(), 0.1, []() { });
    engine->schedule_task("test_metro", std::move(metro_task));
    EXPECT_TRUE(engine->cancel_task("test_metro"));
    EXPECT_FALSE(engine->cancel_task("nonexistent_task"));
}

TEST_F(EngineTest, RandomEngineAccess)
{
    auto* rng = engine->get_random_engine();
    ASSERT_NE(rng, nullptr);

    double uniform = rng->random_sample(-1.0, 1.0);
    EXPECT_GE(uniform, -1.0);
    EXPECT_LE(uniform, 1.0);

    rng->set_type(Utils::distribution::NORMAL);

    std::vector<double> samples = rng->random_array(0.0, 1.0, 100);
    EXPECT_EQ(samples.size(), 100);

    for (const auto& sample : samples) {
        EXPECT_GE(sample, 0.0);
        EXPECT_LE(sample, 1.0);
    }

    rng->set_type(Utils::distribution::EXPONENTIAL);
    double exp_sample = rng->random_sample(0.0, 1.0);
    EXPECT_GE(exp_sample, 0.0);

    rng->set_type(Utils::distribution::POISSON);
    double pois_sample = rng->random_sample(0.0, 10.0);
    EXPECT_GE(pois_sample, 0.0);
}

TEST_F(EngineTest, RestartableTask)
{
    float start_value = 0.0f;
    float end_value = 1.0f;
    float duration = 0.01f;
    bool restartable = true;

    auto line_task = Tasks::line(*engine->get_scheduler(), start_value, end_value, duration, 5, restartable);
    engine->schedule_task("restartable_line", std::move(line_task));

    std::vector<double> output_buffer(TestConfig::BUFFER_SIZE * TestConfig::NUM_CHANNELS, 0.0);

    int buffers_needed = static_cast<int>(std::ceil((duration * TestConfig::SAMPLE_RATE) / TestConfig::BUFFER_SIZE));

    for (int i = 0; i < buffers_needed; i++) {
        engine->process_output(output_buffer.data(), TestConfig::BUFFER_SIZE);
    }

    float* value_ptr = engine->get_line_value("restartable_line");
    ASSERT_NE(value_ptr, nullptr);
    EXPECT_FLOAT_EQ(*value_ptr, end_value);

    EXPECT_TRUE(engine->restart_task("restartable_line"));

    EXPECT_NEAR(*value_ptr, start_value, 0.01f);

    engine->process_output(output_buffer.data(), TestConfig::BUFFER_SIZE);
    EXPECT_GT(*value_ptr, start_value);
}

TEST_F(EngineTest, ParameterUpdating)
{
    float start_value = 0.0f;
    float end_value = 1.0f;
    float duration = 0.1f;

    auto line_task = Tasks::line(*engine->get_scheduler(), start_value, end_value, duration, 5, true);
    engine->schedule_task("param_line", std::move(line_task));

    float new_end = 2.0f;
    EXPECT_TRUE(engine->update_task_params("param_line", "end_value", new_end));

    float* end_ptr = engine->get_scheduler()->get_tasks()[0]->get_state<float>("end_value");
    ASSERT_NE(end_ptr, nullptr);
    EXPECT_FLOAT_EQ(*end_ptr, new_end);

    EXPECT_FALSE(engine->update_task_params("nonexistent", "value", 1.0f));
}

TEST_F(EngineTest, ConcurrentTasks)
{
    int metro1_count = 0;
    int metro2_count = 0;

    auto metro1_task = Tasks::metro(*engine->get_scheduler(), 0.005, [&metro1_count]() {
        metro1_count++;
    });

    auto metro2_task = Tasks::metro(*engine->get_scheduler(), 0.01, [&metro2_count]() {
        metro2_count++;
    });

    engine->schedule_task("metro1", std::move(metro1_task));
    engine->schedule_task("metro2", std::move(metro2_task));

    std::vector<double> output_buffer(TestConfig::BUFFER_SIZE * TestConfig::NUM_CHANNELS, 0.0);

    int buffer_count = static_cast<int>(std::ceil((0.02 * TestConfig::SAMPLE_RATE) / TestConfig::BUFFER_SIZE));

    for (int i = 0; i < buffer_count; i++) {
        engine->process_output(output_buffer.data(), TestConfig::BUFFER_SIZE);
    }

    EXPECT_GT(metro1_count, 0);
    EXPECT_GT(metro2_count, 0);
    EXPECT_GE(metro1_count, metro2_count);
}

TEST_F(EngineTest, SequenceTask)
{
    std::vector<int> execution_order;

    auto sequence_task = Tasks::sequence(*engine->get_scheduler(), { { 0.0, [&execution_order]() { execution_order.push_back(1); } }, { 0.005, [&execution_order]() { execution_order.push_back(2); } }, { 0.005, [&execution_order]() { execution_order.push_back(3); } } });

    engine->schedule_task("test_sequence", std::move(sequence_task));

    EXPECT_EQ(execution_order.size(), 1);
    EXPECT_EQ(execution_order[0], 1);

    std::vector<double> output_buffer(TestConfig::BUFFER_SIZE * TestConfig::NUM_CHANNELS, 0.0);

    int buffer_count = static_cast<int>(std::ceil((0.02 * TestConfig::SAMPLE_RATE) / TestConfig::BUFFER_SIZE));

    for (int i = 0; i < buffer_count; i++) {
        engine->process_output(output_buffer.data(), TestConfig::BUFFER_SIZE);
    }

    EXPECT_EQ(execution_order.size(), 3);
    EXPECT_EQ(execution_order[0], 1);
    EXPECT_EQ(execution_order[1], 2);
    EXPECT_EQ(execution_order[2], 3);
}

TEST_F(EngineTest, NamedTaskLookup)
{
    auto metro_task = Tasks::metro(*engine->get_scheduler(), 0.1, []() { });
    auto line_task = Tasks::line(*engine->get_scheduler(), 0.0f, 1.0f, 0.1f, 5, false);

    engine->schedule_task("task1", std::move(metro_task));
    engine->schedule_task("task2", std::move(line_task), true);

    float* value_ptr = engine->get_line_value("task2");
    ASSERT_NE(value_ptr, nullptr);
    ASSERT_NEAR(*value_ptr, 0.0f, 0.01f);

    float* null_ptr = engine->get_line_value("nonexistent");
    EXPECT_EQ(null_ptr, nullptr);

    auto null_func = engine->line_value("nonexistent");
    EXPECT_FLOAT_EQ(null_func(), 0.0f);

    EXPECT_TRUE(engine->cancel_task("task1"));
    EXPECT_TRUE(engine->cancel_task("task2"));
}

#endif // INTEGRATION_TEST

} // namespace MayaFlux::Test
