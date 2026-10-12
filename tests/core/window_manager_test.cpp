#include "gtest/gtest.h"

#if defined(MAYAFLUX_PLATFORM_LINUX) && defined(WAYLAND_BACKEND)

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>

#include <wayland-client.h>

#include "MayaFlux/Core/Windowing/WindowManager.hpp"

namespace MayaFlux::Test {
namespace {

    bool compositor_reachable()
    {
        static const bool reachable = [] {
            wl_display* display = wl_display_connect(nullptr);
            if (!display)
                return false;
            wl_display_disconnect(display);
            return true;
        }();
        return reachable;
    }

#define REQUIRE_WM_COMPOSITOR()                                \
    do {                                                       \
        if (!compositor_reachable())                           \
            GTEST_SKIP() << "No Wayland compositor reachable"; \
    } while (0)

    class ScopedEnv {
    public:
        ScopedEnv(const char* name, const char* value)
            : m_name(name)
        {
            if (const char* previous = std::getenv(name)) {
                m_previous = previous;
            }
            if (value) {
                setenv(name, value, 1);
            } else {
                unsetenv(name);
            }
        }

        ~ScopedEnv()
        {
            if (m_previous) {
                setenv(m_name.c_str(), m_previous->c_str(), 1);
            } else {
                unsetenv(m_name.c_str());
            }
        }

        ScopedEnv(const ScopedEnv&) = delete;
        ScopedEnv& operator=(const ScopedEnv&) = delete;
        ScopedEnv(ScopedEnv&&) = delete;
        ScopedEnv& operator=(ScopedEnv&&) = delete;

    private:
        std::string m_name;
        std::optional<std::string> m_previous;
    };

    Core::WindowCreateInfo window_info(const std::string& title, bool processing = true)
    {
        Core::WindowCreateInfo info;
        info.title = title;
        info.width = 320;
        info.height = 240;
        info.register_for_processing = processing;
        return info;
    }

    bool contains(const std::vector<std::shared_ptr<Core::Window>>& windows,
        const std::shared_ptr<Core::Window>& window)
    {
        return std::ranges::find(windows, window) != windows.end();
    }

    // ------------------------------------------------------------------
    // Creation and lookup
    // ------------------------------------------------------------------

    TEST(WindowManagerTest, CreateWindowRegistersWindowForLookup)
    {
        REQUIRE_WM_COMPOSITOR();
        Core::WindowManager manager { Core::GlobalGraphicsConfig {} };

        auto window = manager.create_window(window_info("Alpha"));

        ASSERT_NE(window, nullptr);
        EXPECT_EQ(manager.window_count(), 1U);
        EXPECT_EQ(manager.find_window("Alpha"), window);
        EXPECT_EQ(manager.get_window(0), window);
        EXPECT_EQ(manager.get_windows().size(), 1U);
        EXPECT_NE(window->get_native_handle(), nullptr);
    }

    TEST(WindowManagerTest, DuplicateTitleReturnsNullAndKeepsFirstWindow)
    {
        REQUIRE_WM_COMPOSITOR();
        Core::WindowManager manager { Core::GlobalGraphicsConfig {} };

        auto first = manager.create_window(window_info("Alpha"));
        ASSERT_NE(first, nullptr);

        EXPECT_EQ(manager.create_window(window_info("Alpha")), nullptr);
        EXPECT_EQ(manager.window_count(), 1U);
        EXPECT_EQ(manager.find_window("Alpha"), first);
    }

    TEST(WindowManagerTest, LookupMissesReturnNull)
    {
        REQUIRE_WM_COMPOSITOR();
        Core::WindowManager manager { Core::GlobalGraphicsConfig {} };
        auto window = manager.create_window(window_info("Alpha"));
        ASSERT_NE(window, nullptr);

        EXPECT_EQ(manager.find_window("Missing"), nullptr);
        EXPECT_EQ(manager.get_window(1), nullptr);
        EXPECT_EQ(manager.get_window(100), nullptr);
    }

    TEST(WindowManagerTest, WindowsAreReturnedInCreationOrder)
    {
        REQUIRE_WM_COMPOSITOR();
        Core::WindowManager manager { Core::GlobalGraphicsConfig {} };

        auto first = manager.create_window(window_info("First"));
        auto second = manager.create_window(window_info("Second"));
        ASSERT_NE(first, nullptr);
        ASSERT_NE(second, nullptr);

        EXPECT_EQ(manager.get_window(0), first);
        EXPECT_EQ(manager.get_window(1), second);
        EXPECT_EQ(manager.get_windows(), (std::vector<std::shared_ptr<Core::Window>> { first, second }));
    }

    TEST(WindowManagerTest, ProcessingRegistrationFollowsCreateInfo)
    {
        REQUIRE_WM_COMPOSITOR();
        Core::WindowManager manager { Core::GlobalGraphicsConfig {} };

        auto processed = manager.create_window(window_info("Processed", true));
        auto passive = manager.create_window(window_info("Passive", false));
        ASSERT_NE(processed, nullptr);
        ASSERT_NE(passive, nullptr);

        const auto processing = manager.get_processing_windows();
        EXPECT_EQ(manager.window_count(), 2U);
        EXPECT_TRUE(contains(processing, processed));
        EXPECT_FALSE(contains(processing, passive));
    }

    TEST(WindowManagerTest, UnsupportedBackendCreatesNothing)
    {
        Core::GlobalGraphicsConfig config;
        config.windowing_backend = Core::GlobalGraphicsConfig::WindowingBackend::NONE;
        Core::WindowManager manager { config };

        EXPECT_EQ(manager.create_window(window_info("Alpha")), nullptr);
        EXPECT_EQ(manager.window_count(), 0U);
        EXPECT_EQ(manager.find_window("Alpha"), nullptr);
    }

    TEST(WindowManagerTest, UnreachableCompositorCreatesNothing)
    {
        const ScopedEnv display("WAYLAND_DISPLAY", "mayaflux-test-no-such-socket");
        const ScopedEnv socket_fd("WAYLAND_SOCKET", nullptr);
        Core::WindowManager manager { Core::GlobalGraphicsConfig {} };

        EXPECT_EQ(manager.create_window(window_info("Alpha")), nullptr);
        EXPECT_EQ(manager.window_count(), 0U);
        EXPECT_EQ(manager.find_window("Alpha"), nullptr);
        EXPECT_TRUE(manager.get_processing_windows().empty());
    }

    TEST(WindowManagerTest, FailedCreationDoesNotReserveTitle)
    {
        REQUIRE_WM_COMPOSITOR();
        Core::WindowManager manager { Core::GlobalGraphicsConfig {} };

        {
            const ScopedEnv display("WAYLAND_DISPLAY", "mayaflux-test-no-such-socket");
            const ScopedEnv socket_fd("WAYLAND_SOCKET", nullptr);
            ASSERT_EQ(manager.create_window(window_info("Alpha")), nullptr);
        }

        auto window = manager.create_window(window_info("Alpha"));
        ASSERT_NE(window, nullptr);
        EXPECT_EQ(manager.window_count(), 1U);
    }

    // ------------------------------------------------------------------
    // Destruction
    // ------------------------------------------------------------------

    TEST(WindowManagerTest, DestroyWindowUnregistersFromEveryView)
    {
        REQUIRE_WM_COMPOSITOR();
        Core::WindowManager manager { Core::GlobalGraphicsConfig {} };
        auto window = manager.create_window(window_info("Alpha"));
        ASSERT_NE(window, nullptr);

        manager.destroy_window(window);

        EXPECT_EQ(manager.window_count(), 0U);
        EXPECT_EQ(manager.find_window("Alpha"), nullptr);
        EXPECT_EQ(manager.get_window(0), nullptr);
        EXPECT_FALSE(contains(manager.get_processing_windows(), window));
    }

    TEST(WindowManagerTest, DestroyWindowWithoutBackendCleanupKeepsNativeSurface)
    {
        REQUIRE_WM_COMPOSITOR();
        Core::WindowManager manager { Core::GlobalGraphicsConfig {} };
        auto window = manager.create_window(window_info("Alpha"));
        ASSERT_NE(window, nullptr);

        manager.destroy_window(window);

        EXPECT_NE(window->get_native_handle(), nullptr);
    }

    TEST(WindowManagerTest, DestroyWindowWithBackendCleanupReleasesNativeSurface)
    {
        REQUIRE_WM_COMPOSITOR();
        Core::WindowManager manager { Core::GlobalGraphicsConfig {} };
        auto window = manager.create_window(window_info("Alpha"));
        ASSERT_NE(window, nullptr);

        manager.destroy_window(window, true);

        EXPECT_EQ(manager.window_count(), 0U);
        EXPECT_EQ(window->get_native_handle(), nullptr);
    }

    TEST(WindowManagerTest, DestroyWindowAfterTerminateStillReleasesNativeSurface)
    {
        REQUIRE_WM_COMPOSITOR();
        Core::WindowManager manager { Core::GlobalGraphicsConfig {} };
        auto window = manager.create_window(window_info("Alpha"));
        ASSERT_NE(window, nullptr);

        manager.set_terminate();
        manager.destroy_window(window, true);

        EXPECT_EQ(manager.window_count(), 0U);
        EXPECT_EQ(window->get_native_handle(), nullptr);
    }

    TEST(WindowManagerTest, DestroyWindowIgnoresNull)
    {
        Core::WindowManager manager { Core::GlobalGraphicsConfig {} };

        EXPECT_NO_THROW(manager.destroy_window(nullptr));
        EXPECT_NO_THROW(manager.destroy_window(nullptr, true));
        EXPECT_EQ(manager.window_count(), 0U);
    }

    TEST(WindowManagerTest, DestroyByTitleRemovesOnlyThatWindow)
    {
        REQUIRE_WM_COMPOSITOR();
        Core::WindowManager manager { Core::GlobalGraphicsConfig {} };
        auto first = manager.create_window(window_info("First"));
        auto second = manager.create_window(window_info("Second"));
        ASSERT_NE(first, nullptr);
        ASSERT_NE(second, nullptr);

        EXPECT_TRUE(manager.destroy_window_by_title("First"));
        EXPECT_FALSE(manager.destroy_window_by_title("First"));
        EXPECT_FALSE(manager.destroy_window_by_title("Missing"));

        EXPECT_EQ(manager.window_count(), 1U);
        EXPECT_EQ(manager.find_window("First"), nullptr);
        EXPECT_EQ(manager.find_window("Second"), second);
    }

    TEST(WindowManagerTest, DestroyedTitleCanBeReused)
    {
        REQUIRE_WM_COMPOSITOR();
        Core::WindowManager manager { Core::GlobalGraphicsConfig {} };
        auto first = manager.create_window(window_info("Alpha"));
        ASSERT_NE(first, nullptr);

        manager.destroy_window(first);
        auto second = manager.create_window(window_info("Alpha"));

        ASSERT_NE(second, nullptr);
        EXPECT_NE(second, first);
        EXPECT_EQ(manager.find_window("Alpha"), second);
    }

    TEST(WindowManagerTest, FreshWindowsAreNotClosedAndCloseSweepDestroysNothing)
    {
        REQUIRE_WM_COMPOSITOR();
        Core::WindowManager manager { Core::GlobalGraphicsConfig {} };
        auto window = manager.create_window(window_info("Alpha"));
        ASSERT_NE(window, nullptr);

        EXPECT_FALSE(manager.any_window_should_close());
        EXPECT_EQ(manager.destroy_closed_windows(), 0U);
        EXPECT_EQ(manager.window_count(), 1U);
    }

    TEST(WindowManagerTest, DestructionReleasesManagedWindows)
    {
        REQUIRE_WM_COMPOSITOR();
        std::weak_ptr<Core::Window> weak;
        {
            Core::WindowManager manager { Core::GlobalGraphicsConfig {} };
            weak = manager.create_window(window_info("Alpha"));
            ASSERT_FALSE(weak.expired());
        }

        EXPECT_TRUE(weak.expired());
    }

    // ------------------------------------------------------------------
    // Processing
    // ------------------------------------------------------------------

    TEST(WindowManagerTest, ProcessReportsWhetherWindowsRemain)
    {
        REQUIRE_WM_COMPOSITOR();
        Core::WindowManager manager { Core::GlobalGraphicsConfig {} };

        EXPECT_FALSE(manager.process());

        auto window = manager.create_window(window_info("Alpha"));
        ASSERT_NE(window, nullptr);
        EXPECT_TRUE(manager.process());
        EXPECT_FALSE(window->should_close());

        manager.destroy_window(window);
        EXPECT_FALSE(manager.process());
    }

    TEST(WindowManagerTest, ProcessRunsFrameHookEachCallUntilUnregistered)
    {
        Core::WindowManager manager { Core::GlobalGraphicsConfig {} };
        int calls = 0;

        manager.register_frame_hook("counter", [&calls] { ++calls; });
        manager.process();
        manager.process();
        EXPECT_EQ(calls, 2);

        manager.unregister_frame_hook("counter");
        manager.process();
        EXPECT_EQ(calls, 2);
    }

} // namespace
} // namespace MayaFlux::Test

#endif
