#include "../test_config.h"

#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Core/Engine.hpp"
#include "MayaFlux/Core/SubsystemManager.hpp"
#include "MayaFlux/Core/Subsystems/AudioSubsystem.hpp"
#include "MayaFlux/Nodes/Generators/Sine.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

#define INTEGRATION_TEST

namespace MayaFlux::Test {

#ifdef INTEGRATION_TEST

//-------------------------------------------------------------------------
// SubsystemManager Construction and Initialization Tests
//-------------------------------------------------------------------------

class SubsystemManagerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        node_graph_manager = std::make_shared<Nodes::NodeGraphManager>();
        buffer_manager = std::make_shared<Buffers::BufferManager>(
            TestConfig::NUM_CHANNELS,
            0,
            TestConfig::BUFFER_SIZE,
            Buffers::ProcessingToken::AUDIO_BACKEND);
        task_scheduler = std::make_shared<Vruta::TaskScheduler>(TestConfig::SAMPLE_RATE);

        stream_info.sample_rate = TestConfig::SAMPLE_RATE;
        stream_info.buffer_size = TestConfig::BUFFER_SIZE;
        stream_info.output.channels = TestConfig::NUM_CHANNELS;
        stream_info.input.enabled = false;
    }

    void TearDown() override
    {
        if (subsystem_manager) {
            subsystem_manager->shutdown();
        }
        subsystem_manager.reset();

        task_scheduler.reset();
        buffer_manager.reset();
        node_graph_manager.reset();
    }

    std::unique_ptr<Core::SubsystemManager> subsystem_manager;
    std::shared_ptr<Nodes::NodeGraphManager> node_graph_manager;
    std::shared_ptr<Buffers::BufferManager> buffer_manager;
    std::shared_ptr<Vruta::TaskScheduler> task_scheduler;

    Core::GlobalStreamInfo stream_info;
};

TEST_F(SubsystemManagerTest, ValidConstruction)
{
    EXPECT_NO_THROW({
        subsystem_manager = std::make_unique<Core::SubsystemManager>(
            node_graph_manager, buffer_manager, task_scheduler);
    });

    ASSERT_NE(subsystem_manager, nullptr);

    EXPECT_FALSE(subsystem_manager->has_subsystem(Core::SubsystemType::AUDIO));
    EXPECT_FALSE(subsystem_manager->has_subsystem(Core::SubsystemType::VISUAL));
    EXPECT_FALSE(subsystem_manager->has_subsystem(Core::SubsystemType::WINDOWING));
}

/* TEST_F(SubsystemManagerTest, ConstructionWithNullManagers)
{
    EXPECT_THROW({ auto bad_manager = std::make_unique<Core::SubsystemManager>(
                       nullptr, buffer_manager, task_scheduler); }, std::runtime_error);

    EXPECT_THROW({ auto bad_manager = std::make_unique<Core::SubsystemManager>(
                       node_graph_manager, nullptr, task_scheduler); }, std::runtime_error);

    EXPECT_THROW({ auto bad_manager = std::make_unique<Core::SubsystemManager>(
                       node_graph_manager, buffer_manager, nullptr); }, std::runtime_error);
} */

TEST_F(SubsystemManagerTest, ManagerStateValidation)
{
    subsystem_manager = std::make_unique<Core::SubsystemManager>(
        node_graph_manager, buffer_manager, task_scheduler);

    auto status = subsystem_manager->query_subsystem_status();
    EXPECT_TRUE(status.empty());

    EXPECT_NO_THROW(subsystem_manager->start_all_subsystems());

    EXPECT_NO_THROW(subsystem_manager->shutdown());
}

//-------------------------------------------------------------------------
// AudioSubsystem Creation and Management Tests
//-------------------------------------------------------------------------

TEST_F(SubsystemManagerTest, AudioSubsystemCreation)
{
    subsystem_manager = std::make_unique<Core::SubsystemManager>(
        node_graph_manager, buffer_manager, task_scheduler);

    EXPECT_FALSE(subsystem_manager->has_subsystem(Core::SubsystemType::AUDIO));

    EXPECT_NO_THROW({
        subsystem_manager->create_audio_subsystem(stream_info, Utils::AudioBackendType::RTAUDIO);
    });

    EXPECT_TRUE(subsystem_manager->has_subsystem(Core::SubsystemType::AUDIO));

    auto audio_subsystem = subsystem_manager->get_audio_subsystem();
    ASSERT_NE(audio_subsystem, nullptr);

    EXPECT_EQ(audio_subsystem->get_type(), Core::SubsystemType::AUDIO);
    EXPECT_TRUE(audio_subsystem->is_ready()) << "AudioSubsystem should be ready after creation";
    EXPECT_FALSE(audio_subsystem->is_running()) << "AudioSubsystem should not be running yet";

    const auto& subsystem_stream_info = audio_subsystem->get_stream_info();
    EXPECT_EQ(subsystem_stream_info.sample_rate, TestConfig::SAMPLE_RATE);
    EXPECT_EQ(subsystem_stream_info.buffer_size, TestConfig::BUFFER_SIZE);
    EXPECT_EQ(subsystem_stream_info.output.channels, TestConfig::NUM_CHANNELS);
}

TEST_F(SubsystemManagerTest, AudioSubsystemWithCustomConfiguration)
{
    subsystem_manager = std::make_unique<Core::SubsystemManager>(
        node_graph_manager, buffer_manager, task_scheduler);

    Core::GlobalStreamInfo custom_stream;
    custom_stream.sample_rate = 44100;
    custom_stream.buffer_size = 256;
    custom_stream.output.channels = 1;
    custom_stream.input.enabled = true;
    custom_stream.input.channels = 2;
    custom_stream.non_interleaved = true;
    custom_stream.priority = Core::GlobalStreamInfo::StreamPriority::REALTIME;

    EXPECT_NO_THROW({
        subsystem_manager->create_audio_subsystem(custom_stream, Utils::AudioBackendType::RTAUDIO);
    });

    auto audio_subsystem = subsystem_manager->get_audio_subsystem();
    ASSERT_NE(audio_subsystem, nullptr);

    const auto& applied_config = audio_subsystem->get_stream_info();
    EXPECT_EQ(applied_config.sample_rate, 44100);
    EXPECT_EQ(applied_config.buffer_size, 256);
    EXPECT_EQ(applied_config.output.channels, 1);
    EXPECT_TRUE(applied_config.input.enabled);
    EXPECT_EQ(applied_config.input.channels, 2);
    EXPECT_TRUE(applied_config.non_interleaved);
    EXPECT_EQ(applied_config.priority, Core::GlobalStreamInfo::StreamPriority::REALTIME);
}

TEST_F(SubsystemManagerTest, SubsystemLifecycleManagement)
{
    subsystem_manager = std::make_unique<Core::SubsystemManager>(
        node_graph_manager, buffer_manager, task_scheduler);

    subsystem_manager->create_audio_subsystem(stream_info, Utils::AudioBackendType::RTAUDIO);

    auto audio_subsystem = subsystem_manager->get_audio_subsystem();
    ASSERT_NE(audio_subsystem, nullptr);

    EXPECT_TRUE(audio_subsystem->is_ready());
    EXPECT_FALSE(audio_subsystem->is_running());

    EXPECT_NO_THROW(subsystem_manager->start_all_subsystems());

    auto status = subsystem_manager->query_subsystem_status();
    EXPECT_EQ(status.size(), 1);
    EXPECT_TRUE(status.count(Core::SubsystemType::AUDIO) > 0);

    auto [is_ready, is_running] = status[Core::SubsystemType::AUDIO];
    EXPECT_TRUE(is_ready);
}

//-------------------------------------------------------------------------
// Process Hook System Tests
//-------------------------------------------------------------------------

TEST_F(SubsystemManagerTest, ProcessHookRegistration)
{
    subsystem_manager = std::make_unique<Core::SubsystemManager>(
        node_graph_manager, buffer_manager, task_scheduler);

    subsystem_manager->create_audio_subsystem(stream_info, Utils::AudioBackendType::RTAUDIO);

    int pre_hook_count = 0;
    int post_hook_count = 0;

    EXPECT_NO_THROW({
        subsystem_manager->register_process_hook(
            Core::SubsystemType::AUDIO,
            "test_pre_hook",
            [&pre_hook_count](unsigned int) { pre_hook_count++; },
            Core::HookPosition::PRE_PROCESS);
    });

    EXPECT_NO_THROW({
        subsystem_manager->register_process_hook(
            Core::SubsystemType::AUDIO,
            "test_post_hook",
            [&post_hook_count](unsigned int) { post_hook_count++; });
    });

    EXPECT_TRUE(subsystem_manager->has_process_hook(Core::SubsystemType::AUDIO, "test_pre_hook"));
    EXPECT_TRUE(subsystem_manager->has_process_hook(Core::SubsystemType::AUDIO, "test_post_hook"));
    EXPECT_FALSE(subsystem_manager->has_process_hook(Core::SubsystemType::AUDIO, "nonexistent_hook"));

    auto audio_subsystem = subsystem_manager->get_audio_subsystem();
    std::vector<double> output_buffer(TestConfig::BUFFER_SIZE * TestConfig::NUM_CHANNELS, 0.0);

    EXPECT_NO_THROW({
        audio_subsystem->process_audio(nullptr, output_buffer.data(), TestConfig::BUFFER_SIZE);
    });

    EXPECT_GT(pre_hook_count, 0);
    EXPECT_GT(post_hook_count, 0);
}

TEST_F(SubsystemManagerTest, ProcessHookUnregistration)
{
    subsystem_manager = std::make_unique<Core::SubsystemManager>(
        node_graph_manager, buffer_manager, task_scheduler);

    subsystem_manager->create_audio_subsystem(stream_info, Utils::AudioBackendType::RTAUDIO);

    int hook_count = 0;

    subsystem_manager->register_process_hook(
        Core::SubsystemType::AUDIO,
        "removable_hook",
        [&hook_count](unsigned int) { hook_count++; });

    EXPECT_TRUE(subsystem_manager->has_process_hook(Core::SubsystemType::AUDIO, "removable_hook"));

    auto audio_subsystem = subsystem_manager->get_audio_subsystem();
    std::vector<double> output_buffer(TestConfig::BUFFER_SIZE * TestConfig::NUM_CHANNELS, 0.0);

    audio_subsystem->process_audio(nullptr, output_buffer.data(), TestConfig::BUFFER_SIZE);

    int count_after_first = hook_count;
    EXPECT_GT(count_after_first, 0);

    EXPECT_NO_THROW({
        subsystem_manager->unregister_process_hook(Core::SubsystemType::AUDIO, "removable_hook");
    });

    EXPECT_FALSE(subsystem_manager->has_process_hook(Core::SubsystemType::AUDIO, "removable_hook"));

    audio_subsystem->process_output(output_buffer.data(), TestConfig::BUFFER_SIZE);
    EXPECT_EQ(hook_count, count_after_first) << "Hook should not execute after unregistration";
}

//-------------------------------------------------------------------------
// Cross-Subsystem Operations Tests
//-------------------------------------------------------------------------

TEST_F(SubsystemManagerTest, CrossAccessPermissions)
{
    subsystem_manager = std::make_unique<Core::SubsystemManager>(
        node_graph_manager, buffer_manager, task_scheduler);

    subsystem_manager->create_audio_subsystem(stream_info, Utils::AudioBackendType::RTAUDIO);

    EXPECT_NO_THROW({
        subsystem_manager->allow_cross_access(
            Core::SubsystemType::VISUAL,
            Core::SubsystemType::AUDIO);
    });

    // Test cross-subsystem buffer reading
    // Note: This would typically fail since VISUAL subsystem doesn't exist
    auto buffer_data = subsystem_manager->read_cross_subsystem_buffer(
        Core::SubsystemType::VISUAL,
        Core::SubsystemType::AUDIO,
        0);

    EXPECT_FALSE(buffer_data.has_value());
}

TEST_F(SubsystemManagerTest, CombinedTokenOperations)
{
    subsystem_manager = std::make_unique<Core::SubsystemManager>(
        node_graph_manager, buffer_manager, task_scheduler);

    subsystem_manager->create_audio_subsystem(stream_info, Utils::AudioBackendType::RTAUDIO);

    Core::SubsystemTokens audio_tokens = {
        Buffers::ProcessingToken::AUDIO_BACKEND,
        Nodes::ProcessingToken::AUDIO_RATE,
        Vruta::ProcessingToken::SAMPLE_ACCURATE
    };

    Core::SubsystemTokens visual_tokens = {
        Buffers::ProcessingToken::GRAPHICS_BACKEND,
        Nodes::ProcessingToken::VISUAL_RATE,
        Vruta::ProcessingToken::FRAME_ACCURATE
    };

    bool operation_executed = false;

    EXPECT_NO_THROW({
        subsystem_manager->execute_with_combined_tokens(
            audio_tokens,
            visual_tokens,
            [&operation_executed](Core::SubsystemProcessingHandle& handle) {
                operation_executed = true;
                auto tokens = handle.get_tokens();
                EXPECT_EQ(tokens.Buffer, Buffers::ProcessingToken::AUDIO_BACKEND);
                EXPECT_EQ(tokens.Node, Nodes::ProcessingToken::AUDIO_RATE);
            });
    });

    EXPECT_TRUE(operation_executed);
}

//-------------------------------------------------------------------------
// Subsystem Removal and Cleanup Tests
//-------------------------------------------------------------------------

TEST_F(SubsystemManagerTest, SubsystemRemoval)
{
    subsystem_manager = std::make_unique<Core::SubsystemManager>(
        node_graph_manager, buffer_manager, task_scheduler);

    subsystem_manager->create_audio_subsystem(stream_info, Utils::AudioBackendType::RTAUDIO);
    EXPECT_TRUE(subsystem_manager->has_subsystem(Core::SubsystemType::AUDIO));

    auto audio_subsystem = subsystem_manager->get_audio_subsystem();
    ASSERT_NE(audio_subsystem, nullptr);
    EXPECT_TRUE(audio_subsystem->is_ready());

    EXPECT_NO_THROW({
        subsystem_manager->remove_subsystem(Core::SubsystemType::AUDIO);
    });

    EXPECT_FALSE(subsystem_manager->has_subsystem(Core::SubsystemType::AUDIO));

    auto removed_subsystem = subsystem_manager->get_audio_subsystem();
    EXPECT_EQ(removed_subsystem, nullptr);
}

TEST_F(SubsystemManagerTest, ManagerShutdown)
{
    subsystem_manager = std::make_unique<Core::SubsystemManager>(
        node_graph_manager, buffer_manager, task_scheduler);

    subsystem_manager->create_audio_subsystem(stream_info, Utils::AudioBackendType::RTAUDIO);

    auto audio_subsystem = subsystem_manager->get_audio_subsystem();
    ASSERT_NE(audio_subsystem, nullptr);

    subsystem_manager->start_all_subsystems();

    EXPECT_NO_THROW(subsystem_manager->shutdown());

    EXPECT_FALSE(audio_subsystem->is_running());
    EXPECT_FALSE(audio_subsystem->is_ready());

    auto status = subsystem_manager->query_subsystem_status();
    EXPECT_TRUE(status.empty()) << "All subsystems should be removed after shutdown";
}

//-------------------------------------------------------------------------
// Error Handling and Edge Cases Tests
//-------------------------------------------------------------------------

TEST_F(SubsystemManagerTest, InvalidSubsystemAccess)
{
    subsystem_manager = std::make_unique<Core::SubsystemManager>(
        node_graph_manager, buffer_manager, task_scheduler);

    EXPECT_EQ(subsystem_manager->get_audio_subsystem(), nullptr);
    EXPECT_EQ(subsystem_manager->get_subsystem(Core::SubsystemType::VISUAL), nullptr);
    EXPECT_EQ(subsystem_manager->get_subsystem(Core::SubsystemType::WINDOWING), nullptr);

    EXPECT_NO_THROW({
        subsystem_manager->remove_subsystem(Core::SubsystemType::VISUAL);
    });

    EXPECT_NO_THROW({
        subsystem_manager->register_process_hook(
            Core::SubsystemType::AUDIO,
            "test_hook",
            [](unsigned int) { });
    });

    EXPECT_FALSE(subsystem_manager->has_process_hook(Core::SubsystemType::VISUAL, "test_hook"));
}

TEST_F(SubsystemManagerTest, DuplicateSubsystemCreation)
{
    subsystem_manager = std::make_unique<Core::SubsystemManager>(
        node_graph_manager, buffer_manager, task_scheduler);

    subsystem_manager->create_audio_subsystem(stream_info, Utils::AudioBackendType::RTAUDIO);
    EXPECT_TRUE(subsystem_manager->has_subsystem(Core::SubsystemType::AUDIO));

    auto first_subsystem = subsystem_manager->get_audio_subsystem();
    ASSERT_NE(first_subsystem, nullptr);

    Core::GlobalStreamInfo modified_stream = stream_info;
    modified_stream.sample_rate = 44100;

    EXPECT_NO_THROW({
        subsystem_manager->create_audio_subsystem(modified_stream, Utils::AudioBackendType::RTAUDIO);
    });

    EXPECT_TRUE(subsystem_manager->has_subsystem(Core::SubsystemType::AUDIO));

    auto second_subsystem = subsystem_manager->get_audio_subsystem();
    ASSERT_NE(second_subsystem, nullptr);

    EXPECT_EQ(second_subsystem->get_stream_info().sample_rate, 44100);
}

TEST_F(SubsystemManagerTest, EmptyManagerOperations)
{
    subsystem_manager = std::make_unique<Core::SubsystemManager>(
        node_graph_manager, buffer_manager, task_scheduler);

    EXPECT_NO_THROW(subsystem_manager->start_all_subsystems());
    EXPECT_NO_THROW(subsystem_manager->shutdown());

    auto status = subsystem_manager->query_subsystem_status();
    EXPECT_TRUE(status.empty());
}

//-------------------------------------------------------------------------
// Integration with Engine Tests
//-------------------------------------------------------------------------

TEST_F(SubsystemManagerTest, EngineIntegration)
{
    auto engine = AudioTestHelper::createTestEngine();

    auto engine_subsystem_manager = engine->get_subsystem_manager();
    ASSERT_NE(engine_subsystem_manager, nullptr);

    EXPECT_TRUE(engine_subsystem_manager->has_subsystem(Core::SubsystemType::AUDIO));

    auto audio_subsystem = engine_subsystem_manager->get_audio_subsystem();
    ASSERT_NE(audio_subsystem, nullptr);
    EXPECT_TRUE(audio_subsystem->is_ready());

    EXPECT_NO_THROW(engine->Start());

    auto sine = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.3f);
    auto node_graph = engine->get_node_graph_manager();

    EXPECT_NO_THROW({
        node_graph->add_to_root(sine, Nodes::ProcessingToken::AUDIO_RATE);
    });

    AudioTestHelper::waitForAudio(50);

    auto& root = node_graph->get_root_node(Nodes::ProcessingToken::AUDIO_RATE, 0);
    EXPECT_NO_THROW(root.unregister_node(sine));

    engine->End();
}

#endif // INTEGRATION_TEST

} // namespace MayaFlux::Test
