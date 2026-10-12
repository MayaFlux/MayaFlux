#include "gtest/gtest.h"

#if defined(MAYAFLUX_PLATFORM_LINUX) && defined(WAYLAND_BACKEND)

#include <array>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "MayaFlux/Core/Backends/Windowing/Wayland/KeyMapping.hpp"
#include "MayaFlux/Core/Backends/Windowing/Wayland/WaylandWindow.hpp"

#define REQUIRE_COMPOSITOR()                                   \
    do {                                                       \
        if (!MayaFlux::Test::compositor_available())           \
            GTEST_SKIP() << "No Wayland compositor reachable"; \
    } while (0)

namespace MayaFlux::Test {

bool compositor_available()
{
    static const bool available = [] {
        wl_display* display = wl_display_connect(nullptr);
        if (!display)
            return false;
        wl_display_disconnect(display);
        return true;
    }();
    return available;
}

namespace {

    using Core::WindowEvent;
    using Core::WindowEventType;

    Core::WindowCreateInfo small_window(const std::string& title = "MayaFlux Wayland Test")
    {
        Core::WindowCreateInfo info;
        info.title = title;
        info.width = 320;
        info.height = 240;
        return info;
    }

    class ScopedEnv {
    public:
        ScopedEnv(const char* name, const char* value)
            : m_name(name)
        {
            if (const char* previous = std::getenv(name))
                m_previous = previous;
            setenv(name, value, 1);
        }

        ~ScopedEnv()
        {
            if (m_previous)
                setenv(m_name.c_str(), m_previous->c_str(), 1);
            else
                unsetenv(m_name.c_str());
        }

        ScopedEnv(const ScopedEnv&) = delete;
        ScopedEnv& operator=(const ScopedEnv&) = delete;
        ScopedEnv(ScopedEnv&&) = delete;
        ScopedEnv& operator=(ScopedEnv&&) = delete;

    private:
        std::string m_name;
        std::optional<std::string> m_previous;
    };

    std::unique_ptr<Core::WaylandWindow> make_window(
        const Core::WindowCreateInfo& info = small_window())
    {
        return std::make_unique<Core::WaylandWindow>(info, Core::GlobalGraphicsConfig {});
    }

    void pump(Core::WaylandWindow& window, int iterations = 3)
    {
        for (int i = 0; i < iterations; ++i)
            window.poll();
    }

    // ------------------------------------------------------------------
    // Connection failure
    // ------------------------------------------------------------------

    TEST(WaylandWindowTest, UnreachableCompositorLeavesInertWindow)
    {
        const ScopedEnv display("WAYLAND_DISPLAY", "mayaflux-test-no-such-socket");
        const ScopedEnv socket_fd("WAYLAND_SOCKET", "");
        unsetenv("WAYLAND_SOCKET");

        auto window = make_window();

        EXPECT_EQ(window->get_native_display(), nullptr);
        EXPECT_EQ(window->get_native_handle(), nullptr);
        EXPECT_FALSE(window->should_close());

        EXPECT_NO_THROW(window->show());
        EXPECT_NO_THROW(window->hide());
        EXPECT_NO_THROW(window->poll());
        EXPECT_NO_THROW(window->set_title("still safe"));
        EXPECT_NO_THROW(window->set_size(100, 100));
        EXPECT_NO_THROW(window->destroy());
        EXPECT_NO_THROW(window->destroy());
    }

    // ------------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------------

    TEST(WaylandWindowTest, ConstructorCreatesSurfaceAndDisplayWithRequestedTitle)
    {
        REQUIRE_COMPOSITOR();

        auto window = make_window(small_window("MayaFlux Title Test"));

        EXPECT_NE(window->get_native_handle(), nullptr);
        EXPECT_NE(window->get_native_display(), nullptr);
        EXPECT_EQ(window->get_create_info().title, "MayaFlux Title Test");
        EXPECT_FALSE(window->should_close());
    }

    TEST(WaylandWindowTest, InitialStateReflectsRequestedSize)
    {
        REQUIRE_COMPOSITOR();

        auto window = make_window();

        EXPECT_GT(window->get_state().current_width, 0U);
        EXPECT_GT(window->get_state().current_height, 0U);
    }

    TEST(WaylandWindowTest, DestroyReleasesSurfaceAndIsIdempotent)
    {
        REQUIRE_COMPOSITOR();

        auto window = make_window();
        ASSERT_NE(window->get_native_handle(), nullptr);

        window->poll();
        window->destroy();
        EXPECT_EQ(window->get_native_handle(), nullptr);

        EXPECT_NO_THROW(window->destroy());
        EXPECT_EQ(window->get_native_handle(), nullptr);
    }

    TEST(WaylandWindowTest, PollAfterDestroyIsSafe)
    {
        REQUIRE_COMPOSITOR();

        auto window = make_window();
        window->destroy();

        EXPECT_NO_THROW(window->poll());
        EXPECT_NO_THROW(window->poll());
        EXPECT_FALSE(window->should_close());
    }

    TEST(WaylandWindowTest, ShowAndHideAfterDestroyAreSafe)
    {
        REQUIRE_COMPOSITOR();

        auto window = make_window();
        window->destroy();

        EXPECT_NO_THROW(window->show());
        EXPECT_NO_THROW(window->hide());
        EXPECT_NO_THROW(window->set_title("after destroy"));
        EXPECT_NO_THROW(window->set_size(200, 200));
    }

    TEST(WaylandWindowTest, DestructionWithoutDestroyIsClean)
    {
        REQUIRE_COMPOSITOR();

        auto window = make_window();
        window->poll();
        EXPECT_NO_THROW(window.reset());
    }

    TEST(WaylandWindowTest, RepeatedCreationAndDestructionInOneProcess)
    {
        REQUIRE_COMPOSITOR();

        for (int i = 0; i < 5; ++i) {
            auto window = make_window(small_window("Cycle " + std::to_string(i)));
            ASSERT_NE(window->get_native_handle(), nullptr);
            pump(*window);
        }
    }

    TEST(WaylandWindowTest, ConcurrentWindowsAreIndependent)
    {
        REQUIRE_COMPOSITOR();

        auto first = make_window(small_window("First"));
        auto second = make_window(small_window("Second"));

        ASSERT_NE(first->get_native_handle(), nullptr);
        ASSERT_NE(second->get_native_handle(), nullptr);
        EXPECT_NE(first->get_native_handle(), second->get_native_handle());
        EXPECT_NE(first->get_native_display(), second->get_native_display());

        first->destroy();
        EXPECT_EQ(first->get_native_handle(), nullptr);
        EXPECT_NE(second->get_native_handle(), nullptr);
        EXPECT_NO_THROW(pump(*second));
        EXPECT_FALSE(second->should_close());
    }

    // ------------------------------------------------------------------
    // Window interface
    // ------------------------------------------------------------------

    TEST(WaylandWindowTest, PollOnIdleWindowDeliversNoCloseAndKeepsRunning)
    {
        REQUIRE_COMPOSITOR();

        auto window = make_window();

        pump(*window, 20);

        EXPECT_FALSE(window->should_close());
        EXPECT_NE(window->get_native_handle(), nullptr);
    }

    TEST(WaylandWindowTest, ShowAndHideKeepWindowUsable)
    {
        REQUIRE_COMPOSITOR();

        auto window = make_window();

        window->hide();
        pump(*window);
        window->show();
        pump(*window);

        EXPECT_FALSE(window->should_close());
        EXPECT_NE(window->get_native_handle(), nullptr);
    }

    TEST(WaylandWindowTest, SetTitleUpdatesCreateInfo)
    {
        REQUIRE_COMPOSITOR();

        auto window = make_window();

        window->set_title("Renamed");
        pump(*window);

        EXPECT_EQ(window->get_create_info().title, "Renamed");
    }

    TEST(WaylandWindowTest, SetSizeUpdatesCreateInfoAndKeepsWindowAlive)
    {
        REQUIRE_COMPOSITOR();

        auto window = make_window();

        window->set_size(400, 300);
        pump(*window);

        EXPECT_EQ(window->get_create_info().width, 400U);
        EXPECT_EQ(window->get_create_info().height, 300U);
        EXPECT_FALSE(window->should_close());
    }

    TEST(WaylandWindowTest, SetPositionIsIgnoredBecauseClientsCannotPlaceWindows)
    {
        REQUIRE_COMPOSITOR();

        auto window = make_window();
        const auto before = window->get_state();

        EXPECT_NO_THROW(window->set_position(50, 60));
        pump(*window);

        EXPECT_EQ(window->get_state().current_width, before.current_width);
        EXPECT_EQ(window->get_state().current_height, before.current_height);
        EXPECT_FALSE(window->should_close());
    }

    TEST(WaylandWindowTest, EventCallbackReplacementIsAccepted)
    {
        REQUIRE_COMPOSITOR();

        auto window = make_window();
        std::vector<WindowEvent> first;
        std::vector<WindowEvent> second;

        window->set_event_callback([&first](const WindowEvent& event) { first.push_back(event); });
        window->set_event_callback([&second](const WindowEvent& event) { second.push_back(event); });
        pump(*window);

        EXPECT_TRUE(first.empty());
        for (const auto& event : second)
            EXPECT_NE(event.type, WindowEventType::WINDOW_CLOSED);
    }

    TEST(WaylandWindowTest, EventSourceReferenceIsStable)
    {
        REQUIRE_COMPOSITOR();

        auto window = make_window();
        const auto* const_source = &std::as_const(*window).get_event_source();

        EXPECT_EQ(&window->get_event_source(), const_source);
    }

    TEST(WaylandWindowTest, StoresRenderTrackingAndFlagState)
    {
        REQUIRE_COMPOSITOR();

        auto holder = make_window();
        auto& window = *holder;

        EXPECT_FALSE(window.is_graphics_registered());
        window.set_graphics_registered(true);
        EXPECT_TRUE(window.is_graphics_registered());

        EXPECT_FALSE(window.is_capture_enabled());
        window.set_capture_enabled(true);
        EXPECT_TRUE(window.is_capture_enabled());

        EXPECT_TRUE(window.get_frame_commands().empty());
        window.track_frame_command(7);
        window.track_frame_command(9);
        EXPECT_EQ(window.get_frame_commands(), (std::vector<uint64_t> { 7, 9 }));
        window.clear_frame_commands();
        EXPECT_TRUE(window.get_frame_commands().empty());

        EXPECT_TRUE(window.get_rendering_buffers().empty());

        Core::InputConfig input;
        input.keyboard_enabled = false;
        input.cursor_mode = Core::CursorMode::HIDDEN;
        window.set_input_config(input);
        EXPECT_FALSE(window.get_input_config().keyboard_enabled);
        EXPECT_EQ(window.get_input_config().cursor_mode, Core::CursorMode::HIDDEN);

        window.set_color({ 0.25F, 0.5F, 0.75F, 1.0F });
        EXPECT_EQ(window.get_create_info().clear_color, (std::array<float, 4> { 0.25F, 0.5F, 0.75F, 1.0F }));
    }

    TEST(WaylandWindowTest, UnregisteringUnknownRenderingBufferIsSafe)
    {
        REQUIRE_COMPOSITOR();

        auto window = make_window();

        EXPECT_NO_THROW(window->unregister_rendering_buffer(nullptr));
        EXPECT_TRUE(window->get_rendering_buffers().empty());
    }

    // ------------------------------------------------------------------
    // Key mapping
    // ------------------------------------------------------------------

    TEST(WaylandKeyMappingTest, PrintableKeysymsMapToUppercaseKeys)
    {
        EXPECT_EQ(Core::from_xkb_keysym(XKB_KEY_a), IO::Keys::A);
        EXPECT_EQ(Core::from_xkb_keysym(XKB_KEY_A), IO::Keys::A);
        EXPECT_EQ(Core::from_xkb_keysym(XKB_KEY_z), IO::Keys::Z);
        EXPECT_EQ(Core::from_xkb_keysym(XKB_KEY_0), IO::Keys::N0);
        EXPECT_EQ(Core::from_xkb_keysym(XKB_KEY_space), IO::Keys::Space);
    }

    TEST(WaylandKeyMappingTest, NamedKeysRoundTripThroughKeysym)
    {
        constexpr std::array keys {
            IO::Keys::A, IO::Keys::Z, IO::Keys::N0, IO::Keys::N9, IO::Keys::Space,
            IO::Keys::Escape, IO::Keys::Enter, IO::Keys::Tab, IO::Keys::Backspace,
            IO::Keys::Insert, IO::Keys::Delete,
            IO::Keys::Left, IO::Keys::Right, IO::Keys::Up, IO::Keys::Down,
            IO::Keys::PageUp, IO::Keys::PageDown, IO::Keys::Home, IO::Keys::End,
            IO::Keys::CapsLock, IO::Keys::ScrollLock, IO::Keys::NumLock,
            IO::Keys::PrintScreen, IO::Keys::Pause,
            IO::Keys::F1, IO::Keys::F12, IO::Keys::F25,
            IO::Keys::KP0, IO::Keys::KP9, IO::Keys::KPDecimal, IO::Keys::KPDivide,
            IO::Keys::KPMultiply, IO::Keys::KPSubtract, IO::Keys::KPAdd,
            IO::Keys::KPEnter, IO::Keys::KPEqual,
            IO::Keys::LShift, IO::Keys::RShift, IO::Keys::LCtrl, IO::Keys::RCtrl,
            IO::Keys::LAlt, IO::Keys::RAlt, IO::Keys::LSuper, IO::Keys::RSuper,
            IO::Keys::Menu
        };

        for (const IO::Keys key : keys) {
            const xkb_keysym_t sym = Core::to_xkb_keysym(key);
            EXPECT_NE(sym, static_cast<xkb_keysym_t>(XKB_KEY_NoSymbol)) << static_cast<int>(key);
            EXPECT_EQ(Core::from_xkb_keysym(sym), key) << static_cast<int>(key);
        }
    }

    TEST(WaylandKeyMappingTest, ModifierKeysMapToDistinctCodes)
    {
        constexpr std::array syms {
            XKB_KEY_Shift_L, XKB_KEY_Shift_R, XKB_KEY_Control_L, XKB_KEY_Control_R,
            XKB_KEY_Alt_L, XKB_KEY_Alt_R, XKB_KEY_Super_L, XKB_KEY_Super_R
        };

        std::vector<IO::Keys> mapped;
        for (const auto sym : syms) {
            const IO::Keys key = Core::from_xkb_keysym(sym);
            EXPECT_NE(key, IO::Keys::Unknown);
            mapped.push_back(key);
        }

        for (size_t i = 0; i < mapped.size(); ++i)
            for (size_t j = i + 1; j < mapped.size(); ++j)
                EXPECT_NE(mapped[i], mapped[j]);
    }

    TEST(WaylandKeyMappingTest, UnmappedKeysymsReportUnknownAndInvalid)
    {
        constexpr std::array unmapped {
            XKB_KEY_eacute, XKB_KEY_Hyper_L, XKB_KEY_ISO_Left_Tab, XKB_KEY_Multi_key
        };

        for (const auto sym : unmapped) {
            EXPECT_EQ(Core::from_xkb_keysym(sym), IO::Keys::Unknown) << sym;
            EXPECT_FALSE(Core::is_valid_xkb_keysym(sym)) << sym;
        }

        EXPECT_FALSE(Core::is_valid_xkb_keysym(XKB_KEY_NoSymbol));
        EXPECT_EQ(Core::from_xkb_keysym(XKB_KEY_NoSymbol), IO::Keys::Unknown);
    }

    TEST(WaylandKeyMappingTest, ValidKeysymsAreReportedValid)
    {
        EXPECT_TRUE(Core::is_valid_xkb_keysym(XKB_KEY_a));
        EXPECT_TRUE(Core::is_valid_xkb_keysym(XKB_KEY_Escape));
        EXPECT_TRUE(Core::is_valid_xkb_keysym(XKB_KEY_F13));
        EXPECT_TRUE(Core::is_valid_xkb_keysym(XKB_KEY_KP_Enter));
    }

    TEST(WaylandKeyMappingTest, UnmappedKeysConvertToNoSymbol)
    {
        EXPECT_EQ(Core::to_xkb_keysym(IO::Keys::Unknown), static_cast<xkb_keysym_t>(XKB_KEY_NoSymbol));
    }

    struct ScancodeCase {
        const char* name;
        uint32_t scancode;
        IO::Keys key;
    };

    class WaylandEvdevScancodeTest : public ::testing::TestWithParam<ScancodeCase> { };

    TEST_P(WaylandEvdevScancodeTest, KeypadScancodeMapsToPhysicalKey)
    {
        const auto& param = GetParam();
        EXPECT_EQ(Core::from_evdev_scancode(param.scancode), param.key);
    }

    INSTANTIATE_TEST_SUITE_P(
        WaylandKeyMapping,
        WaylandEvdevScancodeTest,
        ::testing::Values(
            ScancodeCase { "KP7", 71, IO::Keys::KP7 },
            ScancodeCase { "KP8", 72, IO::Keys::KP8 },
            ScancodeCase { "KP9", 73, IO::Keys::KP9 },
            ScancodeCase { "KP4", 75, IO::Keys::KP4 },
            ScancodeCase { "KP5", 76, IO::Keys::KP5 },
            ScancodeCase { "KP6", 77, IO::Keys::KP6 },
            ScancodeCase { "KP1", 79, IO::Keys::KP1 },
            ScancodeCase { "KP2", 80, IO::Keys::KP2 },
            ScancodeCase { "KP3", 81, IO::Keys::KP3 },
            ScancodeCase { "KP0", 82, IO::Keys::KP0 },
            ScancodeCase { "KPDecimal", 83, IO::Keys::KPDecimal },
            ScancodeCase { "KPDivide", 98, IO::Keys::KPDivide },
            ScancodeCase { "KPMultiply", 55, IO::Keys::KPMultiply },
            ScancodeCase { "KPSubtract", 74, IO::Keys::KPSubtract },
            ScancodeCase { "KPAdd", 78, IO::Keys::KPAdd },
            ScancodeCase { "KPEnter", 96, IO::Keys::KPEnter },
            ScancodeCase { "KPEqual", 117, IO::Keys::KPEqual },
            ScancodeCase { "LetterAIsNotHandled", 30, IO::Keys::Unknown },
            ScancodeCase { "ZeroIsNotHandled", 0, IO::Keys::Unknown }),
        [](const ::testing::TestParamInfo<ScancodeCase>& info) { return std::string(info.param.name); });

} // namespace
} // namespace MayaFlux::Test

#endif
