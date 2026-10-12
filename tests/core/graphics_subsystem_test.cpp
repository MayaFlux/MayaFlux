#include "gtest/gtest.h"

#include <chrono>
#include <thread>

#include "../support/vulkan_test_support.hpp"

#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Core/ProcessingArchitecture.hpp"
#include "MayaFlux/Core/Subsystems/GraphicsSubsystem.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"
#include "MayaFlux/Portal/Graphics/Graphics.hpp"
#include "MayaFlux/Portal/Text/Text.hpp"
#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/BufferService.hpp"
#include "MayaFlux/Vruta/Clock.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

namespace MayaFlux::Test {
namespace {

    using namespace std::chrono_literals;
    using Core::GlobalGraphicsConfig;

    constexpr uint32_t k_sample_rate = 48000;
    constexpr uint32_t k_buffer_size = 512;
    constexpr uint32_t k_target_fps = 120;

    template <typename Predicate>
    bool wait_until(const Predicate& predicate,std::chrono::milliseconds timeout = 3000ms)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (predicate()) {
                return true;
            }
            std::this_thread::sleep_for(1ms);
        }
        return predicate();
    }

    GlobalGraphicsConfig subsystem_config()
    {
        GlobalGraphicsConfig config = headless_graphics_config();
        config.target_frame_rate = k_target_fps;
        return config;
    }

    /**
     * @brief Owns a GraphicsSubsystem together with the managers behind its processing handle.
     *
     * The window and input managers stay null, so the subsystem runs headless.
     */
    class GraphicsSubsystemRig {
    public:
        explicit GraphicsSubsystemRig(const GlobalGraphicsConfig& config = subsystem_config())
            : nodes(std::make_shared<Nodes::NodeGraphManager>())
            , buffers(std::make_shared<Buffers::BufferManager>(1, 0, k_sample_rate, k_buffer_size))
            , tasks(std::make_shared<Vruta::TaskScheduler>(k_sample_rate))
            , subsystem(std::make_unique<Core::GraphicsSubsystem>(config))
            , handle(buffers, nodes, tasks, nullptr, nullptr, subsystem->get_tokens())
        {
        }

        GraphicsSubsystemRig(const GraphicsSubsystemRig&) = delete;
        GraphicsSubsystemRig& operator=(const GraphicsSubsystemRig&) = delete;
        GraphicsSubsystemRig(GraphicsSubsystemRig&&) = delete;
        GraphicsSubsystemRig& operator=(GraphicsSubsystemRig&&) = delete;

        ~GraphicsSubsystemRig()
        {
            if (subsystem->is_ready()) {
                subsystem->shutdown();
            }
        }

        std::shared_ptr<Nodes::NodeGraphManager> nodes;
        std::shared_ptr<Buffers::BufferManager> buffers;
        std::shared_ptr<Vruta::TaskScheduler> tasks;
        std::unique_ptr<Core::GraphicsSubsystem> subsystem;
        Core::SubsystemProcessingHandle handle;
    };

    bool buffer_service_registered()
    {
        return Registry::BackendRegistry::instance().has_service<Registry::Service::BufferService>();
    }

}

TEST(GraphicsSubsystemTest, ConstructorExposesGraphicsTokensAndIsIdle)
{
    GraphicsSubsystemRig rig;

    const auto tokens = rig.subsystem->get_tokens();
    EXPECT_EQ(tokens.Buffer, Buffers::ProcessingToken::GRAPHICS_BACKEND);
    EXPECT_EQ(tokens.Node, Nodes::ProcessingToken::VISUAL_RATE);
    EXPECT_EQ(tokens.Task, Vruta::ProcessingToken::FRAME_ACCURATE);

    EXPECT_EQ(rig.subsystem->get_type(), Core::SubsystemType::GRAPHICS);
    EXPECT_EQ(rig.subsystem->get_backend_type(), GlobalGraphicsConfig::GraphicsApi::VULKAN);
    EXPECT_NE(rig.subsystem->get_backend(), nullptr);
    EXPECT_FALSE(rig.subsystem->is_ready());
    EXPECT_FALSE(rig.subsystem->is_running());
}

TEST(GraphicsSubsystemTest, ProcessingHandleIsNullBeforeInitialize)
{
    GraphicsSubsystemRig rig;
    EXPECT_EQ(rig.subsystem->get_processing_context_handle(), nullptr);
    EXPECT_NO_THROW(rig.subsystem->process());
}

TEST(GraphicsSubsystemTest, UnsupportedApiHasNoBackendAndInitializeThrows)
{
    GlobalGraphicsConfig config = subsystem_config();
    config.requested_api = GlobalGraphicsConfig::GraphicsApi::OPENGL;
    GraphicsSubsystemRig rig(config);

    EXPECT_EQ(rig.subsystem->get_backend(), nullptr);
    EXPECT_THROW(rig.subsystem->initialize(rig.handle), std::runtime_error);
    EXPECT_FALSE(rig.subsystem->is_ready());
    EXPECT_NO_THROW(rig.subsystem->shutdown());
}

TEST(GraphicsSubsystemTest, ShutdownWithoutInitializeIsSafe)
{
    GraphicsSubsystemRig rig;
    EXPECT_NO_THROW(rig.subsystem->shutdown());
    EXPECT_FALSE(rig.subsystem->is_ready());
}

TEST(GraphicsSubsystemTest, RegisterCallbacksBeforeInitializeThrows)
{
    GraphicsSubsystemRig rig;
    EXPECT_THROW(rig.subsystem->register_callbacks(), std::runtime_error);
}

TEST(GraphicsSubsystemTest, InitializeMakesSubsystemReadyAndBringsUpPortal)
{
    REQUIRE_VULKAN();

    GraphicsSubsystemRig rig;
    ASSERT_NO_THROW(rig.subsystem->initialize(rig.handle));

    EXPECT_TRUE(rig.subsystem->is_ready());
    EXPECT_FALSE(rig.subsystem->is_running());
    EXPECT_EQ(rig.subsystem->get_processing_context_handle(), &rig.handle);
    EXPECT_TRUE(Portal::Graphics::is_initialized());
    EXPECT_TRUE(buffer_service_registered());
    EXPECT_EQ(rig.subsystem->get_target_fps(), k_target_fps);
}

TEST(GraphicsSubsystemTest, RegisterCallbacksSucceedsAfterInitialize)
{
    REQUIRE_VULKAN();

    GraphicsSubsystemRig rig;
    ASSERT_NO_THROW(rig.subsystem->initialize(rig.handle));
    EXPECT_NO_THROW(rig.subsystem->register_callbacks());
}

TEST(GraphicsSubsystemTest, ShutdownTearsDownPortalAndBackendServices)
{
    REQUIRE_VULKAN();

    GraphicsSubsystemRig rig;
    ASSERT_NO_THROW(rig.subsystem->initialize(rig.handle));
    rig.subsystem->shutdown();

    EXPECT_FALSE(rig.subsystem->is_ready());
    EXPECT_FALSE(Portal::Graphics::is_initialized());
    EXPECT_FALSE(Portal::Text::is_initialized());
    EXPECT_FALSE(buffer_service_registered());
}

TEST(GraphicsSubsystemTest, SequentialSubsystemsInSameProcess)
{
    REQUIRE_VULKAN();

    for (int i = 0; i < 3; ++i) {
        GraphicsSubsystemRig rig;
        ASSERT_NO_THROW(rig.subsystem->initialize(rig.handle)) << "iteration " << i;
        EXPECT_TRUE(Portal::Graphics::is_initialized());
    }
    EXPECT_FALSE(Portal::Graphics::is_initialized());
}

TEST(GraphicsSubsystemTest, ProcessRunsPreAndPostHooksOncePerCall)
{
    REQUIRE_VULKAN();

    GraphicsSubsystemRig rig;
    ASSERT_NO_THROW(rig.subsystem->initialize(rig.handle));

    int pre_calls = 0;
    int post_calls = 0;
    rig.handle.pre_process_hooks["pre"] = [&pre_calls](uint32_t) { ++pre_calls; };
    rig.handle.post_process_hooks["post"] = [&post_calls](uint32_t) { ++post_calls; };

    rig.subsystem->process();
    rig.subsystem->process();

    EXPECT_EQ(pre_calls, 2);
    EXPECT_EQ(post_calls, 2);
}

TEST(GraphicsSubsystemTest, StartRunsGraphicsThreadThatAdvancesFrameClock)
{
    REQUIRE_VULKAN();

    GraphicsSubsystemRig rig;
    ASSERT_NO_THROW(rig.subsystem->initialize(rig.handle));

    rig.subsystem->start();
    rig.subsystem->wait_until_running();

    EXPECT_TRUE(rig.subsystem->is_running());
    EXPECT_TRUE(wait_until([&] { return rig.subsystem->get_frame_clock().current_position() > 0; }));
    EXPECT_FALSE(rig.subsystem->is_graphics_thread());
    EXPECT_NE(rig.subsystem->get_graphics_thread_id(), std::this_thread::get_id());

    rig.subsystem->stop();
    EXPECT_FALSE(rig.subsystem->is_running());
}

TEST(GraphicsSubsystemTest, PauseHaltsFrameClockAndResumeRestartsIt)
{
    REQUIRE_VULKAN();

    GraphicsSubsystemRig rig;
    ASSERT_NO_THROW(rig.subsystem->initialize(rig.handle));
    rig.subsystem->start();
    rig.subsystem->wait_until_running();
    ASSERT_TRUE(wait_until([&] { return rig.subsystem->get_frame_clock().current_position() > 0; }));

    rig.subsystem->pause();
    EXPECT_FALSE(rig.subsystem->is_running());
    std::this_thread::sleep_for(50ms);
    const uint64_t paused_at = rig.subsystem->get_frame_clock().current_position();
    std::this_thread::sleep_for(100ms);
    EXPECT_EQ(rig.subsystem->get_frame_clock().current_position(), paused_at);

    rig.subsystem->resume();
    EXPECT_TRUE(rig.subsystem->is_running());
    EXPECT_TRUE(wait_until([&] { return rig.subsystem->get_frame_clock().current_position() > paused_at; }));

    rig.subsystem->stop();
}

TEST(GraphicsSubsystemTest, PauseAndResumeWithoutStartAreIgnored)
{
    REQUIRE_VULKAN();

    GraphicsSubsystemRig rig;
    ASSERT_NO_THROW(rig.subsystem->initialize(rig.handle));

    rig.subsystem->pause();
    rig.subsystem->resume();
    EXPECT_FALSE(rig.subsystem->is_running());
}

TEST(GraphicsSubsystemTest, StopWithoutStartIsNoOp)
{
    REQUIRE_VULKAN();

    GraphicsSubsystemRig rig;
    ASSERT_NO_THROW(rig.subsystem->initialize(rig.handle));
    EXPECT_NO_THROW(rig.subsystem->stop());
    EXPECT_TRUE(Portal::Graphics::is_initialized());
}

TEST(GraphicsSubsystemTest, StartTwiceKeepsOneRunningThread)
{
    REQUIRE_VULKAN();

    GraphicsSubsystemRig rig;
    ASSERT_NO_THROW(rig.subsystem->initialize(rig.handle));
    rig.subsystem->start();
    rig.subsystem->wait_until_running();
    const auto thread_id = rig.subsystem->get_graphics_thread_id();

    EXPECT_NO_THROW(rig.subsystem->start());
    EXPECT_EQ(rig.subsystem->get_graphics_thread_id(), thread_id);

    rig.subsystem->stop();
    EXPECT_FALSE(rig.subsystem->is_running());
}

TEST(GraphicsSubsystemTest, StartStopCanRepeat)
{
    REQUIRE_VULKAN();

    GraphicsSubsystemRig rig;
    ASSERT_NO_THROW(rig.subsystem->initialize(rig.handle));

    for (int i = 0; i < 2; ++i) {
        rig.subsystem->start();
        rig.subsystem->wait_until_running();
        EXPECT_TRUE(wait_until([&] { return rig.subsystem->get_frame_clock().current_position() > 0; })) << "iteration " << i;
        rig.subsystem->stop();
        EXPECT_FALSE(rig.subsystem->is_running()) << "iteration " << i;
    }
}

TEST(GraphicsSubsystemTest, SetTargetFpsUpdatesClock)
{
    REQUIRE_VULKAN();

    GraphicsSubsystemRig rig;
    ASSERT_NO_THROW(rig.subsystem->initialize(rig.handle));

    rig.subsystem->set_target_fps(30);
    EXPECT_EQ(rig.subsystem->get_target_fps(), 30U);
    EXPECT_EQ(rig.subsystem->get_frame_clock().frame_rate(), 30U);
}

TEST(GraphicsSubsystemTest, ShutdownWhileRunningStopsThreadFirst)
{
    REQUIRE_VULKAN();

    GraphicsSubsystemRig rig;
    ASSERT_NO_THROW(rig.subsystem->initialize(rig.handle));
    rig.subsystem->start();
    rig.subsystem->wait_until_running();

    EXPECT_NO_THROW(rig.subsystem->shutdown());
    EXPECT_FALSE(rig.subsystem->is_running());
    EXPECT_FALSE(rig.subsystem->is_ready());
    EXPECT_FALSE(Portal::Graphics::is_initialized());
}

}
