#include "gtest/gtest.h"

#if defined(MAYAFLUX_PLATFORM_MACOS) && defined(COCOA_BACKEND)

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

#include "MayaFlux/Core/Backends/Windowing/Cocoa/CocoaWindow.hpp"
#include "MayaFlux/Core/Backends/Windowing/Cocoa/KeyMapping.hpp"

#include "../support/cocoa_event_injector.hpp"

#define REQUIRE_DISPLAY()                                                      \
    do {                                                                       \
        if (!CocoaInput::display_available())                                  \
            GTEST_SKIP() << "No window server session with an active display"; \
    } while (false)

namespace MayaFlux::Test {
namespace {

    using namespace std::chrono_literals;
    using CocoaInput::Modifier;
    using Core::WindowEvent;
    using Core::WindowEventType;

    template <typename Predicate>
    bool eventually(Predicate predicate, std::chrono::milliseconds timeout = 2s)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (predicate())
                return true;
            CocoaInput::pump_main_queue();
        }
        return predicate();
    }

    Core::WindowCreateInfo small_window(const std::string& title = "MayaFlux Cocoa Test")
    {
        Core::WindowCreateInfo info;
        info.title = title;
        info.width = 320;
        info.height = 240;
        return info;
    }

    uint16_t key_code(IO::Keys key)
    {
        return static_cast<uint16_t>(Core::to_cocoa_key(key));
    }

    class WindowHarness {
    public:
        explicit WindowHarness(
            const Core::WindowCreateInfo& info = small_window(),
            const Core::GlobalGraphicsConfig& config = {})
            : m_window(std::make_unique<Core::CocoaWindow>(info, config))
        {
            m_window->set_event_callback([this](const WindowEvent& event) {
                m_events.push_back(event);
            });
        }

        WindowHarness(const WindowHarness&) = delete;
        WindowHarness& operator=(const WindowHarness&) = delete;
        WindowHarness(WindowHarness&&) = delete;
        WindowHarness& operator=(WindowHarness&&) = delete;

        Core::CocoaWindow& window() { return *m_window; }
        [[nodiscard]] void* native() const { return m_window->get_native_handle(); }

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
        std::unique_ptr<Core::CocoaWindow> m_window;
    };

    template <typename Data>
    const Data& data_of(const WindowEvent& event)
    {
        const auto* data = std::get_if<Data>(&event.data);
        EXPECT_NE(data, nullptr);
        static const Data fallback {};
        return data ? *data : fallback;
    }

    TEST(CocoaWindowTest, ConstructorCreatesNativeWindowWithRequestedTitleAndSize)
    {
        REQUIRE_DISPLAY();
        WindowHarness harness(small_window("MayaFlux Title Test"));

        ASSERT_NE(harness.native(), nullptr);
        EXPECT_EQ(harness.window().get_native_display(), nullptr);
        EXPECT_EQ(CocoaInput::title(harness.native()), "MayaFlux Title Test");
        EXPECT_EQ(harness.window().get_create_info().title, "MayaFlux Title Test");
        EXPECT_EQ(CocoaInput::content_size(harness.native()), (std::pair<double, double> { 320.0, 240.0 }));
        EXPECT_EQ(harness.window().get_state().current_width, 320U);
        EXPECT_EQ(harness.window().get_state().current_height, 240U);
        EXPECT_FALSE(harness.window().get_state().is_visible);
        EXPECT_FALSE(CocoaInput::is_visible(harness.native()));
        EXPECT_FALSE(harness.window().should_close());
    }

    TEST(CocoaWindowTest, MetalLayerBacksContentViewAndTracksWindowScale)
    {
        REQUIRE_DISPLAY();
        WindowHarness harness;

        void* layer = harness.window().get_metal_layer();
        ASSERT_NE(layer, nullptr);
        EXPECT_TRUE(CocoaInput::is_metal_layer(layer));
        EXPECT_EQ(CocoaInput::layer_scale(layer), CocoaInput::backing_scale(harness.native()));
    }

    TEST(CocoaWindowTest, DestroyReleasesNativeWindowAndIsIdempotent)
    {
        REQUIRE_DISPLAY();
        WindowHarness harness;
        const auto retained = CocoaInput::retain(harness.native());
        ASSERT_NE(retained, nullptr);

        harness.window().show();
        ASSERT_TRUE(eventually([&] { return CocoaInput::is_visible(retained.get()); }));

        harness.window().destroy();
        EXPECT_EQ(harness.window().get_native_handle(), nullptr);
        EXPECT_EQ(harness.window().get_metal_layer(), nullptr);
        EXPECT_FALSE(CocoaInput::is_visible(retained.get()));
        EXPECT_FALSE(CocoaInput::has_delegate(retained.get()));

        EXPECT_NO_THROW(harness.window().destroy());
        EXPECT_NO_THROW(harness.window().show());
        EXPECT_NO_THROW(harness.window().hide());
        EXPECT_NO_THROW(harness.window().set_title("after destroy"));
        EXPECT_NO_THROW(harness.window().set_size(100, 100));
        EXPECT_NO_THROW(harness.window().set_position(10, 10));
        CocoaInput::pump_main_queue();
        EXPECT_FALSE(CocoaInput::is_visible(retained.get()));
    }

    TEST(CocoaWindowTest, PollAfterDestroyIsSafeAndDeliversNothing)
    {
        REQUIRE_DISPLAY();
        WindowHarness harness;
        harness.window().destroy();

        EXPECT_NO_THROW(harness.poll());
        EXPECT_TRUE(harness.events().empty());
    }

    TEST(CocoaWindowTest, DestructionWithoutDestroyReleasesNativeWindow)
    {
        REQUIRE_DISPLAY();
        std::shared_ptr<void> retained;
        {
            WindowHarness harness;
            retained = CocoaInput::retain(harness.native());
            ASSERT_NE(retained, nullptr);

            harness.window().show();
            ASSERT_TRUE(eventually([&] { return CocoaInput::is_visible(retained.get()); }));
        }
        EXPECT_FALSE(CocoaInput::is_visible(retained.get()));
        EXPECT_FALSE(CocoaInput::has_delegate(retained.get()));
    }

    TEST(CocoaWindowTest, RepeatedCreationAndDestructionInOneProcess)
    {
        REQUIRE_DISPLAY();
        for (int round = 0; round < 5; ++round) {
            std::shared_ptr<void> retained;
            {
                WindowHarness harness;
                retained = CocoaInput::retain(harness.native());
                ASSERT_NE(retained, nullptr);
            }
            EXPECT_FALSE(CocoaInput::has_delegate(retained.get()));
        }
    }

    TEST(CocoaWindowTest, ConcurrentWindowsAreIndependent)
    {
        REQUIRE_DISPLAY();
        WindowHarness first(small_window("MayaFlux First"));
        WindowHarness second(small_window("MayaFlux Second"));
        ASSERT_NE(first.native(), nullptr);
        ASSERT_NE(second.native(), nullptr);
        EXPECT_NE(first.native(), second.native());

        CocoaInput::mouse_move(first.native(), 10.0, 20.0);
        first.poll();
        second.poll();
        EXPECT_EQ(first.events_of(WindowEventType::MOUSE_MOTION).size(), 1U);
        EXPECT_TRUE(second.events_of(WindowEventType::MOUSE_MOTION).empty());

        first.window().destroy();
        EXPECT_NE(second.native(), nullptr);
        EXPECT_FALSE(second.window().should_close());
    }

    TEST(CocoaWindowTest, CreateAndDestroyOnWorkerThreadCompleteWhileMainPumpsRunLoop)
    {
        REQUIRE_DISPLAY();
        std::unique_ptr<Core::CocoaWindow> window;
        std::atomic<int> stage { 0 };

        std::thread worker([&] {
            window = std::make_unique<Core::CocoaWindow>(small_window(), Core::GlobalGraphicsConfig {});
            stage.store(1);

            const auto deadline = std::chrono::steady_clock::now() + 10s;
            while (stage.load() < 2 && std::chrono::steady_clock::now() < deadline)
                std::this_thread::sleep_for(1ms);

            if (window)
                window->destroy();
            stage.store(3);
        });

        struct WorkerGuard {
            std::thread& worker;
            std::atomic<int>& stage;

            WorkerGuard(std::thread& thread, std::atomic<int>& state)
                : worker(thread)
                , stage(state)
            {
            }

            WorkerGuard(const WorkerGuard&) = delete;
            WorkerGuard& operator=(const WorkerGuard&) = delete;

            ~WorkerGuard()
            {
                const auto deadline = std::chrono::steady_clock::now() + 10s;
                while (stage.load() != 3 && std::chrono::steady_clock::now() < deadline) {
                    int created = 1;
                    stage.compare_exchange_strong(created, 2);
                    CocoaInput::pump_main_queue();
                }
                if (worker.joinable())
                    worker.join();
            }
        } guard { worker, stage };

        ASSERT_TRUE(eventually([&] { return stage.load() >= 1; }, 5s));
        EXPECT_NE(window->get_native_handle(), nullptr);

        stage.store(2);
        ASSERT_TRUE(eventually([&] { return stage.load() == 3; }, 5s));
        EXPECT_EQ(window->get_native_handle(), nullptr);
    }

    TEST(CocoaWindowTest, ShowAndHideToggleNativeVisibilityAndState)
    {
        REQUIRE_DISPLAY();
        WindowHarness harness;
        EXPECT_FALSE(CocoaInput::is_visible(harness.native()));

        harness.window().show();
        EXPECT_TRUE(harness.window().get_state().is_visible);
        EXPECT_TRUE(eventually([&] { return CocoaInput::is_visible(harness.native()); }));

        harness.window().hide();
        EXPECT_FALSE(harness.window().get_state().is_visible);
        EXPECT_TRUE(eventually([&] { return !CocoaInput::is_visible(harness.native()); }));
    }

    TEST(CocoaWindowTest, SetTitleUpdatesNativeTitleAndCreateInfo)
    {
        REQUIRE_DISPLAY();
        WindowHarness harness;
        harness.window().set_title("Renamed Window");

        EXPECT_EQ(harness.window().get_create_info().title, "Renamed Window");
        EXPECT_TRUE(eventually([&] { return CocoaInput::title(harness.native()) == "Renamed Window"; }));
    }

    TEST(CocoaWindowTest, SetSizeUpdatesStateAndEmitsPointAndPixelResizes)
    {
        REQUIRE_DISPLAY();
        WindowHarness harness;
        harness.window().set_size(400, 300);

        ASSERT_TRUE(eventually([&] {
            return CocoaInput::content_size(harness.native()) == std::pair<double, double> { 400.0, 300.0 };
        }));
        harness.poll();

        const auto& state = harness.window().get_state();
        EXPECT_EQ(state.current_width, 400U);
        EXPECT_EQ(state.current_height, 300U);
        EXPECT_EQ(harness.window().get_create_info().width, 400U);
        EXPECT_EQ(harness.window().get_create_info().height, 300U);

        const auto resized = harness.events_of(WindowEventType::WINDOW_RESIZED);
        ASSERT_FALSE(resized.empty());
        EXPECT_EQ(data_of<WindowEvent::ResizeData>(resized.back()).width, 400U);
        EXPECT_EQ(data_of<WindowEvent::ResizeData>(resized.back()).height, 300U);

        const auto scale = CocoaInput::backing_scale(harness.native());
        const auto framebuffer = harness.events_of(WindowEventType::FRAMEBUFFER_RESIZED);
        ASSERT_FALSE(framebuffer.empty());
        EXPECT_EQ(data_of<WindowEvent::ResizeData>(framebuffer.back()).width, static_cast<uint32_t>(400 * scale));
        EXPECT_EQ(data_of<WindowEvent::ResizeData>(framebuffer.back()).height, static_cast<uint32_t>(300 * scale));
    }

    TEST(CocoaWindowTest, SetPositionMovesContentAreaUsingTopLeftCoordinates)
    {
        REQUIRE_DISPLAY();
        WindowHarness harness;
        harness.window().set_position(100, 120);

        EXPECT_TRUE(eventually([&] {
            return CocoaInput::content_origin(harness.native()) == std::pair<double, double> { 100.0, 120.0 };
        }));
    }

    TEST(CocoaWindowTest, FocusNotificationsUpdateStateAndEmitEvents)
    {
        REQUIRE_DISPLAY();
        WindowHarness harness;
        EXPECT_FALSE(harness.window().get_state().is_focused);

        CocoaInput::focus_gained(harness.native());
        harness.poll();
        EXPECT_TRUE(harness.window().get_state().is_focused);

        CocoaInput::focus_lost(harness.native());
        harness.poll();
        EXPECT_FALSE(harness.window().get_state().is_focused);

        EXPECT_EQ(harness.events_of(WindowEventType::WINDOW_FOCUS_GAINED).size(), 1U);
        EXPECT_EQ(harness.events_of(WindowEventType::WINDOW_FOCUS_LOST).size(), 1U);
    }

    TEST(CocoaWindowTest, CloseRequestFlagsWindowAndLeavesDestructionToOwner)
    {
        REQUIRE_DISPLAY();
        WindowHarness harness;
        EXPECT_FALSE(harness.window().should_close());

        EXPECT_FALSE(CocoaInput::close_requested(harness.native()));
        EXPECT_TRUE(harness.window().should_close());
        EXPECT_NE(harness.native(), nullptr);

        harness.poll();
        EXPECT_EQ(harness.events_of(WindowEventType::WINDOW_CLOSED).size(), 1U);
        EXPECT_NO_THROW(harness.window().destroy());
        EXPECT_EQ(harness.native(), nullptr);
    }

    TEST(CocoaWindowTest, MinimizeAndRestoreUpdateStateAndEmitEvents)
    {
        REQUIRE_DISPLAY();
        WindowHarness harness;

        CocoaInput::minimized(harness.native());
        harness.poll();
        EXPECT_TRUE(harness.window().get_state().is_minimized);
        EXPECT_EQ(harness.events_of(WindowEventType::WINDOW_MINIMIZED).size(), 1U);

        CocoaInput::restored(harness.native());
        harness.poll();
        EXPECT_FALSE(harness.window().get_state().is_minimized);
        EXPECT_EQ(harness.events_of(WindowEventType::WINDOW_RESTORED).size(), 1U);
    }

    TEST(CocoaWindowTest, LiveResizeSuppressesResizeEventsUntilDragEnds)
    {
        REQUIRE_DISPLAY();
        WindowHarness harness;
        harness.poll();
        harness.clear_events();

        CocoaInput::live_resize_began(harness.native());
        harness.window().set_size(380, 280);
        ASSERT_TRUE(eventually([&] {
            return CocoaInput::content_size(harness.native()) == std::pair<double, double> { 380.0, 280.0 };
        }));
        harness.poll();
        EXPECT_TRUE(harness.events_of(WindowEventType::WINDOW_RESIZED).empty());
        EXPECT_TRUE(harness.events_of(WindowEventType::FRAMEBUFFER_RESIZED).empty());

        CocoaInput::live_resize_ended(harness.native());
        harness.poll();
        const auto resized = harness.events_of(WindowEventType::WINDOW_RESIZED);
        ASSERT_EQ(resized.size(), 1U);
        EXPECT_EQ(data_of<WindowEvent::ResizeData>(resized.front()).width, 380U);
        EXPECT_EQ(data_of<WindowEvent::ResizeData>(resized.front()).height, 280U);
        EXPECT_EQ(harness.events_of(WindowEventType::FRAMEBUFFER_RESIZED).size(), 1U);
        EXPECT_EQ(harness.window().get_state().current_width, 380U);
    }

    TEST(CocoaWindowTest, StoresRenderTrackingAndFlagState)
    {
        REQUIRE_DISPLAY();
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

    TEST(CocoaWindowTest, DisabledKeyboardFiltersKeyAndTextEventsOnly)
    {
        REQUIRE_DISPLAY();
        WindowHarness harness;
        Core::InputConfig input;
        input.keyboard_enabled = false;
        harness.window().set_input_config(input);

        CocoaInput::key_down(harness.native(), key_code(IO::Keys::A));
        CocoaInput::key_up(harness.native(), key_code(IO::Keys::A));
        CocoaInput::insert_text(harness.native(), u"a");
        CocoaInput::mouse_move(harness.native(), 5.0, 5.0);
        CocoaInput::focus_gained(harness.native());
        harness.poll();

        EXPECT_TRUE(harness.events_of(WindowEventType::KEY_PRESSED).empty());
        EXPECT_TRUE(harness.events_of(WindowEventType::KEY_RELEASED).empty());
        EXPECT_TRUE(harness.events_of(WindowEventType::TEXT_INPUT).empty());
        EXPECT_EQ(harness.events_of(WindowEventType::MOUSE_MOTION).size(), 1U);
        EXPECT_EQ(harness.events_of(WindowEventType::WINDOW_FOCUS_GAINED).size(), 1U);
    }

    TEST(CocoaWindowTest, DisabledMouseFiltersMouseEventsOnly)
    {
        REQUIRE_DISPLAY();
        WindowHarness harness;
        Core::InputConfig input;
        input.mouse_enabled = false;
        harness.window().set_input_config(input);

        CocoaInput::mouse_move(harness.native(), 5.0, 5.0);
        CocoaInput::mouse_button(harness.native(), 0, true);
        CocoaInput::mouse_button(harness.native(), 0, false);
        CocoaInput::scroll_precise(harness.native(), 0, 20);
        CocoaInput::mouse_entered(harness.native());
        CocoaInput::key_down(harness.native(), key_code(IO::Keys::A));
        harness.poll();

        EXPECT_TRUE(harness.events_of(WindowEventType::MOUSE_MOTION).empty());
        EXPECT_TRUE(harness.events_of(WindowEventType::MOUSE_BUTTON_PRESSED).empty());
        EXPECT_TRUE(harness.events_of(WindowEventType::MOUSE_BUTTON_RELEASED).empty());
        EXPECT_TRUE(harness.events_of(WindowEventType::MOUSE_SCROLLED).empty());
        EXPECT_TRUE(harness.events_of(WindowEventType::MOUSE_ENTERED).empty());
        EXPECT_EQ(harness.events_of(WindowEventType::KEY_PRESSED).size(), 1U);
    }

    TEST(CocoaWindowTest, KeyDownAndUpEmitEventsAndTrackSourceState)
    {
        REQUIRE_DISPLAY();
        WindowHarness harness;
        auto& source = harness.window().get_event_source();
        const auto code = key_code(IO::Keys::A);

        CocoaInput::key_down(harness.native(), code);
        harness.poll();
        const auto pressed = harness.events_of(WindowEventType::KEY_PRESSED);
        ASSERT_EQ(pressed.size(), 1U);
        const auto& down = data_of<WindowEvent::KeyData>(pressed.front());
        EXPECT_EQ(down.key, static_cast<int16_t>(IO::Keys::A));
        EXPECT_EQ(down.scancode, static_cast<int32_t>(code));
        EXPECT_EQ(down.mods, 0);
        EXPECT_TRUE(source.is_key_pressed(IO::Keys::A));

        CocoaInput::key_up(harness.native(), code);
        harness.poll();
        const auto released = harness.events_of(WindowEventType::KEY_RELEASED);
        ASSERT_EQ(released.size(), 1U);
        EXPECT_EQ(data_of<WindowEvent::KeyData>(released.front()).key, static_cast<int16_t>(IO::Keys::A));
        EXPECT_FALSE(source.is_key_pressed(IO::Keys::A));
    }

    TEST(CocoaWindowTest, OsAutoRepeatKeyDownIsIgnored)
    {
        REQUIRE_DISPLAY();
        WindowHarness harness;
        const auto code = key_code(IO::Keys::A);

        CocoaInput::key_down(harness.native(), code);
        CocoaInput::key_down(harness.native(), code, Modifier::None, true);
        CocoaInput::key_down(harness.native(), code, Modifier::None, true);
        harness.poll();

        EXPECT_EQ(harness.events_of(WindowEventType::KEY_PRESSED).size(), 1U);
    }

    TEST(CocoaWindowTest, HeldKeyRepeatsAfterInitialDelayAndStopsOnRelease)
    {
        REQUIRE_DISPLAY();
        Core::GlobalGraphicsConfig config;
        config.key_repeat_config.initial_delay_ms = 300;
        config.key_repeat_config.interval_ms = 5;
        WindowHarness harness(small_window(), config);
        const auto code = key_code(IO::Keys::A);

        CocoaInput::key_down(harness.native(), code);
        harness.poll();
        EXPECT_TRUE(harness.events_of(WindowEventType::KEY_REPEAT).empty());

        ASSERT_TRUE(eventually([&] {
            harness.poll();
            return harness.events_of(WindowEventType::KEY_REPEAT).size() >= 2;
        },
            3s));
        const auto repeat = harness.events_of(WindowEventType::KEY_REPEAT).front();
        EXPECT_EQ(data_of<WindowEvent::KeyData>(repeat).key, static_cast<int16_t>(IO::Keys::A));

        CocoaInput::key_up(harness.native(), code);
        harness.poll();
        const auto after_release = harness.events_of(WindowEventType::KEY_REPEAT).size();
        std::this_thread::sleep_for(50ms);
        harness.poll();
        EXPECT_EQ(harness.events_of(WindowEventType::KEY_REPEAT).size(), after_release);
    }

    struct ModifierCase {
        const char* name;
        Modifier flag;
        int32_t expected_mods;
    };

    class CocoaModifierMaskTest : public ::testing::TestWithParam<ModifierCase> { };

    TEST_P(CocoaModifierMaskTest, KeyEventsCarryModifierBits)
    {
        REQUIRE_DISPLAY();
        const auto& param = GetParam();
        WindowHarness harness;
        const auto code = key_code(IO::Keys::A);

        CocoaInput::key_down(harness.native(), code, param.flag);
        CocoaInput::key_up(harness.native(), code, param.flag);
        harness.poll();

        const auto pressed = harness.events_of(WindowEventType::KEY_PRESSED);
        const auto released = harness.events_of(WindowEventType::KEY_RELEASED);
        ASSERT_EQ(pressed.size(), 1U);
        ASSERT_EQ(released.size(), 1U);
        EXPECT_EQ(data_of<WindowEvent::KeyData>(pressed.front()).mods, param.expected_mods);
        EXPECT_EQ(data_of<WindowEvent::KeyData>(released.front()).mods, param.expected_mods);
    }

    INSTANTIATE_TEST_SUITE_P(
        CocoaWindow,
        CocoaModifierMaskTest,
        ::testing::Values(
            ModifierCase { "Shift", Modifier::Shift, 0x0001 },
            ModifierCase { "Control", Modifier::Control, 0x0002 },
            ModifierCase { "Option", Modifier::Option, 0x0004 },
            ModifierCase { "Command", Modifier::Command, 0x0008 },
            ModifierCase { "CapsLock", Modifier::CapsLock, 0x0010 },
            ModifierCase { "ShiftAndCommand", Modifier::Shift | Modifier::Command, 0x0009 }),
        [](const ::testing::TestParamInfo<ModifierCase>& info) { return std::string(info.param.name); });

    struct ModifierKeyCase {
        const char* name;
        IO::Keys key;
        Modifier flag;
        int32_t expected_mods;
    };

    class CocoaModifierKeyTest : public ::testing::TestWithParam<ModifierKeyCase> { };

    TEST_P(CocoaModifierKeyTest, FlagsChangedEmitsPressThenRelease)
    {
        REQUIRE_DISPLAY();
        const auto& param = GetParam();
        WindowHarness harness;
        auto& source = harness.window().get_event_source();
        const auto code = key_code(param.key);

        CocoaInput::flags_changed(harness.native(), code, param.flag);
        harness.poll();
        const auto pressed = harness.events_of(WindowEventType::KEY_PRESSED);
        ASSERT_EQ(pressed.size(), 1U);
        EXPECT_EQ(data_of<WindowEvent::KeyData>(pressed.front()).key, static_cast<int16_t>(param.key));
        EXPECT_EQ(data_of<WindowEvent::KeyData>(pressed.front()).mods, param.expected_mods);
        EXPECT_TRUE(source.is_key_pressed(param.key));

        CocoaInput::flags_changed(harness.native(), code, Modifier::None);
        harness.poll();
        const auto released = harness.events_of(WindowEventType::KEY_RELEASED);
        ASSERT_EQ(released.size(), 1U);
        EXPECT_EQ(data_of<WindowEvent::KeyData>(released.front()).key, static_cast<int16_t>(param.key));
        EXPECT_FALSE(source.is_key_pressed(param.key));
    }

    INSTANTIATE_TEST_SUITE_P(
        CocoaWindow,
        CocoaModifierKeyTest,
        ::testing::Values(
            ModifierKeyCase { "LeftShift", IO::Keys::LShift, Modifier::Shift, 0x0001 },
            ModifierKeyCase { "RightShift", IO::Keys::RShift, Modifier::Shift, 0x0001 },
            ModifierKeyCase { "LeftControl", IO::Keys::LCtrl, Modifier::Control, 0x0002 },
            ModifierKeyCase { "RightControl", IO::Keys::RCtrl, Modifier::Control, 0x0002 },
            ModifierKeyCase { "LeftOption", IO::Keys::LAlt, Modifier::Option, 0x0004 },
            ModifierKeyCase { "RightOption", IO::Keys::RAlt, Modifier::Option, 0x0004 },
            ModifierKeyCase { "LeftCommand", IO::Keys::LSuper, Modifier::Command, 0x0008 },
            ModifierKeyCase { "RightCommand", IO::Keys::RSuper, Modifier::Command, 0x0008 }),
        [](const ::testing::TestParamInfo<ModifierKeyCase>& info) { return std::string(info.param.name); });

    TEST(CocoaWindowTest, CapsLockToggleReportsPressFollowedByReleaseAndNeverRepeats)
    {
        REQUIRE_DISPLAY();
        WindowHarness harness;
        auto& source = harness.window().get_event_source();

        CocoaInput::flags_changed(harness.native(), key_code(IO::Keys::CapsLock), Modifier::CapsLock);
        harness.poll();

        ASSERT_EQ(harness.events().size(), 2U);
        EXPECT_EQ(harness.events()[0].type, WindowEventType::KEY_PRESSED);
        EXPECT_EQ(harness.events()[1].type, WindowEventType::KEY_RELEASED);
        EXPECT_EQ(data_of<WindowEvent::KeyData>(harness.events()[0]).key, static_cast<int16_t>(IO::Keys::CapsLock));
        EXPECT_FALSE(source.is_key_pressed(IO::Keys::CapsLock));

        std::this_thread::sleep_for(150ms);
        harness.poll();
        EXPECT_TRUE(harness.events_of(WindowEventType::KEY_REPEAT).empty());
    }

    TEST(CocoaWindowTest, FocusLossReleasesHeldKeysBeforeFocusLostEvent)
    {
        REQUIRE_DISPLAY();
        WindowHarness harness;
        auto& source = harness.window().get_event_source();

        CocoaInput::key_down(harness.native(), key_code(IO::Keys::A));
        CocoaInput::focus_lost(harness.native());
        harness.poll();

        const auto& events = harness.events();
        ASSERT_EQ(events.size(), 3U);
        EXPECT_EQ(events[0].type, WindowEventType::KEY_PRESSED);
        EXPECT_EQ(events[1].type, WindowEventType::KEY_RELEASED);
        EXPECT_EQ(events[2].type, WindowEventType::WINDOW_FOCUS_LOST);
        EXPECT_EQ(data_of<WindowEvent::KeyData>(events[1]).key, static_cast<int16_t>(IO::Keys::A));
        EXPECT_FALSE(source.is_key_pressed(IO::Keys::A));

        harness.clear_events();
        std::this_thread::sleep_for(150ms);
        harness.poll();
        EXPECT_TRUE(harness.events_of(WindowEventType::KEY_REPEAT).empty());
    }

    TEST(CocoaWindowTest, TextInputDeliversBasicMultilingualPlaneCodepoints)
    {
        REQUIRE_DISPLAY();
        WindowHarness harness;

        CocoaInput::insert_text(harness.native(), u"aé");
        harness.poll();

        const auto text = harness.events_of(WindowEventType::TEXT_INPUT);
        ASSERT_EQ(text.size(), 2U);
        EXPECT_EQ(data_of<WindowEvent::TextData>(text[0]).codepoint, 0x61U);
        EXPECT_EQ(data_of<WindowEvent::TextData>(text[1]).codepoint, 0xE9U);
    }

    TEST(CocoaWindowTest, SurrogatePairIsCombinedIntoSingleCodepoint)
    {
        REQUIRE_DISPLAY();
        WindowHarness harness;

        CocoaInput::insert_text(harness.native(), std::u16string { char16_t(0xD83D), char16_t(0xDE00) });
        harness.poll();

        const auto text = harness.events_of(WindowEventType::TEXT_INPUT);
        ASSERT_EQ(text.size(), 1U);
        EXPECT_EQ(data_of<WindowEvent::TextData>(text.front()).codepoint, 0x1F600U);
    }

    struct RejectedTextCase {
        const char* name;
        char16_t unit;
    };

    class CocoaRejectedTextTest : public ::testing::TestWithParam<RejectedTextCase> { };

    TEST_P(CocoaRejectedTextTest, CommittedTextProducesNoTextEvent)
    {
        REQUIRE_DISPLAY();
        WindowHarness harness;

        CocoaInput::insert_text(harness.native(), std::u16string(1, GetParam().unit));
        harness.poll();

        EXPECT_TRUE(harness.events_of(WindowEventType::TEXT_INPUT).empty());
    }

    INSTANTIATE_TEST_SUITE_P(
        CocoaWindow,
        CocoaRejectedTextTest,
        ::testing::Values(
            RejectedTextCase { "Backspace", char16_t(0x08) },
            RejectedTextCase { "Delete", char16_t(0x7F) },
            RejectedTextCase { "OrphanHighSurrogate", char16_t(0xD83D) },
            RejectedTextCase { "OrphanLowSurrogate", char16_t(0xDC00) }),
        [](const ::testing::TestParamInfo<RejectedTextCase>& info) { return std::string(info.param.name); });

    TEST(CocoaWindowTest, MouseMoveEmitsTopLeftPositionAndUpdatesSource)
    {
        REQUIRE_DISPLAY();
        WindowHarness harness;

        CocoaInput::mouse_move(harness.native(), 42.0, 77.0);
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
        IO::MouseButtons button;
    };

    class CocoaMouseButtonTest : public ::testing::TestWithParam<MouseButtonCase> { };

    TEST_P(CocoaMouseButtonTest, PressAndReleaseEmitButtonEventsAndTrackSourceState)
    {
        REQUIRE_DISPLAY();
        const auto number = static_cast<int>(GetParam().button);
        WindowHarness harness;
        auto& source = harness.window().get_event_source();

        CocoaInput::mouse_button(harness.native(), number, true);
        harness.poll();
        const auto pressed = harness.events_of(WindowEventType::MOUSE_BUTTON_PRESSED);
        ASSERT_EQ(pressed.size(), 1U);
        EXPECT_EQ(data_of<WindowEvent::MouseButtonData>(pressed.front()).button, static_cast<int8_t>(number));
        EXPECT_TRUE(source.is_mouse_pressed(number));

        CocoaInput::mouse_button(harness.native(), number, false);
        harness.poll();
        const auto released = harness.events_of(WindowEventType::MOUSE_BUTTON_RELEASED);
        ASSERT_EQ(released.size(), 1U);
        EXPECT_EQ(data_of<WindowEvent::MouseButtonData>(released.front()).button, static_cast<int8_t>(number));
        EXPECT_FALSE(source.is_mouse_pressed(number));
    }

    INSTANTIATE_TEST_SUITE_P(
        CocoaWindow,
        CocoaMouseButtonTest,
        ::testing::Values(
            MouseButtonCase { "Left", IO::MouseButtons::Left },
            MouseButtonCase { "Right", IO::MouseButtons::Right },
            MouseButtonCase { "Middle", IO::MouseButtons::Middle },
            MouseButtonCase { "Button4", IO::MouseButtons::Button4 },
            MouseButtonCase { "Button5", IO::MouseButtons::Button5 },
            MouseButtonCase { "Button8", IO::MouseButtons::Button8 }),
        [](const ::testing::TestParamInfo<MouseButtonCase>& info) { return std::string(info.param.name); });

    TEST(CocoaWindowTest, MouseButtonBeyondButton8IsIgnored)
    {
        REQUIRE_DISPLAY();
        WindowHarness harness;

        CocoaInput::mouse_button(harness.native(), static_cast<int>(IO::MouseButtons::Button8) + 1, true);
        harness.poll();

        EXPECT_TRUE(harness.events_of(WindowEventType::MOUSE_BUTTON_PRESSED).empty());
    }

    TEST(CocoaWindowTest, MouseButtonCarriesModifierBits)
    {
        REQUIRE_DISPLAY();
        WindowHarness harness;

        CocoaInput::mouse_button(harness.native(), 0, true, Modifier::Shift | Modifier::Option);
        harness.poll();

        const auto pressed = harness.events_of(WindowEventType::MOUSE_BUTTON_PRESSED);
        ASSERT_EQ(pressed.size(), 1U);
        EXPECT_EQ(data_of<WindowEvent::MouseButtonData>(pressed.front()).mods, 0x0005);
    }

    struct ScrollCase {
        const char* name;
        int32_t delta_x;
        int32_t delta_y;
        double expected_x;
        double expected_y;
    };

    class CocoaScrollTest : public ::testing::TestWithParam<ScrollCase> { };

    TEST_P(CocoaScrollTest, PreciseDeltasAreScaledAndHorizontalIsFlipped)
    {
        REQUIRE_DISPLAY();
        const auto& param = GetParam();
        WindowHarness harness;

        CocoaInput::scroll_precise(harness.native(), param.delta_x, param.delta_y);
        harness.poll();

        const auto scrolled = harness.events_of(WindowEventType::MOUSE_SCROLLED);
        ASSERT_EQ(scrolled.size(), 1U);
        const auto& scroll = data_of<WindowEvent::ScrollData>(scrolled.front());
        EXPECT_NEAR(scroll.x_offset, param.expected_x, 1e-6);
        EXPECT_NEAR(scroll.y_offset, param.expected_y, 1e-6);
    }

    INSTANTIATE_TEST_SUITE_P(
        CocoaWindow,
        CocoaScrollTest,
        ::testing::Values(
            ScrollCase { "VerticalUp", 0, 20, 0.0, 2.0 },
            ScrollCase { "VerticalDown", 0, -20, 0.0, -2.0 },
            ScrollCase { "AppKitPositiveHorizontalIsNegativeX", 10, 0, -1.0, 0.0 },
            ScrollCase { "AppKitNegativeHorizontalIsPositiveX", -10, 0, 1.0, 0.0 },
            ScrollCase { "Diagonal", -30, 50, 3.0, 5.0 }),
        [](const ::testing::TestParamInfo<ScrollCase>& info) { return std::string(info.param.name); });

    TEST(CocoaWindowTest, DiscreteLineScrollKeepsDirectionWithoutPreciseScaling)
    {
        REQUIRE_DISPLAY();
        WindowHarness harness;

        CocoaInput::scroll_lines(harness.native(), 0, 1);
        CocoaInput::scroll_lines(harness.native(), 0, -1);
        harness.poll();

        const auto scrolled = harness.events_of(WindowEventType::MOUSE_SCROLLED);
        ASSERT_EQ(scrolled.size(), 2U);
        EXPECT_GT(data_of<WindowEvent::ScrollData>(scrolled[0]).y_offset, 0.0);
        EXPECT_LT(data_of<WindowEvent::ScrollData>(scrolled[1]).y_offset, 0.0);
    }

    TEST(CocoaWindowTest, ZeroScrollEmitsNothing)
    {
        REQUIRE_DISPLAY();
        WindowHarness harness;

        CocoaInput::scroll_precise(harness.native(), 0, 0);
        harness.poll();

        EXPECT_TRUE(harness.events_of(WindowEventType::MOUSE_SCROLLED).empty());
    }

    TEST(CocoaWindowTest, MouseEnterAndExitUpdateHoverState)
    {
        REQUIRE_DISPLAY();
        WindowHarness harness;
        EXPECT_FALSE(harness.window().get_state().is_hovered);

        CocoaInput::mouse_entered(harness.native());
        harness.poll();
        EXPECT_TRUE(harness.window().get_state().is_hovered);
        EXPECT_EQ(harness.events_of(WindowEventType::MOUSE_ENTERED).size(), 1U);

        CocoaInput::mouse_exited(harness.native());
        harness.poll();
        EXPECT_FALSE(harness.window().get_state().is_hovered);
        EXPECT_EQ(harness.events_of(WindowEventType::MOUSE_EXITED).size(), 1U);
    }

    TEST(CocoaWindowTest, EventQueueDropsOverflowWithoutBlockingAndRecovers)
    {
        REQUIRE_DISPLAY();
        WindowHarness harness;

        for (int i = 0; i < 400; ++i)
            CocoaInput::mouse_move(harness.native(), static_cast<double>(i), 0.0);
        harness.poll();

        const auto delivered = harness.events_of(WindowEventType::MOUSE_MOTION).size();
        EXPECT_GT(delivered, 0U);
        EXPECT_LE(delivered, 256U);

        harness.clear_events();
        CocoaInput::mouse_move(harness.native(), 7.0, 0.0);
        harness.poll();
        EXPECT_EQ(harness.events_of(WindowEventType::MOUSE_MOTION).size(), 1U);
    }

    TEST(CocoaWindowTest, PollPreservesEventOrder)
    {
        REQUIRE_DISPLAY();
        WindowHarness harness;
        const auto code = key_code(IO::Keys::A);

        CocoaInput::key_down(harness.native(), code);
        CocoaInput::mouse_move(harness.native(), 1.0, 2.0);
        CocoaInput::key_up(harness.native(), code);
        harness.poll();

        const auto& events = harness.events();
        ASSERT_EQ(events.size(), 3U);
        EXPECT_EQ(events[0].type, WindowEventType::KEY_PRESSED);
        EXPECT_EQ(events[1].type, WindowEventType::MOUSE_MOTION);
        EXPECT_EQ(events[2].type, WindowEventType::KEY_RELEASED);
    }

    TEST(CocoaKeyMappingTest, LettersAndNavigationKeysRoundTripThroughTable)
    {
        for (auto raw = static_cast<int16_t>(IO::Keys::A); raw <= static_cast<int16_t>(IO::Keys::Z); ++raw) {
            const auto key = static_cast<IO::Keys>(raw);
            const int code = Core::to_cocoa_key(key);
            ASSERT_GE(code, 0);
            EXPECT_TRUE(Core::is_valid_cocoa_key(static_cast<uint16_t>(code)));
            EXPECT_EQ(Core::from_cocoa_key(static_cast<uint16_t>(code)), key);
        }

        for (const auto key : { IO::Keys::Space, IO::Keys::Enter, IO::Keys::Left, IO::Keys::Right }) {
            const int code = Core::to_cocoa_key(key);
            ASSERT_GE(code, 0);
            EXPECT_EQ(Core::from_cocoa_key(static_cast<uint16_t>(code)), key);
        }
    }

    TEST(CocoaKeyMappingTest, ModifierKeysMapToDistinctCodes)
    {
        const std::array modifiers {
            IO::Keys::LShift, IO::Keys::RShift, IO::Keys::LCtrl, IO::Keys::RCtrl,
            IO::Keys::LAlt, IO::Keys::RAlt, IO::Keys::LSuper, IO::Keys::RSuper,
            IO::Keys::CapsLock
        };

        std::vector<int> codes;
        for (const auto key : modifiers) {
            const int code = Core::to_cocoa_key(key);
            ASSERT_GE(code, 0);
            EXPECT_EQ(Core::from_cocoa_key(static_cast<uint16_t>(code)), key);
            codes.push_back(code);
        }

        std::ranges::sort(codes);
        EXPECT_EQ(std::ranges::adjacent_find(codes), codes.end());
    }

    TEST(CocoaKeyMappingTest, UnmappedKeysReportUnknownAndNegativeOne)
    {
        EXPECT_EQ(Core::to_cocoa_key(IO::Keys::Unknown), -1);
        EXPECT_EQ(Core::from_cocoa_key(0xFFFF), IO::Keys::Unknown);
        EXPECT_EQ(Core::from_cocoa_key(128), IO::Keys::Unknown);
        EXPECT_FALSE(Core::is_valid_cocoa_key(0xFFFF));
    }

}
}

#undef REQUIRE_DISPLAY

#endif
