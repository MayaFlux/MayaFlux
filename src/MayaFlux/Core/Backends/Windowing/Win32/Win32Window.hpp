#pragma once

#ifdef MAYAFLUX_PLATFORM_WINDOWS

#include "MayaFlux/Core/Backends/Windowing/ModifierKeyState.hpp"
#include "MayaFlux/Core/Backends/Windowing/Window.hpp"
#include "MayaFlux/Vruta/WindowEventSource.hpp"

#include "MayaFlux/Transitive/Memory/RingBuffer.hpp"

#include <windows.h>

#ifdef ERROR
#undef ERROR
#endif

namespace MayaFlux::Core {

/**
 * @struct RawMouseState
 * @brief Previous absolute position, for devices that report absolute raw motion.
 */
struct RawMouseState {
    bool absolute_seen {};
    LONG last_x {};
    LONG last_y {};
};

/**
 * @brief Motion in pixels carried by one raw mouse record, if it moved.
 * @param mouse The record from WM_INPUT.
 * @param state Previous absolute position, updated for absolute devices.
 * @param primary Size of the primary monitor in pixels.
 * @param virtual_screen Size of the whole virtual desktop in pixels.
 *
 * Relative records give lLastX and lLastY as they are. Absolute records, which
 * some virtual devices send, are normalised to 0 to 65535 over the primary
 * monitor or, with MOUSE_VIRTUAL_DESKTOP, the virtual desktop; they are
 * differenced against the previous record, and the first one reports no motion.
 */
MAYAFLUX_API std::optional<std::pair<double, double>> raw_mouse_delta(
    const RAWMOUSE& mouse, RawMouseState& state, SIZE primary, SIZE virtual_screen) noexcept;

/**
 * @class Win32Window
 * @brief Native Win32 window backend.
 *
 * The Win32 message loop runs on a dedicated UI thread spawned in the
 * constructor. WndProc enqueues WindowEvents under m_event_mutex; poll()
 * drains the queue on the graphics thread and signals m_event_source.
 * WindowManager::process() calls poll() once per frame.
 *
 * Everything that belongs to the window's thread happens there: modifier state,
 * cursor hiding, the pointer lock, raw input registration and the clipboard.
 * Other threads hand work over by message and read results back through poll().
 */
class MAYAFLUX_API Win32Window : public Window {
public:
    Win32Window(const WindowCreateInfo& create_info,
        const GlobalGraphicsConfig& graphics_config);

    ~Win32Window() override;

    Win32Window(const Win32Window&) = delete;
    Win32Window& operator=(const Win32Window&) = delete;
    Win32Window(Win32Window&&) = delete;
    Win32Window& operator=(Win32Window&&) = delete;

    void show() override;
    void hide() override;
    void destroy() override;
    void poll() override;

    void request_clipboard_text(ClipboardTextCallback callback) override;
    void set_clipboard_text(const std::string& text) override;

    [[nodiscard]] bool should_close() const override;
    void request_close() override;

    [[nodiscard]] const WindowState& get_state() const override { return m_state; }
    [[nodiscard]] const WindowCreateInfo& get_create_info() const override { return m_create_info; }

    void set_input_config(const InputConfig& config) override;
    [[nodiscard]] const InputConfig& get_input_config() const override { return m_input_config; }

    void set_event_callback(WindowEventCallback callback) override;

    [[nodiscard]] void* get_native_handle() const override { return m_hwnd; }
    [[nodiscard]] void* get_native_display() const override { return nullptr; }

    void set_title(const std::string& title) override;
    void set_size(uint32_t width, uint32_t height) override;
    void set_position(uint32_t x, uint32_t y) override;
    void set_color(const std::array<float, 4>& color) override;

    static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    Vruta::WindowEventSource& get_event_source() override { return m_event_source; }
    [[nodiscard]] const Vruta::WindowEventSource& get_event_source() const override { return m_event_source; }

    [[nodiscard]] bool is_graphics_registered() const override { return m_graphics_registered.load(); }
    void set_graphics_registered(bool registered) override { m_graphics_registered.store(registered); }

    void register_rendering_buffer(std::shared_ptr<Buffers::VKBuffer> buffer) override;
    void unregister_rendering_buffer(std::shared_ptr<Buffers::VKBuffer> buffer) override;
    void track_frame_command(uint64_t cmd_id) override;
    [[nodiscard]] const std::vector<uint64_t>& get_frame_commands() const override;
    void clear_frame_commands() override;
    [[nodiscard]] std::vector<std::shared_ptr<Buffers::VKBuffer>> get_rendering_buffers() const override;

    [[nodiscard]] bool is_capture_enabled() const override { return m_capture_enabled.load(std::memory_order_acquire); }
    void set_capture_enabled(bool enabled) override { m_capture_enabled.store(enabled, std::memory_order_release); }

private:
    static constexpr UINT k_message_input_config = WM_APP + 1;
    static constexpr UINT k_message_clipboard = WM_APP + 2;
    static constexpr UINT_PTR k_timer_clipboard = 1;
    static constexpr int k_clipboard_attempts = 20;
    static constexpr UINT k_clipboard_retry_ms = 15;

    HWND m_hwnd { nullptr };
    HWND m_ui_hwnd { nullptr };
    HINSTANCE m_hinstance { nullptr };
    std::atomic<bool> m_ui_alive { false };

    std::thread m_ui_thread;
    std::atomic<bool> m_hwnd_ready { false };
    std::atomic<bool> m_should_close { false };
    std::atomic<bool> m_capture_enabled { false };
    std::atomic<bool> m_in_size_move { false };

    static constexpr size_t EVENT_QUEUE_CAPACITY = 256;

    Memory::LockFreeQueue<WindowEvent, EVENT_QUEUE_CAPACITY> m_event_queue;

    KeyRepeatConfig m_key_repeat_config;
    WindowCreateInfo m_create_info;
    WindowState m_state;
    InputConfig m_input_config;
    WindowEventCallback m_event_callback;

    std::atomic<bool> m_graphics_registered { false };
    Vruta::WindowEventSource m_event_source;
    std::atomic<bool> m_keys_dirty { false };
    std::unordered_map<int16_t, WindowEvent::KeyData> m_held_keys_ui;
    std::unordered_map<int16_t, WindowEvent::KeyData> m_held_keys;
    ULONGLONG m_repeat_next_tick { 0 };
    uint16_t m_pending_high_surrogate { 0 };

    ModifierKeyState m_modifier_keys_ui;
    std::atomic<IO::Modifiers> m_mods { IO::Modifiers::None };

    std::mutex m_config_mutex;
    InputConfig m_posted_config;
    std::atomic<bool> m_config_wake_posted { false };
    InputConfig m_ui_config;

    bool m_cursor_locked_ui {};
    bool m_lock_uses_raw_ui {};
    bool m_cursor_hidden_ui {};
    bool m_raw_registered_ui {};
    double m_virtual_x {};
    double m_virtual_y {};
    POINT m_center_client_ui {};
    POINT m_center_screen_ui {};
    RawMouseState m_raw_state_ui;
    std::optional<RAWINPUTDEVICE> m_saved_raw_mouse_ui;

    struct ClipboardWriteJob {
        std::string text;
        int attempts {};
    };

    struct ClipboardReadJob {
        ClipboardTextCallback callback;
        int attempts {};
    };

    struct ClipboardDelivery {
        ClipboardTextCallback callback;
        std::optional<std::string> text;
    };

    std::mutex m_clipboard_mutex;
    std::vector<ClipboardWriteJob> m_clipboard_writes;
    std::vector<ClipboardReadJob> m_clipboard_reads;
    std::vector<ClipboardDelivery> m_clipboard_done;
    std::atomic<bool> m_clipboard_wake_posted { false };

    std::vector<std::weak_ptr<Buffers::VKBuffer>> m_rendering_buffers;
    std::vector<uint64_t> m_frame_commands;
    mutable std::mutex m_render_tracking_mutex;

    void ui_thread_main();
    void push_event(WindowEvent ev);

    /** @brief UI thread: IO::Modifiers bits for the held modifier keys and the lock state, also published for poll(). */
    IO::Modifiers update_mods();

    /** @brief UI thread: seed the held modifier keys from the keyboard state when focus arrives. */
    void seed_modifier_keys();

    /** @brief UI thread: release every held key and modifier, as when focus is lost. */
    void release_held_keys();

    /** @brief UI thread: bring the pointer lock, raw input and cursor visibility in line with m_ui_config. */
    void apply_cursor_mode();

    /** @brief UI thread: confine the cursor to the client area; false when the system refuses. */
    bool update_clip();
    void lock_cursor();
    void unlock_cursor();
    void register_raw_mouse();
    void unregister_raw_mouse();
    void update_cursor_visibility();

    /** @brief UI thread: make the system ask the window for its cursor again, so a change shows without a mouse move. */
    void refresh_cursor();

    /** @brief UI thread: motion while locked without raw input, from the distance to the client centre. */
    void handle_locked_motion(int x, int y);

    /** @brief UI thread: motion while locked with raw input. */
    void handle_raw_input(HRAWINPUT handle);

    /** @brief UI thread: report the virtual position as MOUSE_MOTION. */
    void emit_virtual_motion();

    /** @brief UI thread: release the lock and the raw input registration. */
    void release_cursor_state();

    /** @brief Any thread: ask the UI thread to apply the posted input config. */
    void wake_input_config();

    /** @brief Any thread: ask the UI thread to run the clipboard queue, or fail it when there is none. */
    void wake_clipboard();

    /** @brief Any thread: finish every queued clipboard job as if the clipboard were unavailable. */
    void fail_pending_clipboard();

    /**
     * @brief UI thread: run queued clipboard jobs in order.
     * @param final When true a clipboard that is busy fails the job instead of retrying on a timer.
     */
    void service_clipboard(bool final);

    /** @brief Call every finished clipboard request that has not been delivered yet. */
    void deliver_clipboard();
};

} // namespace MayaFlux::Core

#endif // MAYAFLUX_PLATFORM_WINDOWS
