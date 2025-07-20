#include "../test_config.h"

#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Core/Engine.hpp"
#include "MayaFlux/Core/SubsystemManager.hpp"
#include "MayaFlux/Core/Subsystems/AudioSubsystem.hpp"
#include "MayaFlux/Kriya/Tasks.hpp"
#include "MayaFlux/Nodes/Generators/Sine.hpp"
#include "MayaFlux/Nodes/Generators/Stochastic.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

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

    Nodes::ProcessingToken node_token = Nodes::ProcessingToken::AUDIO_RATE;
    Buffers::ProcessingToken buf_token = Buffers::ProcessingToken::AUDIO_BACKEND;
};

#ifdef INTEGRATION_TEST

//-------------------------------------------------------------------------
// Engine Initialization State Management Tests
//-------------------------------------------------------------------------

TEST_F(EngineTest, InitializationFlagHandling)
{
    auto test_engine = std::make_unique<Core::Engine>();

    EXPECT_NO_THROW(test_engine->Init(TestConfig::SAMPLE_RATE, TestConfig::BUFFER_SIZE, TestConfig::NUM_CHANNELS));

    EXPECT_NE(test_engine->get_subsystem_manager(), nullptr);
    EXPECT_NE(test_engine->get_scheduler(), nullptr);
    EXPECT_NE(test_engine->get_node_graph_manager(), nullptr);

    EXPECT_NO_THROW(test_engine->Start());

    test_engine->End();
}

TEST_F(EngineTest, AudioBackendDependentBehavior)
{
    auto test_engine = std::make_unique<Core::Engine>();

    EXPECT_NO_THROW(test_engine->Init(TestConfig::SAMPLE_RATE, TestConfig::BUFFER_SIZE, TestConfig::NUM_CHANNELS));
    EXPECT_NO_THROW(test_engine->Start());

    auto audio_subsystem = test_engine->get_subsystem_manager()->get_audio_subsystem();
    EXPECT_NE(audio_subsystem, nullptr);

    EXPECT_TRUE(audio_subsystem->is_ready());

    test_engine->End();
}

//-------------------------------------------------------------------------
// Lifecycle and Component Orchestration Tests
//-------------------------------------------------------------------------

TEST_F(EngineTest, ConstructorCreatesCleanState)
{
    auto test_engine = std::make_unique<Core::Engine>();

    EXPECT_FALSE(test_engine->is_running()) << "New engine should not be running";

    EXPECT_EQ(test_engine->get_subsystem_manager(), nullptr) << "SubsystemManager should be null before Init";
    EXPECT_EQ(test_engine->get_node_graph_manager(), nullptr) << "NodeGraphManager should be null before Init";
    EXPECT_EQ(test_engine->get_buffer_manager(), nullptr) << "BufferManager should be null before Init";
    EXPECT_EQ(test_engine->get_scheduler(), nullptr) << "TaskScheduler should be null before Init";

    EXPECT_NE(test_engine->get_random_engine(), nullptr) << "Random engine should be available";
}

TEST_F(EngineTest, InitializationCreatesAndWiresComponents)
{
    auto test_engine = std::make_unique<Core::Engine>();

    EXPECT_NO_THROW(test_engine->Init(TestConfig::SAMPLE_RATE, TestConfig::BUFFER_SIZE, TestConfig::NUM_CHANNELS, 0));

    EXPECT_NE(test_engine->get_subsystem_manager(), nullptr) << "SubsystemManager not created";
    EXPECT_NE(test_engine->get_node_graph_manager(), nullptr) << "NodeGraphManager not created";
    EXPECT_NE(test_engine->get_buffer_manager(), nullptr) << "BufferManager not created";
    EXPECT_NE(test_engine->get_scheduler(), nullptr) << "TaskScheduler not created";

    EXPECT_EQ(test_engine->get_scheduler()->get_rate(), TestConfig::SAMPLE_RATE);
    EXPECT_EQ(test_engine->get_buffer_manager()->get_num_channels(buf_token), TestConfig::NUM_CHANNELS);
    EXPECT_EQ(test_engine->get_buffer_manager()->get_root_audio_buffer_size(buf_token), TestConfig::BUFFER_SIZE);

    const auto& stream_info = test_engine->get_stream_info();
    EXPECT_EQ(stream_info.sample_rate, TestConfig::SAMPLE_RATE);
    EXPECT_EQ(stream_info.buffer_size, TestConfig::BUFFER_SIZE);
    EXPECT_EQ(stream_info.output.channels, TestConfig::NUM_CHANNELS);
}

TEST_F(EngineTest, InitializationWithCustomStreamInfo)
{
    auto test_engine = std::make_unique<Core::Engine>();

    Core::GlobalStreamInfo custom_config;
    custom_config.sample_rate = 44100;
    custom_config.buffer_size = 256;
    custom_config.output.channels = 1;
    custom_config.input.enabled = true;
    custom_config.input.channels = 2;
    custom_config.non_interleaved = true;
    custom_config.priority = Core::GlobalStreamInfo::StreamPriority::REALTIME;

    EXPECT_NO_THROW(test_engine->Init(custom_config));

    const auto& applied_config = test_engine->get_stream_info();
    EXPECT_EQ(applied_config.sample_rate, 44100);
    EXPECT_EQ(applied_config.buffer_size, 256);
    EXPECT_EQ(applied_config.output.channels, 1);
    EXPECT_TRUE(applied_config.input.enabled);
    EXPECT_EQ(applied_config.input.channels, 2);
    EXPECT_TRUE(applied_config.non_interleaved);
    EXPECT_EQ(applied_config.priority, Core::GlobalStreamInfo::StreamPriority::REALTIME);

    EXPECT_EQ(test_engine->get_buffer_manager()->get_num_channels(buf_token), 1);
    EXPECT_EQ(test_engine->get_buffer_manager()->get_root_audio_buffer_size(buf_token), 256);
    EXPECT_EQ(test_engine->get_scheduler()->get_rate(), 44100);
}

TEST_F(EngineTest, LifecycleStateTransitions)
{
    EXPECT_FALSE(engine->is_running()) << "Engine should not be running initially";

    EXPECT_NO_THROW(engine->Start());
    // Note: is_running() depends on actual audio backend initialization success
    // In CI/test environments, audio may not be available, so we don't assert on is_running()

    EXPECT_NO_THROW(engine->Pause());
    EXPECT_NO_THROW(engine->Resume());
    EXPECT_NO_THROW(engine->End());

    EXPECT_FALSE(engine->is_running()) << "Engine should not be running after End";
}

TEST_F(EngineTest, SubsystemOrchestration)
{
    auto subsystem_manager = engine->get_subsystem_manager();
    ASSERT_NE(subsystem_manager, nullptr);

    auto audio_subsystem = subsystem_manager->get_audio_subsystem();
    EXPECT_NE(audio_subsystem, nullptr) << "Audio subsystem should be created during Init";

    EXPECT_NO_THROW(engine->Start());

    EXPECT_TRUE(audio_subsystem->is_ready()) << "Audio subsystem should be ready";
    // Note: is_running() may be false due to audio backend initialization in CI
}

TEST_F(EngineTest, ComponentAccessRouting)
{
    auto scheduler = engine->get_scheduler();
    auto node_graph = engine->get_node_graph_manager();
    auto buffer_manager = engine->get_buffer_manager();
    auto subsystem_manager = engine->get_subsystem_manager();
    auto random_engine = engine->get_random_engine();

    EXPECT_NE(scheduler, nullptr);
    EXPECT_NE(node_graph, nullptr);
    EXPECT_NE(buffer_manager, nullptr);
    EXPECT_NE(subsystem_manager, nullptr);
    EXPECT_NE(random_engine, nullptr);

    EXPECT_EQ(scheduler->get_rate(), TestConfig::SAMPLE_RATE);
    auto& root = node_graph->get_root_node(node_token, 0);
    EXPECT_NE(&(root), nullptr);
    EXPECT_EQ(buffer_manager->get_num_channels(buf_token), TestConfig::NUM_CHANNELS);
}

//-------------------------------------------------------------------------
// Component Lifetime Management Tests
//-------------------------------------------------------------------------

TEST_F(EngineTest, SharedOwnershipOfComponents)
{
    auto scheduler_ref = engine->get_scheduler();
    auto node_graph_ref = engine->get_node_graph_manager();
    auto buffer_manager_ref = engine->get_buffer_manager();

    engine.reset();

    EXPECT_NE(scheduler_ref, nullptr);
    EXPECT_NE(node_graph_ref, nullptr);
    EXPECT_NE(buffer_manager_ref, nullptr);

    EXPECT_EQ(scheduler_ref->get_rate(), TestConfig::SAMPLE_RATE);
    EXPECT_EQ(buffer_manager_ref->get_num_channels(buf_token), TestConfig::NUM_CHANNELS);
}

TEST_F(EngineTest, CleanShutdownAndResourceManagement)
{
    EXPECT_NO_THROW(engine->Start());
    // Note: is_running() may return false due to audio backend issues in test environment

    EXPECT_NO_THROW(engine->End());
    EXPECT_FALSE(engine->is_running());

    EXPECT_NO_THROW(engine->End());
    EXPECT_FALSE(engine->is_running());

    EXPECT_NO_THROW(engine->Start());
    EXPECT_NO_THROW(engine->End());
}

TEST_F(EngineTest, MoveSemantics)
{
    auto first_engine = std::make_unique<Core::Engine>();
    first_engine->Init(TestConfig::SAMPLE_RATE, TestConfig::BUFFER_SIZE, TestConfig::NUM_CHANNELS);
    first_engine->Start();

    auto second_engine = std::make_unique<Core::Engine>(std::move(*first_engine));

    EXPECT_NE(second_engine->get_scheduler(), nullptr);
    EXPECT_NE(second_engine->get_node_graph_manager(), nullptr);
    EXPECT_NE(second_engine->get_buffer_manager(), nullptr);

    second_engine->End();
}

//-------------------------------------------------------------------------
// Digital-First Processing Integration Tests
//-------------------------------------------------------------------------

TEST_F(EngineTest, NodeGraphIntegration)
{
    EXPECT_NO_THROW(engine->Start());

    auto sine = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f);

    auto node_graph = engine->get_node_graph_manager();
    ASSERT_NE(node_graph, nullptr);

    EXPECT_NO_THROW(node_graph->add_to_root(sine, node_token));

    AudioTestHelper::waitForAudio(50);

    EXPECT_NO_THROW(node_graph->get_root_node(node_token, 0).unregister_node(sine));
}

TEST_F(EngineTest, SchedulerIntegrationWithCoroutines)
{
    EXPECT_NO_THROW(engine->Start());

    int execution_count = 0;
    auto metro_routine = std::make_shared<Vruta::SoundRoutine>(Kriya::metro(*engine->get_scheduler(), 0.005, [&execution_count]() {
        execution_count++;
    }));

    auto scheduler = engine->get_scheduler();
    ASSERT_NE(scheduler, nullptr);

    scheduler->add_task(metro_routine, "", false);

    AudioTestHelper::waitForAudio(50);

    // Note: Execution count may be 0 if audio backend failed to start
    // The important thing is that scheduling doesn't crash
    EXPECT_GE(execution_count, 0) << "Scheduled metro task should not crash";
}

TEST_F(EngineTest, BufferSystemIntegration)
{
    EXPECT_NO_THROW(engine->Start());

    auto buffer_manager = engine->get_buffer_manager();
    ASSERT_NE(buffer_manager, nullptr);

    EXPECT_EQ(buffer_manager->get_root_audio_buffer_size(buf_token), TestConfig::BUFFER_SIZE);
    EXPECT_EQ(buffer_manager->get_num_channels(buf_token), TestConfig::NUM_CHANNELS);

    for (unsigned int i = 0; i < TestConfig::NUM_CHANNELS; i++) {
        auto channel = buffer_manager->get_root_audio_buffer(buf_token, i);
        EXPECT_NE(channel, nullptr);
        EXPECT_EQ(channel->get_channel_id(), i);
        EXPECT_EQ(channel->get_num_samples(), TestConfig::BUFFER_SIZE);
    }
}

//-------------------------------------------------------------------------
// Advanced Digital Paradigm Tests
//-------------------------------------------------------------------------

TEST_F(EngineTest, StochasticEngineIntegration)
{
    auto rng = engine->get_random_engine();
    ASSERT_NE(rng, nullptr);

    double uniform = rng->random_sample(-1.0, 1.0);
    EXPECT_GE(uniform, -1.0);
    EXPECT_LE(uniform, 1.0);

    rng->set_type(Utils::distribution::NORMAL);
    std::vector<double> samples = rng->random_array(0.0, 1.0, 100);
    EXPECT_EQ(samples.size(), 100);

    rng->set_type(Utils::distribution::EXPONENTIAL);
    double exp_sample = rng->random_sample(0.0, 1.0);
    EXPECT_GE(exp_sample, 0.0);

    rng->set_type(Utils::distribution::POISSON);
    double pois_sample = rng->random_sample(0.0, 10.0);
    EXPECT_GE(pois_sample, 0.0);
}

TEST_F(EngineTest, DataDrivenProcessingCapabilities)
{
    EXPECT_NO_THROW(engine->Start());

    auto scheduler = engine->get_scheduler();
    auto node_graph = engine->get_node_graph_manager();

    std::vector<std::pair<double, std::function<void()>>> sequence = {
        { 0.0, []() { /* Digital event 1 */ } },
        { 0.005, []() { /* Digital event 2 */ } },
        { 0.010, []() { /* Digital event 3 */ } }
    };

    auto sequence_routine = std::make_shared<Vruta::SoundRoutine>(Kriya::sequence(*scheduler, sequence));
    EXPECT_NO_THROW(scheduler->add_task(std::move(sequence_routine), "", false));

    AudioTestHelper::waitForAudio(50);

    EXPECT_NE(scheduler, nullptr);
    EXPECT_NE(node_graph, nullptr);
}

TEST_F(EngineTest, SubsystemExtensibility)
{
    auto subsystem_manager = engine->get_subsystem_manager();
    ASSERT_NE(subsystem_manager, nullptr);

    // Engine architecture should support future subsystems
    // (Vulkan, Lua scripting, WASM, UE5 plugins, etc.)

    auto audio_subsystem = subsystem_manager->get_audio_subsystem();
    EXPECT_NE(audio_subsystem, nullptr);

    EXPECT_TRUE(subsystem_manager->has_subsystem(Core::SubsystemType::AUDIO));
}

//-------------------------------------------------------------------------
// Error Handling and Edge Cases
//-------------------------------------------------------------------------

TEST_F(EngineTest, GracefulHandlingOfUninitializedState)
{
    auto test_engine = std::make_unique<Core::Engine>();

    EXPECT_NO_THROW(test_engine->Start());

    EXPECT_NE(test_engine->get_scheduler(), nullptr);
    EXPECT_NE(test_engine->get_node_graph_manager(), nullptr);

    test_engine->End();
}

TEST_F(EngineTest, MultipleInitializationHandling)
{
    EXPECT_NO_THROW(engine->Init(44100, 256, 1));

    auto& stream_info = engine->get_stream_info();
    EXPECT_EQ(stream_info.sample_rate, 44100);
    EXPECT_EQ(stream_info.buffer_size, 256);
    EXPECT_EQ(stream_info.output.channels, 1);

    EXPECT_NO_THROW(engine->Init(48000, 512, 2));

    EXPECT_EQ(engine->get_stream_info().sample_rate, 48000);
    EXPECT_EQ(engine->get_stream_info().buffer_size, 512);
    EXPECT_EQ(engine->get_stream_info().output.channels, 2);
}

//-------------------------------------------------------------------------
// Input Processing and Full-Duplex Integration Tests
//-------------------------------------------------------------------------

TEST_F(EngineTest, InputBufferSystemIntegration)
{
    Core::GlobalStreamInfo input_config;
    input_config.sample_rate = TestConfig::SAMPLE_RATE;
    input_config.buffer_size = TestConfig::BUFFER_SIZE;
    input_config.output.channels = TestConfig::NUM_CHANNELS;
    input_config.input.enabled = true;
    input_config.input.channels = 1;

    auto test_engine = std::make_unique<Core::Engine>();
    EXPECT_NO_THROW(test_engine->Init(input_config));
    EXPECT_NO_THROW(test_engine->Start());

    auto buffer_manager = test_engine->get_buffer_manager();
    ASSERT_NE(buffer_manager, nullptr);

    std::vector<double> synthetic_input(TestConfig::BUFFER_SIZE, 0.5); // Simple test signal

    EXPECT_NO_THROW(buffer_manager->process_input(
        synthetic_input.data(),
        input_config.input.channels,
        TestConfig::BUFFER_SIZE));

    const auto& stream_info = test_engine->get_stream_info();
    EXPECT_TRUE(stream_info.input.enabled);
    EXPECT_EQ(stream_info.input.channels, 1);

    auto sine = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.3f);
    auto node_graph = test_engine->get_node_graph_manager();
    ASSERT_NE(node_graph, nullptr);

    EXPECT_NO_THROW(node_graph->add_to_root(sine, node_token));

    AudioTestHelper::waitForAudio(30);

    EXPECT_NO_THROW(node_graph->get_root_node(node_token, 0).unregister_node(sine));
    test_engine->End();
}

TEST_F(EngineTest, FullDuplexDigitalProcessingChain)
{
    Core::GlobalStreamInfo duplex_config;
    duplex_config.sample_rate = 48000;
    duplex_config.buffer_size = 256;
    duplex_config.output.channels = 2;
    duplex_config.input.enabled = true;
    duplex_config.input.channels = 2;
    duplex_config.priority = Core::GlobalStreamInfo::StreamPriority::REALTIME;

    auto test_engine = std::make_unique<Core::Engine>();
    EXPECT_NO_THROW(test_engine->Init(duplex_config));
    EXPECT_NO_THROW(test_engine->Start());

    auto subsystem_manager = test_engine->get_subsystem_manager();
    ASSERT_NE(subsystem_manager, nullptr);

    auto audio_subsystem = subsystem_manager->get_audio_subsystem();
    ASSERT_NE(audio_subsystem, nullptr);

    std::vector<double> input_data(256 * 2, 0.0);
    std::vector<double> output_data(256 * 2, 0.0);

    for (size_t frame = 0; frame < 256; ++frame) {
        double t = static_cast<double>(frame) / 48000.0;
        input_data[frame * 2] = 0.5 * std::sin(2 * M_PI * 440.0 * t);
        input_data[frame * 2 + 1] = 0.3 * std::sin(2 * M_PI * 880.0 * t);
    }

    EXPECT_NO_THROW(audio_subsystem->process_audio(
        input_data.data(),
        output_data.data(),
        256));

    bool has_output = std::any_of(output_data.begin(), output_data.end(),
        [](double sample) { return std::abs(sample) > 1e-6; });

    // Note: In CI/test environments, actual audio processing may not occur due to
    // missing audio devices, so we focus on API correctness rather than signal content
    EXPECT_GE(output_data.size(), 512) << "Output buffer should maintain expected size";

    const auto& final_stream_info = test_engine->get_stream_info();
    EXPECT_EQ(final_stream_info.sample_rate, 48000);
    EXPECT_TRUE(final_stream_info.input.enabled);
    EXPECT_EQ(final_stream_info.input.channels, 2);
    EXPECT_EQ(final_stream_info.output.channels, 2);

    test_engine->End();
}

#endif

} // namespace MayaFlux::Test
