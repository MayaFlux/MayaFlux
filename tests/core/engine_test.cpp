#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <thread>

#include "../test_config.h"

#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Core/Engine.hpp"
#include "MayaFlux/Core/SubsystemManager.hpp"
#include "MayaFlux/Core/Subsystems/AudioSubsystem.hpp"
#include "MayaFlux/Core/Subsystems/GraphicsSubsystem.hpp"
#include "MayaFlux/Kriya/Tasks.hpp"
#include "MayaFlux/Nodes/Generators/Sine.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"
#include "MayaFlux/Portal/Forma/Forma.hpp"
#include "MayaFlux/Portal/System/System.hpp"
#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/AudioBackendService.hpp"
#include "MayaFlux/Vruta/Clock.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

namespace MayaFlux::Test {
namespace {

    using namespace std::chrono_literals;

    struct EngineCase {
        const char* name;
        uint32_t sample_rate;
        uint32_t buffer_size;
        uint32_t channels;
        uint32_t input_channels {};
    };

    Core::GlobalStreamInfo stream_for(const EngineCase& test_case)
    {
        Core::GlobalStreamInfo stream;
        stream.sample_rate = test_case.sample_rate;
        stream.buffer_size = test_case.buffer_size;
        stream.output.channels = test_case.channels;
        stream.input.enabled = test_case.input_channels > 0;
        stream.input.channels = test_case.input_channels;
        return stream;
    }

    template <typename Predicate>
    bool eventually(Predicate predicate, std::chrono::milliseconds timeout = 2s)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (predicate()) {
                return true;
            }
            std::this_thread::sleep_for(2ms);
        }
        return predicate();
    }

    struct ShutdownRecord {
        bool initialized {};
        bool started {};
        bool stopped {};
        bool shutdown {};
        bool forma_available_at_stop {};
        bool system_available_at_stop {};
        bool forma_available_at_shutdown {};
    };

    class ShutdownProbe final : public Core::ISubsystem {
    public:
        explicit ShutdownProbe(ShutdownRecord& record)
            : record(record)
        {
        }

        void initialize(Core::SubsystemProcessingHandle& processing_handle) override
        {
            handle = &processing_handle;
            record.initialized = true;
        }

        void register_callbacks() override { }
        void start() override { record.started = true; }
        void stop() override
        {
            record.forma_available_at_stop = Portal::Forma::is_initialized();
            record.system_available_at_stop = Portal::System::is_initialized();
            record.stopped = true;
            record.started = false;
        }
        void pause() override { record.started = false; }
        void resume() override { record.started = true; }
        void shutdown() override
        {
            record.forma_available_at_shutdown = Portal::Forma::is_initialized();
            record.shutdown = true;
        }

        Core::SubsystemTokens get_tokens() const override
        {
            return {
                .Buffer = Buffers::ProcessingToken::AUDIO_BACKEND,
                .Node = Nodes::ProcessingToken::AUDIO_RATE,
                .Task = Vruta::ProcessingToken::SAMPLE_ACCURATE
            };
        }

        bool is_ready() const override { return record.initialized; }
        bool is_running() const override { return record.started; }
        Core::SubsystemType get_type() const override { return Core::SubsystemType::CUSTOM; }
        Core::SubsystemProcessingHandle* get_processing_context_handle() override { return handle; }

    private:
        ShutdownRecord& record;
        Core::SubsystemProcessingHandle* handle {};
    };

    class EngineConfigurationTest : public ::testing::TestWithParam<EngineCase> {
    protected:
        void SetUp() override { engine.Init(stream_for(GetParam())); }
        void TearDown() override
        {
            engine.Start();
            engine.End();
        }

        Core::Engine engine;
    };

    TEST_P(EngineConfigurationTest, AppliesStreamConfiguration)
    {
        const auto& expected = GetParam();
        const auto& actual = engine.get_stream_info();
        EXPECT_TRUE(engine.is_configured());
        EXPECT_FALSE(engine.is_running());
        EXPECT_EQ(actual.sample_rate, expected.sample_rate);
        EXPECT_EQ(actual.buffer_size, expected.buffer_size);
        EXPECT_EQ(actual.output.channels, expected.channels);
        EXPECT_EQ(actual.input.enabled, expected.input_channels > 0);
        EXPECT_EQ(actual.input.channels, expected.input_channels);
    }

    TEST_P(EngineConfigurationTest, WiresCoreManagers)
    {
        ASSERT_NE(engine.get_scheduler(), nullptr);
        ASSERT_NE(engine.get_node_graph_manager(), nullptr);
        ASSERT_NE(engine.get_buffer_manager(), nullptr);
        EXPECT_NE(engine.get_event_manager(), nullptr);
        EXPECT_NE(engine.get_input_manager(), nullptr);
        EXPECT_NE(engine.get_io_manager(), nullptr);
        EXPECT_NE(engine.get_window_manager(), nullptr);
        EXPECT_EQ(engine.get_scheduler()->get_rate(), GetParam().sample_rate);
        EXPECT_EQ(engine.get_buffer_manager()->get_buffer_size(Buffers::ProcessingToken::AUDIO_BACKEND), GetParam().buffer_size);
        EXPECT_EQ(engine.get_buffer_manager()->get_num_channels(Buffers::ProcessingToken::AUDIO_BACKEND), GetParam().channels);
    }

    TEST_P(EngineConfigurationTest, CreatesReadySubsystems)
    {
        auto manager = engine.get_subsystem_manager();
        ASSERT_NE(manager, nullptr);
        ASSERT_NE(manager->get_audio_subsystem(), nullptr);
        ASSERT_NE(manager->get_graphics_subsystem(), nullptr);
        ASSERT_NE(manager->get_input_subsystem(), nullptr);
        ASSERT_NE(manager->get_network_subsystem(), nullptr);
        EXPECT_TRUE(manager->get_audio_subsystem()->is_ready());
        EXPECT_TRUE(manager->get_graphics_subsystem()->is_ready());
        EXPECT_FALSE(manager->get_audio_subsystem()->is_running());
        EXPECT_FALSE(manager->get_graphics_subsystem()->is_running());
        EXPECT_NE(manager->get_audio_subsystem()->get_processing_context_handle(), nullptr);
        EXPECT_NE(manager->get_graphics_subsystem()->get_processing_context_handle(), nullptr);
    }

    TEST_P(EngineConfigurationTest, AllocatesEachOutputChannel)
    {
        auto buffers = engine.get_buffer_manager();
        ASSERT_NE(buffers, nullptr);
        for (uint32_t channel = 0; channel < GetParam().channels; ++channel) {
            auto buffer = buffers->get_root_audio_buffer(Buffers::ProcessingToken::AUDIO_BACKEND, channel);
            ASSERT_NE(buffer, nullptr);
            EXPECT_EQ(buffer->get_channel_id(), channel);
            EXPECT_EQ(buffer->get_num_samples(), GetParam().buffer_size);
        }
    }

    INSTANTIATE_TEST_SUITE_P(StreamLayouts, EngineConfigurationTest,
        ::testing::Values(
            EngineCase { "Stereo48k", 48000, 512, 2 },
            EngineCase { "Mono44k", 44100, 256, 1 },
            EngineCase { "Duplex48k", 48000, 256, 2, 2 }),
        [](const auto& info) { return info.param.name; });

    class EngineRunningTest : public ::testing::TestWithParam<EngineCase> {
    protected:
        void SetUp() override
        {
            engine.Init(stream_for(GetParam()));
            engine.Start();
        }

        void TearDown() override { engine.End(); }

        Core::Engine engine;
    };

    TEST_P(EngineRunningTest, StartsActualAudioAndGraphicsBackends)
    {
        auto manager = engine.get_subsystem_manager();
        ASSERT_NE(manager, nullptr);
        ASSERT_NE(manager->get_audio_subsystem(), nullptr);
        ASSERT_NE(manager->get_graphics_subsystem(), nullptr);
        EXPECT_TRUE(engine.is_running());
        EXPECT_TRUE(manager->get_audio_subsystem()->is_running());
        EXPECT_TRUE(manager->get_graphics_subsystem()->is_running());
        EXPECT_TRUE(Portal::Forma::is_initialized());
        EXPECT_TRUE(Portal::System::is_initialized());
    }

    TEST_P(EngineRunningTest, AdvancesGraphicsFrameClock)
    {
        auto graphics = engine.get_subsystem_manager()->get_graphics_subsystem();
        ASSERT_NE(graphics, nullptr);
        const auto before = graphics->get_frame_clock().current_position();
        EXPECT_TRUE(eventually([&] { return graphics->get_frame_clock().current_position() > before; }));
    }

    TEST_P(EngineRunningTest, ProcessesRealAudioCallbacks)
    {
        auto* service = Registry::BackendRegistry::instance()
                            .get_service<Registry::Service::AudioBackendService>();
        ASSERT_NE(service, nullptr);
        auto callbacks = std::make_shared<std::atomic<uint32_t>>(0);
        const auto channels = GetParam().channels;
        const auto observer = service->register_output_observer(
            [callbacks, channels](const double*, uint32_t samples) {
                if (samples >= channels && samples % channels == 0) {
                    callbacks->fetch_add(1, std::memory_order_release);
                }
            });
        const bool observed = eventually([&] { return callbacks->load(std::memory_order_acquire) >= 2; });
        service->unregister_output_observer(observer);
        EXPECT_TRUE(observed);
    }

    TEST_P(EngineRunningTest, PausesAndResumesSubsystems)
    {
        auto manager = engine.get_subsystem_manager();
        ASSERT_NE(manager, nullptr);
        engine.Pause();
        EXPECT_FALSE(engine.is_running());
        EXPECT_FALSE(manager->get_graphics_subsystem()->is_running());
        engine.Pause();
        engine.Resume();
        EXPECT_TRUE(engine.is_running());
        EXPECT_TRUE(manager->get_graphics_subsystem()->is_running());
        engine.Resume();
    }

    INSTANTIATE_TEST_SUITE_P(RealBackends, EngineRunningTest,
        ::testing::Values(
            EngineCase { "Stereo48k", 48000, 512, 2 },
            EngineCase { "Mono44k", 44100, 256, 1 }),
        [](const auto& info) { return info.param.name; });

    TEST(EngineLifecycleTest, ConstructorHasNoActiveManagers)
    {
        Core::Engine engine;
        EXPECT_FALSE(engine.is_configured());
        EXPECT_FALSE(engine.is_running());
        EXPECT_EQ(engine.get_scheduler(), nullptr);
        EXPECT_EQ(engine.get_node_graph_manager(), nullptr);
        EXPECT_EQ(engine.get_buffer_manager(), nullptr);
        EXPECT_EQ(engine.get_subsystem_manager(), nullptr);
        EXPECT_NE(engine.get_stochastic_engine(), nullptr);
    }

    TEST(EngineLifecycleTest, PauseResumeAndEndBeforeInitAreSafe)
    {
        Core::Engine engine;
        engine.Pause();
        engine.Resume();
        engine.End();
        EXPECT_FALSE(engine.is_configured());
        EXPECT_FALSE(engine.is_running());
    }

    TEST(EngineLifecycleDeathTest, InitWithoutStartCanShutDown)
    {
        GTEST_FLAG_SET(death_test_style, "threadsafe");
        EXPECT_EXIT(
            {
                Core::Engine engine;
                engine.Init(stream_for({ "Stereo48k", 48000, 512, 2 }));
                engine.End();
                if (engine.is_configured() || engine.is_running()) {
                    std::_Exit(1);
                }
                std::_Exit(0);
            },
            ::testing::ExitedWithCode(0), "");
    }

    TEST(EngineLifecycleTest, StartImplicitlyInitializes)
    {
        Core::Engine engine;
        engine.Start();
        ASSERT_TRUE(engine.is_configured());
        EXPECT_TRUE(engine.is_running());
        ASSERT_NE(engine.get_subsystem_manager(), nullptr);
        EXPECT_TRUE(engine.get_subsystem_manager()->get_audio_subsystem()->is_running());
        EXPECT_TRUE(engine.get_subsystem_manager()->get_graphics_subsystem()->is_running());
        engine.End();
        EXPECT_FALSE(engine.is_running());
    }

    TEST(EngineLifecycleTest, EndIsIdempotentAndAllowsRestart)
    {
        Core::Engine engine;
        engine.Start();
        engine.End();
        engine.End();
        EXPECT_FALSE(engine.is_configured());
        engine.Start();
        EXPECT_TRUE(engine.is_running());
        engine.End();
        EXPECT_FALSE(engine.is_running());
    }

    TEST(EngineLifecycleTest, ShutsDownPortalsBeforeStoppingSubsystems)
    {
        Core::Engine engine;
        engine.Init(stream_for({ "Stereo48k", 48000, 512, 2 }));
        auto audio = engine.get_subsystem_manager()->get_audio_subsystem();
        auto graphics = engine.get_subsystem_manager()->get_graphics_subsystem();
        ShutdownRecord record;
        auto probe = std::make_shared<ShutdownProbe>(record);
        engine.get_subsystem_manager()->add_subsystem(Core::SubsystemType::CUSTOM, probe);
        ASSERT_TRUE(record.initialized);
        engine.Start();
        ASSERT_TRUE(record.started);
        engine.End();
        EXPECT_TRUE(record.stopped);
        EXPECT_TRUE(record.shutdown);
        EXPECT_FALSE(record.forma_available_at_stop);
        EXPECT_FALSE(record.system_available_at_stop);
        EXPECT_FALSE(record.forma_available_at_shutdown);
        EXPECT_FALSE(engine.is_running());
        EXPECT_FALSE(audio->is_running());
        EXPECT_FALSE(graphics->is_running());
        EXPECT_FALSE(audio->is_ready());
        EXPECT_FALSE(graphics->is_ready());
        EXPECT_FALSE(Portal::Forma::is_initialized());
        EXPECT_FALSE(Portal::System::is_initialized());
        EXPECT_EQ(Registry::BackendRegistry::instance()
                      .get_service<Registry::Service::AudioBackendService>(),
            nullptr);
    }

    TEST(EngineLifecycleTest, ShutdownRequestIsObservable)
    {
        Core::Engine engine;
        EXPECT_FALSE(engine.is_shutdown_requested());
        engine.request_shutdown();
        EXPECT_TRUE(engine.is_shutdown_requested());
    }

    TEST(EngineLifecycleTest, ExternalManagerReferencesOutliveEngine)
    {
        auto engine = std::make_unique<Core::Engine>();
        engine->Init(stream_for({ "Stereo48k", 48000, 512, 2 }));
        auto scheduler = engine->get_scheduler();
        auto graph = engine->get_node_graph_manager();
        auto buffers = engine->get_buffer_manager();
        engine->Start();
        engine->End();
        engine.reset();
        EXPECT_NE(scheduler, nullptr);
        EXPECT_NE(graph, nullptr);
        EXPECT_NE(buffers, nullptr);
        EXPECT_EQ(scheduler->get_rate(), 48000);
    }

    TEST(EngineLifecycleTest, MoveTransfersConfiguredEngine)
    {
        Core::Engine source;
        source.Init(stream_for({ "Mono44k", 44100, 256, 1 }));
        Core::Engine destination(std::move(source));
        EXPECT_FALSE(source.is_configured());
        EXPECT_TRUE(destination.is_configured());
        ASSERT_NE(destination.get_scheduler(), nullptr);
        EXPECT_EQ(destination.get_scheduler()->get_rate(), 44100);
        destination.Start();
        destination.End();
    }

    TEST(EngineLifecycleTest, RealAudioOutputContainsNodeSignal)
    {
        Core::Engine engine;
        engine.Init(stream_for({ "Stereo48k", 48000, 512, 2 }));
        auto sine = std::make_shared<Nodes::Generator::Sine>(440.0F, 0.5);
        engine.get_node_graph_manager()->add_to_root(sine, Nodes::ProcessingToken::AUDIO_RATE);
        auto* service = Registry::BackendRegistry::instance()
                            .get_service<Registry::Service::AudioBackendService>();
        ASSERT_NE(service, nullptr);
        auto heard_signal = std::make_shared<std::atomic<bool>>(false);
        const auto observer = service->register_output_observer(
            [heard_signal](const double* samples, uint32_t count) {
                if (std::any_of(samples, samples + count,
                        [](double sample) { return std::abs(sample) > 1e-4; })) {
                    heard_signal->store(true, std::memory_order_release);
                }
            });
        engine.Start();
        const bool observed = eventually([&] { return heard_signal->load(std::memory_order_acquire); });
        service->unregister_output_observer(observer);
        engine.End();
        EXPECT_TRUE(observed);
    }

    TEST(EngineLifecycleTest, RealAudioCallbacksAdvanceScheduledTasks)
    {
        Core::Engine engine;
        engine.Init(stream_for({ "Stereo48k", 48000, 512, 2 }));
        auto executions = std::make_shared<int>(0);
        auto routine = Kriya::metro(0.005, [executions] { ++*executions; });
        engine.get_scheduler()->add_task(routine, "", false);
        auto* service = Registry::BackendRegistry::instance()
                            .get_service<Registry::Service::AudioBackendService>();
        ASSERT_NE(service, nullptr);
        auto callbacks = std::make_shared<std::atomic<uint32_t>>(0);
        const auto observer = service->register_output_observer(
            [callbacks](const double*, uint32_t) {
                callbacks->fetch_add(1, std::memory_order_release);
            });
        engine.Start();
        const bool observed = eventually([&] { return callbacks->load(std::memory_order_acquire) >= 8; });
        service->unregister_output_observer(observer);
        engine.End();
        EXPECT_TRUE(observed);
        EXPECT_GT(*executions, 0);
    }

}
}
