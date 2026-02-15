#include <cstddef>

#include "../test_config.h"

#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Core/Engine.hpp"
#include "MayaFlux/Core/SubsystemManager.hpp"
#include "MayaFlux/Core/Subsystems/AudioSubsystem.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Nodes/Generators/Sine.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

#define INTEGRATION_TEST

namespace MayaFlux::Test {

#ifdef INTEGRATION_TEST

//-------------------------------------------------------------------------
// AudioSubsystem Basic Construction and Lifecycle Tests
//-------------------------------------------------------------------------

class AudioSubsystemTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        node_graph_manager = std::make_shared<Nodes::NodeGraphManager>();
        buffer_manager = std::make_shared<Buffers::BufferManager>(
            TestConfig::NUM_CHANNELS,
            0,
            TestConfig::SAMPLE_RATE,
            TestConfig::BUFFER_SIZE,
            Buffers::ProcessingToken::AUDIO_BACKEND);

        task_scheduler = std::make_shared<Vruta::TaskScheduler>(TestConfig::SAMPLE_RATE, 512);

        stream_info.sample_rate = TestConfig::SAMPLE_RATE;
        stream_info.buffer_size = TestConfig::BUFFER_SIZE;
        stream_info.output.channels = TestConfig::NUM_CHANNELS;
        stream_info.input.enabled = false;
    }

    void TearDown() override
    {
        if (audio_subsystem && audio_subsystem->is_running()) {
            audio_subsystem->stop();
        }
        if (audio_subsystem && audio_subsystem->is_ready()) {
            audio_subsystem->shutdown();
        }
        audio_subsystem.reset();

        task_scheduler.reset();
        buffer_manager.reset();
        node_graph_manager.reset();

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    std::shared_ptr<Core::AudioSubsystem> audio_subsystem;
    std::shared_ptr<Nodes::NodeGraphManager> node_graph_manager;
    std::shared_ptr<Buffers::BufferManager> buffer_manager;
    std::shared_ptr<Vruta::TaskScheduler> task_scheduler;

    Core::GlobalStreamInfo stream_info;
};

TEST_F(AudioSubsystemTest, BasicConstruction)
{
    EXPECT_NO_THROW({
        audio_subsystem = std::make_shared<Core::AudioSubsystem>(stream_info);
    });

    ASSERT_NE(audio_subsystem, nullptr);
    EXPECT_EQ(audio_subsystem->get_type(), Core::SubsystemType::AUDIO);
    EXPECT_FALSE(audio_subsystem->is_ready()) << "New AudioSubsystem should not be ready before initialization";
    EXPECT_FALSE(audio_subsystem->is_running()) << "New AudioSubsystem should not be running";

    const auto& subsystem_stream_info = audio_subsystem->get_stream_info();
    EXPECT_EQ(subsystem_stream_info.sample_rate, TestConfig::SAMPLE_RATE);
    EXPECT_EQ(subsystem_stream_info.buffer_size, TestConfig::BUFFER_SIZE);
    EXPECT_EQ(subsystem_stream_info.output.channels, TestConfig::NUM_CHANNELS);
}

TEST_F(AudioSubsystemTest, ConstructionWithCustomBackend)
{
    EXPECT_NO_THROW({
        audio_subsystem = std::make_shared<Core::AudioSubsystem>(stream_info);
    });

    ASSERT_NE(audio_subsystem, nullptr);
    EXPECT_NE(audio_subsystem->get_audio_backend(), nullptr);
    EXPECT_NE(audio_subsystem->get_device_manager(), nullptr);

    EXPECT_FALSE(audio_subsystem->is_ready());
    EXPECT_FALSE(audio_subsystem->is_running());
}

TEST_F(AudioSubsystemTest, InitializationWithProcessingHandle)
{
    audio_subsystem = std::make_shared<Core::AudioSubsystem>(stream_info);

    Core::SubsystemTokens tokens = {
        .Buffer = Buffers::ProcessingToken::AUDIO_BACKEND,
        .Node = Nodes::ProcessingToken::AUDIO_RATE,
        .Task = Vruta::ProcessingToken::SAMPLE_ACCURATE
    };

    Core::SubsystemProcessingHandle handle(
        buffer_manager, node_graph_manager, task_scheduler, {}, {}, tokens);

    EXPECT_NO_THROW(audio_subsystem->initialize(handle));
    EXPECT_TRUE(audio_subsystem->is_ready()) << "AudioSubsystem should be ready after initialization";

    auto* context_handle = audio_subsystem->get_processing_context_handle();
    EXPECT_NE(context_handle, nullptr);
    EXPECT_EQ(context_handle->get_tokens().Buffer, Buffers::ProcessingToken::AUDIO_BACKEND);
    EXPECT_EQ(context_handle->get_tokens().Node, Nodes::ProcessingToken::AUDIO_RATE);
}

TEST_F(AudioSubsystemTest, CallbackRegistration)
{
    audio_subsystem = std::make_shared<Core::AudioSubsystem>(stream_info);

    Core::SubsystemTokens tokens = {
        .Buffer = Buffers::ProcessingToken::AUDIO_BACKEND,
        .Node = Nodes::ProcessingToken::AUDIO_RATE,
        .Task = Vruta::ProcessingToken::SAMPLE_ACCURATE
    };

    Core::SubsystemProcessingHandle handle(buffer_manager, node_graph_manager, task_scheduler, {}, {}, tokens);

    EXPECT_NO_THROW(audio_subsystem->initialize(handle));
    EXPECT_NO_THROW(audio_subsystem->register_callbacks());

    EXPECT_NE(audio_subsystem->get_stream_manager(), nullptr);
}

TEST_F(AudioSubsystemTest, LifecycleStateTransitions)
{
    audio_subsystem = std::make_shared<Core::AudioSubsystem>(stream_info);

    Core::SubsystemTokens tokens = {
        .Buffer = Buffers::ProcessingToken::AUDIO_BACKEND,
        .Node = Nodes::ProcessingToken::AUDIO_RATE,
        .Task = Vruta::ProcessingToken::SAMPLE_ACCURATE
    };

    Core::SubsystemProcessingHandle handle(buffer_manager, node_graph_manager, task_scheduler, {}, {}, tokens);

    EXPECT_FALSE(audio_subsystem->is_ready());
    EXPECT_FALSE(audio_subsystem->is_running());

    EXPECT_NO_THROW(audio_subsystem->initialize(handle));
    EXPECT_TRUE(audio_subsystem->is_ready());
    EXPECT_FALSE(audio_subsystem->is_running());

    EXPECT_NO_THROW(audio_subsystem->register_callbacks());
    EXPECT_TRUE(audio_subsystem->is_ready());
    EXPECT_FALSE(audio_subsystem->is_running());

    EXPECT_NO_THROW(audio_subsystem->start());

    EXPECT_NO_THROW(audio_subsystem->stop());
    EXPECT_FALSE(audio_subsystem->is_running());
    EXPECT_TRUE(audio_subsystem->is_ready());

    for (int i = 0; i < 3; ++i) {
        EXPECT_NO_THROW(audio_subsystem->start());
        AudioTestHelper::waitForAudio(10);
        EXPECT_NO_THROW(audio_subsystem->stop());
    }

    EXPECT_NO_THROW(audio_subsystem->shutdown());
    EXPECT_FALSE(audio_subsystem->is_ready());
    EXPECT_FALSE(audio_subsystem->is_running());
}

//-------------------------------------------------------------------------
// AudioSubsystem Audio Processing Tests
//-------------------------------------------------------------------------

TEST_F(AudioSubsystemTest, OutputProcessing)
{
    audio_subsystem = std::make_shared<Core::AudioSubsystem>(stream_info);

    Core::SubsystemTokens tokens = {
        .Buffer = Buffers::ProcessingToken::AUDIO_BACKEND,
        .Node = Nodes::ProcessingToken::AUDIO_RATE,
        .Task = Vruta::ProcessingToken::SAMPLE_ACCURATE
    };

    Core::SubsystemProcessingHandle handle(buffer_manager, node_graph_manager, task_scheduler, {}, {}, tokens);

    audio_subsystem->initialize(handle);
    audio_subsystem->register_callbacks();

    std::vector<double> output_buffer(static_cast<size_t>(TestConfig::BUFFER_SIZE) * TestConfig::NUM_CHANNELS, 0.0);

    EXPECT_NO_THROW({
        int result = audio_subsystem->process_output(output_buffer.data(), TestConfig::BUFFER_SIZE);
        EXPECT_EQ(result, 0) << "Output processing should return success";
    });

    EXPECT_EQ(output_buffer.size(), TestConfig::BUFFER_SIZE * TestConfig::NUM_CHANNELS);
}

TEST_F(AudioSubsystemTest, InputProcessing)
{
    stream_info.input.enabled = true;
    stream_info.input.channels = 1;

    audio_subsystem = std::make_shared<Core::AudioSubsystem>(stream_info);

    Core::SubsystemTokens tokens = {
        .Buffer = Buffers::ProcessingToken::AUDIO_BACKEND,
        .Node = Nodes::ProcessingToken::AUDIO_RATE,
        .Task = Vruta::ProcessingToken::SAMPLE_ACCURATE
    };

    Core::SubsystemProcessingHandle handle(buffer_manager, node_graph_manager, task_scheduler, {}, {}, tokens);

    audio_subsystem->initialize(handle);
    audio_subsystem->register_callbacks();

    std::vector<double> input_buffer(TestConfig::BUFFER_SIZE, 0.5);

    EXPECT_NO_THROW({
        int result = audio_subsystem->process_input(input_buffer.data(), TestConfig::BUFFER_SIZE);
        EXPECT_EQ(result, 0) << "Input processing should return success";
    });
}

TEST_F(AudioSubsystemTest, FullDuplexProcessing)
{
    stream_info.input.enabled = true;
    stream_info.input.channels = TestConfig::NUM_CHANNELS;

    audio_subsystem = std::make_shared<Core::AudioSubsystem>(stream_info);

    Core::SubsystemTokens tokens = {
        .Buffer = Buffers::ProcessingToken::AUDIO_BACKEND,
        .Node = Nodes::ProcessingToken::AUDIO_RATE,
        .Task = Vruta::ProcessingToken::SAMPLE_ACCURATE
    };

    Core::SubsystemProcessingHandle handle(buffer_manager, node_graph_manager, task_scheduler, {}, {}, tokens);

    audio_subsystem->initialize(handle);
    audio_subsystem->register_callbacks();

    std::vector<double> input_buffer(static_cast<size_t>(TestConfig::BUFFER_SIZE * TestConfig::NUM_CHANNELS));
    std::vector<double> output_buffer(static_cast<size_t>(TestConfig::BUFFER_SIZE * TestConfig::NUM_CHANNELS), 0.0);

    for (size_t frame = 0; frame < TestConfig::BUFFER_SIZE; ++frame) {
        double t = static_cast<double>(frame) / TestConfig::SAMPLE_RATE;
        for (unsigned int ch = 0; ch < TestConfig::NUM_CHANNELS; ++ch) {
            input_buffer[frame * TestConfig::NUM_CHANNELS + ch] = 0.5 * std::sin(2 * M_PI * 440.0 * t);
        }
    }

    EXPECT_NO_THROW({
        int result = audio_subsystem->process_audio(
            input_buffer.data(), output_buffer.data(), TestConfig::BUFFER_SIZE);
        EXPECT_EQ(result, 0) << "Full duplex processing should return success";
    });

    EXPECT_EQ(output_buffer.size(), TestConfig::BUFFER_SIZE * TestConfig::NUM_CHANNELS);
}

//-------------------------------------------------------------------------
// AudioSubsystem Error Handling and Edge Cases Tests
//-------------------------------------------------------------------------

TEST_F(AudioSubsystemTest, UninitializedOperations)
{
    audio_subsystem = std::make_shared<Core::AudioSubsystem>(stream_info);

    EXPECT_THROW(audio_subsystem->register_callbacks(), std::runtime_error);
    EXPECT_THROW(audio_subsystem->start(), std::runtime_error);

    EXPECT_NO_THROW(audio_subsystem->stop());
    EXPECT_NO_THROW(audio_subsystem->shutdown());
}

TEST_F(AudioSubsystemTest, ProcessingWithNullBuffers)
{
    audio_subsystem = std::make_shared<Core::AudioSubsystem>(stream_info);

    Core::SubsystemTokens tokens = {
        .Buffer = Buffers::ProcessingToken::AUDIO_BACKEND,
        .Node = Nodes::ProcessingToken::AUDIO_RATE,
        .Task = Vruta::ProcessingToken::SAMPLE_ACCURATE
    };

    Core::SubsystemProcessingHandle handle(buffer_manager, node_graph_manager, task_scheduler, {}, {}, tokens);

    audio_subsystem->initialize(handle);
    audio_subsystem->register_callbacks();

    EXPECT_NO_THROW({
        audio_subsystem->process_output(nullptr, TestConfig::BUFFER_SIZE);
    });

    EXPECT_NO_THROW({
        audio_subsystem->process_input(nullptr, TestConfig::BUFFER_SIZE);
    });
}

//-------------------------------------------------------------------------
// AudioSubsystem Backend Integration Tests
//-------------------------------------------------------------------------

TEST_F(AudioSubsystemTest, BackendAccessAndProperties)
{
    audio_subsystem = std::make_shared<Core::AudioSubsystem>(stream_info);

    auto* backend = audio_subsystem->get_audio_backend();
    EXPECT_NE(backend, nullptr);

    auto* device_manager = audio_subsystem->get_device_manager();
    EXPECT_NE(device_manager, nullptr);

    audio_subsystem->get_stream_manager();

    const auto& subsystem_stream_info = audio_subsystem->get_stream_info();
    EXPECT_EQ(subsystem_stream_info.sample_rate, TestConfig::SAMPLE_RATE);
    EXPECT_EQ(subsystem_stream_info.buffer_size, TestConfig::BUFFER_SIZE);
    EXPECT_EQ(subsystem_stream_info.output.channels, TestConfig::NUM_CHANNELS);
}

TEST_F(AudioSubsystemTest, TokenValidation)
{
    audio_subsystem = std::make_shared<Core::AudioSubsystem>(stream_info);

    auto tokens = audio_subsystem->get_tokens();

    EXPECT_EQ(tokens.Buffer, Buffers::ProcessingToken::AUDIO_BACKEND);
    EXPECT_EQ(tokens.Node, Nodes::ProcessingToken::AUDIO_RATE);

    auto tokens2 = audio_subsystem->get_tokens();
    EXPECT_EQ(tokens.Buffer, tokens2.Buffer);
    EXPECT_EQ(tokens.Node, tokens2.Node);
}

//-------------------------------------------------------------------------
// Integration with Engine Tests
//-------------------------------------------------------------------------

TEST_F(AudioSubsystemTest, EngineIntegration)
{
    auto engine = AudioTestHelper::createTestEngine();

    auto engine_subsystem_manager = engine->get_subsystem_manager();
    ASSERT_NE(engine_subsystem_manager, nullptr);

    EXPECT_TRUE(engine_subsystem_manager->has_subsystem(Core::SubsystemType::AUDIO));

    auto audio_subsystem = engine_subsystem_manager->get_audio_subsystem();
    ASSERT_NE(audio_subsystem, nullptr);
    EXPECT_TRUE(audio_subsystem->is_ready());

    EXPECT_NO_THROW(engine->Start());

    auto sine = std::make_shared<Nodes::Generator::Sine>(440.0F, 0.3F);
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
