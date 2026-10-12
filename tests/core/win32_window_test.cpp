#include "gtest/gtest.h"

#if defined(MAYAFLUX_PLATFORM_WINDOWS)

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <variant>
#include <vector>

#include "MayaFlux/Core/Backends/Windowing/Win32/KeyMapping.hpp"
#include "MayaFlux/Core/Backends/Windowing/Win32/Win32Window.hpp"

namespace MayaFlux::Test {
namespace {

    using namespace std::chrono_literals;
    using Core::WindowEvent;
    using Core::WindowEventType;

    constexpr uint32_t k_scancode_a = 0x1E;

    template <typename Predicate>
    bool eventually(Predicate predicate, std::chrono::milliseconds timeout = 2s)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (predicate())
                return true;
            std::this_thread::sleep_for(2ms);
        }
        return predicate();
    }

    Core::WindowCreateInfo small_window(const std::string& title = "MayaFlux Win32 Test")
    {
        Core::WindowCreateInfo info;
        info.title = title;
        info.width = 320;
        info.height = 240;
        return info;
    }

    class WindowHarness {
    public:
        explicit WindowHarness(
            const Core::WindowCreateInfo& info = small_window(),
            const Core::GlobalGraphicsConfig& config = {})
            : m_window(std::make_unique<Core::Win32Window>(info, config))
        {
            m_window->set_event_callback([this](const WindowEvent& event) {
                m_events.push_back(event);
            });
        }

        WindowHarness(const WindowHarness&) = delete;
        WindowHarness& operator=(const WindowHarness&) = delete;
        WindowHarness(WindowHarness&&) = delete;
        WindowHarness& operator=(WindowHarness&&) = delete;

        Core::Win32Window& window() { return *m_window; }
        [[nodiscard]] HWND hwnd() const { return static_cast<HWND>(m_window->get_native_handle()); }

        LRESULT send(UINT message, WPARAM wparam = 0, LPARAM lparam = 0) const
        {
            return SendMessageW(hwnd(), message, wparam, lparam);
        }

        void poll() { m_window->poll(); }

        [[nodiscard]] std::vector<WindowEvent> events_of(WindowEventType type) const
        {
            std::vector<WindowEvent> matching;
            std::ranges::copy_if(m_events, std::back_inserter(matching), [type](const WindowEvent& event) {
                return event.type == type;
            });
            return matching;
        }

        [[nodiscard]] const std::vector<WindowEvent>& events() const { return m_events; }
        void clear_events() { m_events.clear(); }

    private:
        std::vector<WindowEvent> m_events;
        std::unique_ptr<Core::Win32Window> m_window;
    };

    template <typename Data>
    const Data& data_of(const WindowEvent& event)
    {
        const auto* data = std::get_if<Data>(&event.data);
        EXPECT_NE(data, nullptr);
        static const Data fallback {};
        return data ? *data : fallback;
    }

    LPARAM key_lparam(uint32_t scancode, bool repeat = false)
    {
        return static_cast<LPARAM>((scancode << 16) | 1U | (repeat ? (1U << 30) : 0U));
    }

    std::wstring window_text(HWND hwnd)
    {
        wchar_t buffer[256] {};
        GetWindowTextW(hwnd, buffer, 256);
        return buffer;
    }

    // ------------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------------

    TEST(Win32WindowTest, ConstructorCreatesNativeWindowWithRequestedTitle)
    {
        WindowHarness harness(small_window("MayaFlux Title Test"));

        ASSERT_NE(harness.hwnd(), nullptr);
        EXPECT_TRUE(IsWindow(harness.hwnd()));
        EXPECT_EQ(harness.window().get_native_display(), nullptr);
        EXPECT_EQ(window_text(harness.hwnd()), L"MayaFlux Title Test");
        EXPECT_EQ(harness.window().get_create_info().title, "MayaFlux Title Test");
        EXPECT_FALSE(harness.window().should_close());
    }

    TEST(Win32WindowTest, DestroyClosesNativeWindowAndIsIdempotent)
    {
        WindowHarness harness;
        const auto hwnd = harness.hwnd();
        ASSERT_TRUE(IsWindow(hwnd));

        harness.window().destroy();
        EXPECT_FALSE(IsWindow(hwnd));
        EXPECT_EQ(harness.window().get_native_handle(), nullptr);
        EXPECT_TRUE(harness.window().should_close());

        EXPECT_NO_THROW(harness.window().destroy());
        EXPECT_NO_THROW(harness.window().show());
        EXPECT_NO_THROW(harness.window().hide());
        EXPECT_NO_THROW(harness.window().set_title("after destroy"));
    }

    TEST(Win32WindowTest, PollAfterDestroyDeliversClosedEventOnce)
    {
        WindowHarness harness;
        harness.window().destroy();

        harness.poll();
        EXPECT_EQ(harness.events_of(WindowEventType::WINDOW_CLOSED).size(), 1U);
        harness.poll();
        EXPECT_EQ(harness.events_of(WindowEventType::WINDOW_CLOSED).size(), 1U);
    }

    TEST(Win32WindowTest, DestructionWithoutDestroyReleasesNativeWindow)
    {
        HWND hwnd = nullptr;
        {
            WindowHarness harness;
            hwnd = harness.hwnd();
            ASSERT_TRUE(IsWindow(hwnd));
        }
        EXPECT_FALSE(IsWindow(hwnd));
    }

    TEST(Win32WindowTest, RepeatedCreationAndDestructionInOneProcess)
    {
        for (int round = 0; round < 5; ++round) {
            HWND hwnd = nullptr;
            {
                WindowHarness harness;
                hwnd = harness.hwnd();
                ASSERT_NE(hwnd, nullptr);
                EXPECT_TRUE(IsWindow(hwnd));
            }
            EXPECT_FALSE(IsWindow(hwnd));
        }
    }

    TEST(Win32WindowTest, ConcurrentWindowsAreIndependent)
    {
        WindowHarness first(small_window("MayaFlux First"));
        WindowHarness second(small_window("MayaFlux Second"));
        ASSERT_NE(first.hwnd(), nullptr);
        ASSERT_NE(second.hwnd(), nullptr);
        EXPECT_NE(first.hwnd(), second.hwnd());

        first.send(WM_MOUSEMOVE, 0, MAKELPARAM(10, 20));
        first.poll();
        second.poll();
        EXPECT_EQ(first.events_of(WindowEventType::MOUSE_MOTION).size(), 1U);
        EXPECT_TRUE(second.events_of(WindowEventType::MOUSE_MOTION).empty());

        first.window().destroy();
        EXPECT_TRUE(IsWindow(second.hwnd()));
        EXPECT_FALSE(second.window().should_close());
    }

    // ------------------------------------------------------------------
    // Window state
    // ------------------------------------------------------------------

    TEST(Win32WindowTest, ShowAndHideToggleNativeVisibility)
    {
        WindowHarness harness;
        EXPECT_FALSE(IsWindowVisible(harness.hwnd()));

        harness.window().show();
        EXPECT_TRUE(IsWindowVisible(harness.hwnd()));

        harness.window().hide();
        EXPECT_FALSE(IsWindowVisible(harness.hwnd()));
    }

    TEST(Win32WindowTest, SetTitleUpdatesNativeTextAndCreateInfo)
    {
        WindowHarness harness;
        harness.window().set_title("Renamed Window");

        EXPECT_EQ(window_text(harness.hwnd()), L"Renamed Window");
        EXPECT_EQ(harness.window().get_create_info().title, "Renamed Window");
    }

    TEST(Win32WindowTest, SetSizeUpdatesStateAndEmitsResizeMatchingClientArea)
    {
        WindowHarness harness;
        harness.window().set_size(400, 300);
        harness.poll();

        RECT client {};
        ASSERT_TRUE(GetClientRect(harness.hwnd(), &client));
        const auto width = static_cast<uint32_t>(client.right - client.left);
        const auto height = static_cast<uint32_t>(client.bottom - client.top);
        EXPECT_GT(width, 0U);
        EXPECT_GT(height, 0U);
        EXPECT_LE(width, 400U);
        EXPECT_LE(height, 300U);

        const auto& state = harness.window().get_state();
        EXPECT_EQ(state.current_width, width);
        EXPECT_EQ(state.current_height, height);
        EXPECT_EQ(harness.window().get_create_info().width, 400U);
        EXPECT_EQ(harness.window().get_create_info().height, 300U);

        const auto resized = harness.events_of(WindowEventType::WINDOW_RESIZED);
        ASSERT_FALSE(resized.empty());
        const auto& last = data_of<WindowEvent::ResizeData>(resized.back());
        EXPECT_EQ(last.width, width);
        EXPECT_EQ(last.height, height);
    }

    TEST(Win32WindowTest, SetPositionMovesNativeWindow)
    {
        WindowHarness harness;
        harness.window().set_position(100, 120);

        RECT bounds {};
        ASSERT_TRUE(GetWindowRect(harness.hwnd(), &bounds));
        EXPECT_EQ(bounds.left, 100);
        EXPECT_EQ(bounds.top, 120);
    }

    TEST(Win32WindowTest, FocusMessagesUpdateStateAndEmitEvents)
    {
        WindowHarness harness;
        EXPECT_FALSE(harness.window().get_state().is_focused);

        harness.send(WM_SETFOCUS);
        EXPECT_TRUE(harness.window().get_state().is_focused);
        harness.send(WM_KILLFOCUS);
        EXPECT_FALSE(harness.window().get_state().is_focused);

        harness.poll();
        EXPECT_EQ(harness.events_of(WindowEventType::WINDOW_FOCUS_GAINED).size(), 1U);
        EXPECT_EQ(harness.events_of(WindowEventType::WINDOW_FOCUS_LOST).size(), 1U);
    }

    TEST(Win32WindowTest, CloseMessageRequestsCloseAndDestroysNativeWindow)
    {
        WindowHarness harness;
        const auto hwnd = harness.hwnd();
        EXPECT_FALSE(harness.window().should_close());

        harness.send(WM_CLOSE);
        EXPECT_TRUE(harness.window().should_close());
        EXPECT_FALSE(IsWindow(hwnd));

        harness.poll();
        EXPECT_EQ(harness.events_of(WindowEventType::WINDOW_CLOSED).size(), 1U);
        EXPECT_NO_THROW(harness.window().destroy());
    }

    TEST(Win32WindowTest, InteractiveResizeSuppressesSourceSignalsUntilExit)
    {
        WindowHarness harness;
        auto& source = harness.window().get_event_source();
        source.clear();

        harness.send(WM_ENTERSIZEMOVE);
        harness.window().set_size(380, 280);
        harness.poll();
        EXPECT_FALSE(harness.events_of(WindowEventType::WINDOW_RESIZED).empty());
        EXPECT_EQ(source.pending_count(), 0U);

        harness.send(WM_EXITSIZEMOVE);
        harness.poll();
        EXPECT_EQ(source.pending_count(), 1U);
    }

    TEST(Win32WindowTest, StoresRenderTrackingAndFlagState)
    {
        WindowHarness harness;
        auto& window = harness.window();

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

    // ------------------------------------------------------------------
    // Keyboard
    // ------------------------------------------------------------------

    TEST(Win32WindowTest, KeyDownAndUpEmitEventsAndTrackSourceState)
    {
        WindowHarness harness;
        auto& source = harness.window().get_event_source();

        harness.send(WM_KEYDOWN, 'A', key_lparam(k_scancode_a));
        harness.poll();
        auto pressed = harness.events_of(WindowEventType::KEY_PRESSED);
        ASSERT_EQ(pressed.size(), 1U);
        const auto& down = data_of<WindowEvent::KeyData>(pressed.front());
        EXPECT_EQ(down.key, static_cast<int16_t>(IO::Keys::A));
        EXPECT_EQ(down.scancode, static_cast<int32_t>(k_scancode_a));
        EXPECT_TRUE(source.is_key_pressed(IO::Keys::A));

        harness.send(WM_KEYUP, 'A', key_lparam(k_scancode_a));
        harness.poll();
        auto released = harness.events_of(WindowEventType::KEY_RELEASED);
        ASSERT_EQ(released.size(), 1U);
        EXPECT_EQ(data_of<WindowEvent::KeyData>(released.front()).key, static_cast<int16_t>(IO::Keys::A));
        EXPECT_FALSE(source.is_key_pressed(IO::Keys::A));
    }

    TEST(Win32WindowTest, OsAutoRepeatKeyDownIsIgnored)
    {
        WindowHarness harness;

        harness.send(WM_KEYDOWN, 'A', key_lparam(k_scancode_a));
        harness.send(WM_KEYDOWN, 'A', key_lparam(k_scancode_a, true));
        harness.send(WM_KEYDOWN, 'A', key_lparam(k_scancode_a, true));
        harness.poll();

        EXPECT_EQ(harness.events_of(WindowEventType::KEY_PRESSED).size(), 1U);
    }

    TEST(Win32WindowTest, HeldKeyRepeatsAfterInitialDelayAndStopsOnRelease)
    {
        Core::GlobalGraphicsConfig config;
        config.key_repeat_config.initial_delay_ms = 300;
        config.key_repeat_config.interval_ms = 5;
        WindowHarness harness(small_window(), config);

        harness.send(WM_KEYDOWN, 'A', key_lparam(k_scancode_a));
        harness.poll();
        EXPECT_TRUE(harness.events_of(WindowEventType::KEY_REPEAT).empty());

        ASSERT_TRUE(eventually([&] {
            harness.poll();
            return harness.events_of(WindowEventType::KEY_REPEAT).size() >= 2;
        }, 3s));
        const auto repeat = harness.events_of(WindowEventType::KEY_REPEAT).front();
        EXPECT_EQ(data_of<WindowEvent::KeyData>(repeat).key, static_cast<int16_t>(IO::Keys::A));

        harness.send(WM_KEYUP, 'A', key_lparam(k_scancode_a));
        harness.poll();
        const auto after_release = harness.events_of(WindowEventType::KEY_REPEAT).size();
        std::this_thread::sleep_for(50ms);
        harness.poll();
        EXPECT_EQ(harness.events_of(WindowEventType::KEY_REPEAT).size(), after_release);
    }

    // ------------------------------------------------------------------
    // Text input
    // ------------------------------------------------------------------

    TEST(Win32WindowTest, CharMessageDeliversBasicMultilingualPlaneCodepoint)
    {
        WindowHarness harness;

        harness.send(WM_CHAR, L'a');
        harness.send(WM_CHAR, 0x00E9);
        harness.poll();

        const auto text = harness.events_of(WindowEventType::TEXT_INPUT);
        ASSERT_EQ(text.size(), 2U);
        EXPECT_EQ(data_of<WindowEvent::TextData>(text[0]).codepoint, 0x61U);
        EXPECT_EQ(data_of<WindowEvent::TextData>(text[1]).codepoint, 0xE9U);
    }

    TEST(Win32WindowTest, SurrogatePairIsCombinedIntoSingleCodepoint)
    {
        WindowHarness harness;

        harness.send(WM_CHAR, 0xD83D);
        harness.poll();
        EXPECT_TRUE(harness.events_of(WindowEventType::TEXT_INPUT).empty());

        harness.send(WM_CHAR, 0xDE00);
        harness.poll();
        const auto text = harness.events_of(WindowEventType::TEXT_INPUT);
        ASSERT_EQ(text.size(), 1U);
        EXPECT_EQ(data_of<WindowEvent::TextData>(text.front()).codepoint, 0x1F600U);
    }

    struct RejectedTextCase {
        const char* name;
        uint16_t unit;
    };

    class Win32RejectedTextTest : public ::testing::TestWithParam<RejectedTextCase> { };

    TEST_P(Win32RejectedTextTest, CharMessageProducesNoTextEvent)
    {
        WindowHarness harness;

        harness.send(WM_CHAR, GetParam().unit);
        harness.poll();

        EXPECT_TRUE(harness.events_of(WindowEventType::TEXT_INPUT).empty());
    }

    INSTANTIATE_TEST_SUITE_P(
        Win32Window,
        Win32RejectedTextTest,
        ::testing::Values(
            RejectedTextCase { "Backspace", 0x08 },
            RejectedTextCase { "Delete", 0x7F },
            RejectedTextCase { "OrphanLowSurrogate", 0xDC00 }),
        [](const ::testing::TestParamInfo<RejectedTextCase>& info) { return std::string(info.param.name); });

    // ------------------------------------------------------------------
    // Mouse
    // ------------------------------------------------------------------

    TEST(Win32WindowTest, MouseMoveEmitsPositionAndUpdatesSource)
    {
        WindowHarness harness;

        harness.send(WM_MOUSEMOVE, 0, MAKELPARAM(42, 77));
        harness.poll();

        const auto motion = harness.events_of(WindowEventType::MOUSE_MOTION);
        ASSERT_EQ(motion.size(), 1U);
        const auto& position = data_of<WindowEvent::MousePosData>(motion.front());
        EXPECT_EQ(position.x, 42.0);
        EXPECT_EQ(position.y, 77.0);
        const auto [x, y] = harness.window().get_event_source().get_mouse_position();
        EXPECT_EQ(x, 42.0);
        EXPECT_EQ(y, 77.0);
    }

    struct MouseButtonCase {
        const char* name;
        UINT down_message;
        UINT up_message;
        IO::MouseButtons button;
    };

    class Win32MouseButtonTest : public ::testing::TestWithParam<MouseButtonCase> { };

    TEST_P(Win32MouseButtonTest, PressAndReleaseEmitButtonEventsAndTrackSourceState)
    {
        const auto& param = GetParam();
        WindowHarness harness;
        auto& source = harness.window().get_event_source();

        harness.send(param.down_message, 0, MAKELPARAM(5, 5));
        harness.poll();
        const auto pressed = harness.events_of(WindowEventType::MOUSE_BUTTON_PRESSED);
        ASSERT_EQ(pressed.size(), 1U);
        EXPECT_EQ(data_of<WindowEvent::MouseButtonData>(pressed.front()).button, static_cast<int8_t>(param.button));
        EXPECT_TRUE(source.is_mouse_pressed(static_cast<int>(param.button)));

        harness.send(param.up_message, 0, MAKELPARAM(5, 5));
        harness.poll();
        const auto released = harness.events_of(WindowEventType::MOUSE_BUTTON_RELEASED);
        ASSERT_EQ(released.size(), 1U);
        EXPECT_EQ(data_of<WindowEvent::MouseButtonData>(released.front()).button, static_cast<int8_t>(param.button));
        EXPECT_FALSE(source.is_mouse_pressed(static_cast<int>(param.button)));
    }

    INSTANTIATE_TEST_SUITE_P(
        Win32Window,
        Win32MouseButtonTest,
        ::testing::Values(
            MouseButtonCase { "Left", WM_LBUTTONDOWN, WM_LBUTTONUP, IO::MouseButtons::Left },
            MouseButtonCase { "Right", WM_RBUTTONDOWN, WM_RBUTTONUP, IO::MouseButtons::Right },
            MouseButtonCase { "Middle", WM_MBUTTONDOWN, WM_MBUTTONUP, IO::MouseButtons::Middle }),
        [](const ::testing::TestParamInfo<MouseButtonCase>& info) { return std::string(info.param.name); });

    struct ScrollCase {
        const char* name;
        int16_t delta;
        double expected;
    };

    class Win32ScrollTest : public ::testing::TestWithParam<ScrollCase> { };

    TEST_P(Win32ScrollTest, WheelDeltaIsReportedInNotches)
    {
        WindowHarness harness;

        harness.send(WM_MOUSEWHEEL, MAKEWPARAM(0, static_cast<WORD>(GetParam().delta)), MAKELPARAM(0, 0));
        harness.poll();

        const auto scrolled = harness.events_of(WindowEventType::MOUSE_SCROLLED);
        ASSERT_EQ(scrolled.size(), 1U);
        const auto& scroll = data_of<WindowEvent::ScrollData>(scrolled.front());
        EXPECT_EQ(scroll.x_offset, 0.0);
        EXPECT_EQ(scroll.y_offset, GetParam().expected);
    }

    INSTANTIATE_TEST_SUITE_P(
        Win32Window,
        Win32ScrollTest,
        ::testing::Values(
            ScrollCase { "OneNotchUp", 120, 1.0 },
            ScrollCase { "OneNotchDown", -120, -1.0 },
            ScrollCase { "TwoNotchesUp", 240, 2.0 },
            ScrollCase { "HalfNotchDown", -60, -0.5 }),
        [](const ::testing::TestParamInfo<ScrollCase>& info) { return std::string(info.param.name); });

    // ------------------------------------------------------------------
    // Event queue
    // ------------------------------------------------------------------

    TEST(Win32WindowTest, EventQueueDropsOverflowWithoutBlockingAndRecovers)
    {
        WindowHarness harness;

        for (int i = 0; i < 400; ++i)
            harness.send(WM_MOUSEMOVE, 0, MAKELPARAM(i, 0));
        harness.poll();

        const auto delivered = harness.events_of(WindowEventType::MOUSE_MOTION).size();
        EXPECT_GT(delivered, 0U);
        EXPECT_LE(delivered, 256U);

        harness.clear_events();
        harness.send(WM_MOUSEMOVE, 0, MAKELPARAM(7, 0));
        harness.poll();
        EXPECT_EQ(harness.events_of(WindowEventType::MOUSE_MOTION).size(), 1U);
    }

    TEST(Win32WindowTest, PollPreservesEventOrder)
    {
        WindowHarness harness;

        harness.send(WM_KEYDOWN, 'A', key_lparam(k_scancode_a));
        harness.send(WM_MOUSEMOVE, 0, MAKELPARAM(1, 2));
        harness.send(WM_KEYUP, 'A', key_lparam(k_scancode_a));
        harness.poll();

        const auto& events = harness.events();
        ASSERT_EQ(events.size(), 3U);
        EXPECT_EQ(events[0].type, WindowEventType::KEY_PRESSED);
        EXPECT_EQ(events[1].type, WindowEventType::MOUSE_MOTION);
        EXPECT_EQ(events[2].type, WindowEventType::KEY_RELEASED);
    }

    // ------------------------------------------------------------------
    // Key mapping
    // ------------------------------------------------------------------

    TEST(Win32KeyMappingTest, LettersAndDigitsMapToMatchingKeys)
    {
        EXPECT_EQ(Core::from_win32_key('A'), IO::Keys::A);
        EXPECT_EQ(Core::to_win32_key(IO::Keys::A), static_cast<int>('A'));

        for (WPARAM vk = '0'; vk <= '9'; ++vk) {
            EXPECT_TRUE(Core::is_valid_win32_key(vk));
            EXPECT_EQ(Core::to_win32_key(Core::from_win32_key(vk)), static_cast<int>(vk));
        }
        for (WPARAM vk = 'A'; vk <= 'Z'; ++vk) {
            EXPECT_TRUE(Core::is_valid_win32_key(vk));
            EXPECT_EQ(Core::to_win32_key(Core::from_win32_key(vk)), static_cast<int>(vk));
        }
    }

    TEST(Win32KeyMappingTest, NavigationKeysRoundTripThroughTable)
    {
        EXPECT_EQ(Core::from_win32_key(VK_LEFT), IO::Keys::Left);
        EXPECT_EQ(Core::from_win32_key(VK_RIGHT), IO::Keys::Right);

        for (const WPARAM vk : { static_cast<WPARAM>(VK_RETURN), static_cast<WPARAM>(VK_SPACE),
                 static_cast<WPARAM>(VK_LEFT), static_cast<WPARAM>(VK_RIGHT) }) {
            const auto key = Core::from_win32_key(vk);
            EXPECT_NE(key, IO::Keys::Unknown);
            EXPECT_EQ(Core::to_win32_key(key), static_cast<int>(vk));
        }
    }

    TEST(Win32KeyMappingTest, UnmappedKeysReportUnknownAndZero)
    {
        EXPECT_EQ(Core::to_win32_key(IO::Keys::Unknown), 0);
        EXPECT_EQ(Core::from_win32_key(0), IO::Keys::Unknown);
        EXPECT_FALSE(Core::is_valid_win32_key(0));
        EXPECT_EQ(Core::from_win32_scancode(0), IO::Keys::Unknown);
        EXPECT_EQ(Core::from_win32_scancode(0x1E), IO::Keys::Unknown);
    }

}
}

#endif
