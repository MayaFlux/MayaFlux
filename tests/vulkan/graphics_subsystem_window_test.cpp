#include "gtest/gtest.h"

#include "../support/main_thread_support.hpp"
#include "../support/native_window_test_support.hpp"

#if defined(MAYAFLUX_TEST_NATIVE_WINDOWS)

#include <chrono>
#include <thread>

#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Core/ProcessingArchitecture.hpp"
#include "MayaFlux/Core/Subsystems/GraphicsSubsystem.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"
#include "MayaFlux/Vruta/Clock.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

namespace MayaFlux::Test {
namespace {

    using namespace std::chrono_literals;

    constexpr uint32_t k_sample_rate = 48000;
    constexpr uint32_t k_buffer_size = 512;

    template <typename Predicate>
    bool wait_until(const Predicate& predicate, std::chrono::milliseconds timeout = 3000ms)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (predicate()) {
                return true;
            }
            service_main_queue();
        }
        return predicate();
    }

    class WindowedSubsystemRig {
    public:
        WindowedSubsystemRig()
            : config(presenting_graphics_config())
            , nodes(std::make_shared<Nodes::NodeGraphManager>())
            , buffers(std::make_shared<Buffers::BufferManager>(1, 0, k_sample_rate, k_buffer_size))
            , tasks(std::make_shared<Vruta::TaskScheduler>(k_sample_rate))
            , windows(std::make_shared<Core::WindowManager>(config))
            , subsystem(std::make_unique<Core::GraphicsSubsystem>(config))
            , handle(buffers, nodes, tasks, windows, nullptr, subsystem->get_tokens())
        {
        }

        WindowedSubsystemRig(const WindowedSubsystemRig&) = delete;
        WindowedSubsystemRig& operator=(const WindowedSubsystemRig&) = delete;
        WindowedSubsystemRig(WindowedSubsystemRig&&) = delete;
        WindowedSubsystemRig& operator=(WindowedSubsystemRig&&) = delete;

        ~WindowedSubsystemRig()
        {
            if (subsystem->is_ready()) {
                shutdown();
            }
        }

        void stop()
        {
            call_servicing_main_queue([this] { subsystem->stop(); });
        }

        void shutdown()
        {
            call_servicing_main_queue([this] { subsystem->shutdown(); });
        }

        bool bring_up()
        {
            if (!native_display_available()) {
                return false;
            }
            try {
                subsystem->initialize(handle);
            } catch (const std::exception&) {
                return false;
            }
            return true;
        }

        std::shared_ptr<Core::Window> make_window(bool processing = true)
        {
            static int counter = 0;
            auto info = native_window_info("subsystem-window-" + std::to_string(++counter));
            info.register_for_processing = processing;
            return windows->create_window(info);
        }

        [[nodiscard]] bool backend_has(const std::shared_ptr<Core::Window>& window) const
        {
            return subsystem->get_backend()->is_window_registered(window);
        }

        Core::GlobalGraphicsConfig config;
        std::shared_ptr<Nodes::NodeGraphManager> nodes;
        std::shared_ptr<Buffers::BufferManager> buffers;
        std::shared_ptr<Vruta::TaskScheduler> tasks;
        std::shared_ptr<Core::WindowManager> windows;
        std::unique_ptr<Core::GraphicsSubsystem> subsystem;
        Core::SubsystemProcessingHandle handle;
    };

}

#define REQUIRE_WINDOWED_SUBSYSTEM(rig)                                                      \
    do {                                                                                     \
        if (!(rig).bring_up())                                                               \
            GTEST_SKIP() << "No display session or presenting Vulkan backend in this environment"; \
    } while (false)

TEST(GraphicsSubsystemWindowTest, ProcessRegistersProcessingWindowsWithTheBackend)
{
    WindowedSubsystemRig rig;
    REQUIRE_WINDOWED_SUBSYSTEM(rig);
    auto window = rig.make_window();
    ASSERT_NE(window, nullptr);
    EXPECT_FALSE(window->is_graphics_registered());

    rig.subsystem->process();

    EXPECT_TRUE(window->is_graphics_registered());
    EXPECT_TRUE(rig.backend_has(window));
}

TEST(GraphicsSubsystemWindowTest, WindowsOutsideProcessingStayUnregistered)
{
    WindowedSubsystemRig rig;
    REQUIRE_WINDOWED_SUBSYSTEM(rig);
    auto passive = rig.make_window(false);
    ASSERT_NE(passive, nullptr);

    rig.subsystem->process();

    EXPECT_FALSE(passive->is_graphics_registered());
    EXPECT_FALSE(rig.backend_has(passive));
}

TEST(GraphicsSubsystemWindowTest, WindowCreatedAfterInitializeIsPickedUpOnTheNextProcess)
{
    WindowedSubsystemRig rig;
    REQUIRE_WINDOWED_SUBSYSTEM(rig);
    rig.subsystem->process();

    auto late = rig.make_window();
    ASSERT_NE(late, nullptr);
    EXPECT_FALSE(rig.backend_has(late));

    rig.subsystem->process();

    EXPECT_TRUE(rig.backend_has(late));
}

TEST(GraphicsSubsystemWindowTest, RepeatedProcessKeepsARegisteredWindowRenderable)
{
    WindowedSubsystemRig rig;
    REQUIRE_WINDOWED_SUBSYSTEM(rig);
    auto window = rig.make_window();
    ASSERT_NE(window, nullptr);

    for (int i = 0; i < 12; ++i) {
        EXPECT_NO_THROW(rig.subsystem->process()) << "frame " << i;
    }

    EXPECT_TRUE(rig.backend_has(window));
}

TEST(GraphicsSubsystemWindowTest, SeveralWindowsAreRegisteredTogether)
{
    WindowedSubsystemRig rig;
    REQUIRE_WINDOWED_SUBSYSTEM(rig);
    auto first = rig.make_window();
    auto second = rig.make_window();
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);

    rig.subsystem->process();

    EXPECT_TRUE(rig.backend_has(first));
    EXPECT_TRUE(rig.backend_has(second));
}

TEST(GraphicsSubsystemWindowTest, GraphicsThreadRegistersAndPresentsWindows)
{
    WindowedSubsystemRig rig;
    REQUIRE_WINDOWED_SUBSYSTEM(rig);
    auto window = rig.make_window();
    ASSERT_NE(window, nullptr);

    rig.subsystem->start();
    rig.subsystem->wait_until_running();

    EXPECT_TRUE(wait_until([&] { return window->is_graphics_registered(); }));
    EXPECT_TRUE(wait_until([&] { return rig.subsystem->get_frame_clock().current_position() > 5; }));

    rig.stop();
}

TEST(GraphicsSubsystemWindowTest, StopClearsTheRegistrationFlagOfEveryWindow)
{
    WindowedSubsystemRig rig;
    REQUIRE_WINDOWED_SUBSYSTEM(rig);
    auto window = rig.make_window();
    ASSERT_NE(window, nullptr);
    rig.subsystem->start();
    rig.subsystem->wait_until_running();
    ASSERT_TRUE(wait_until([&] { return window->is_graphics_registered(); }));

    rig.stop();

    EXPECT_FALSE(window->is_graphics_registered());
}

TEST(GraphicsSubsystemWindowTest, WindowsKeepRenderingAfterTheSubsystemRestarts)
{
    WindowedSubsystemRig rig;
    REQUIRE_WINDOWED_SUBSYSTEM(rig);
    auto window = rig.make_window();
    ASSERT_NE(window, nullptr);
    rig.subsystem->start();
    rig.subsystem->wait_until_running();
    ASSERT_TRUE(wait_until([&] { return window->is_graphics_registered(); }));
    rig.stop();

    rig.subsystem->start();
    rig.subsystem->wait_until_running();

    EXPECT_TRUE(wait_until([&] { return window->is_graphics_registered(); }));
    EXPECT_TRUE(rig.backend_has(window));
    rig.stop();
}

TEST(GraphicsSubsystemWindowTest, ShutdownUnregistersEveryWindowFromTheBackend)
{
    WindowedSubsystemRig rig;
    REQUIRE_WINDOWED_SUBSYSTEM(rig);
    auto window = rig.make_window();
    ASSERT_NE(window, nullptr);
    rig.subsystem->process();
    ASSERT_TRUE(window->is_graphics_registered());

    rig.shutdown();

    EXPECT_FALSE(window->is_graphics_registered());
    EXPECT_FALSE(rig.subsystem->is_ready());
}

TEST(GraphicsSubsystemWindowTest, ShutdownWhileRunningWithWindowsIsClean)
{
    WindowedSubsystemRig rig;
    REQUIRE_WINDOWED_SUBSYSTEM(rig);
    auto window = rig.make_window();
    ASSERT_NE(window, nullptr);
    rig.subsystem->start();
    rig.subsystem->wait_until_running();
    ASSERT_TRUE(wait_until([&] { return window->is_graphics_registered(); }));

    EXPECT_NO_THROW(rig.shutdown());

    EXPECT_FALSE(window->is_graphics_registered());
    EXPECT_FALSE(rig.subsystem->is_running());
}

}

#endif
