#pragma once

#ifdef MAYAFLUX_PLATFORM_LINUX

#include "MayaFlux/Core/Backends/Windowing/ModifierKeyState.hpp"
#include "MayaFlux/Core/Backends/Windowing/Window.hpp"
#include "MayaFlux/Vruta/WindowEventSource.hpp"

#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

#include "wayland-cursor-shape-v1-client-protocol.h"
#include "wayland-pointer-constraints-unstable-v1-client-protocol.h"
#include "wayland-relative-pointer-unstable-v1-client-protocol.h"
#include "wayland-xdg-decoration-unstable-v1-client-protocol.h"
#include "wayland-xdg-shell-client-protocol.h"

namespace MayaFlux::Core {

/**
 * @class WaylandWindow
 * @brief Native Wayland window backend.
 *
 * Connects directly to the Wayland compositor via wl_display. Keyboard
 * translation uses xkbcommon with the keymap supplied by the compositor
 * over the wl_keyboard::keymap event fd. poll() calls
 * wl_display_dispatch_pending + wl_display_flush synchronously on the
 * graphics thread; no dedicated UI thread is required because all Wayland
 * callbacks fire on the thread that calls wl_display_dispatch_pending.
 *
 * WindowManager::process() calls poll() on each WaylandWindow directly,
 * matching the Win32 pattern.
 */
class MAYAFLUX_API WaylandWindow : public Window {
public:
    WaylandWindow(const WindowCreateInfo& create_info,
        const GlobalGraphicsConfig& graphics_config);

    ~WaylandWindow() override;

    WaylandWindow(const WaylandWindow&) = delete;
    WaylandWindow& operator=(const WaylandWindow&) = delete;
    WaylandWindow(WaylandWindow&&) = delete;
    WaylandWindow& operator=(WaylandWindow&&) = delete;

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

    [[nodiscard]] void* get_native_handle() const override;
    [[nodiscard]] void* get_native_display() const override;

    void set_title(const std::string& title) override;
    void set_size(uint32_t width, uint32_t height) override;
    void set_position(uint32_t x, uint32_t y) override;
    void set_color(const std::array<float, 4>& color) override;

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
    // -------------------------------------------------------------------------
    // Wayland globals
    // -------------------------------------------------------------------------
    wl_display* m_display {};
    wl_registry* m_registry {};
    wl_compositor* m_compositor {};
    wl_seat* m_seat {};
    xdg_wm_base* m_xdg_wm_base {};
    zxdg_decoration_manager_v1* m_decoration_manager {};
    wl_data_device_manager* m_data_device_manager {};
    zwp_pointer_constraints_v1* m_pointer_constraints {};
    zwp_relative_pointer_manager_v1* m_relative_pointer_manager {};
    wp_cursor_shape_manager_v1* m_cursor_shape_manager {};

    // -------------------------------------------------------------------------
    // Per-window objects
    // -------------------------------------------------------------------------
    wl_surface* m_surface {};
    xdg_surface* m_xdg_surface {};
    xdg_toplevel* m_xdg_toplevel {};
    zxdg_toplevel_decoration_v1* m_decoration {};

    // -------------------------------------------------------------------------
    // Input
    // -------------------------------------------------------------------------
    wl_pointer* m_pointer {};
    wl_keyboard* m_keyboard {};

    xkb_context* m_xkb_context {};
    xkb_keymap* m_xkb_keymap {};
    xkb_state* m_xkb_state {};
    xkb_mod_index_t m_mod_caps { XKB_MOD_INVALID };
    xkb_mod_index_t m_mod_num { XKB_MOD_INVALID };
    ModifierKeyState m_modifier_keys;

    double m_pointer_x {};
    double m_pointer_y {};

    uint32_t m_input_serial {};
    uint32_t m_pointer_enter_serial {};

    zwp_locked_pointer_v1* m_locked_pointer {};
    zwp_relative_pointer_v1* m_relative_pointer {};
    wp_cursor_shape_device_v1* m_cursor_shape_device {};
    bool m_cursor_locked {};
    bool m_cursor_hidden {};
    bool m_pointer_inside {};
    double m_virtual_x {};
    double m_virtual_y {};
    std::atomic<bool> m_cursor_mode_dirty { false };

    struct ClipboardRead {
        ClipboardTextCallback callback;
        int fd { -1 };
        std::string data;
        bool ready {};
        bool ok {};
    };

    struct ClipboardWrite {
        int fd { -1 };
        std::string data;
        size_t offset {};
    };

    wl_data_device* m_data_device {};
    wl_data_source* m_clipboard_source {};
    wl_data_offer* m_clipboard_offer {};
    std::string m_clipboard_text;
    std::unordered_map<wl_data_offer*, std::vector<std::string>> m_offer_mime_types;
    std::vector<ClipboardRead> m_clipboard_reads;
    std::vector<ClipboardWrite> m_clipboard_writes;

    int m_repeat_fd { -1 };
    int m_text_repeat_fd { -1 };
    std::unordered_map<int16_t, WindowEvent::KeyData> m_held_keys;
    std::optional<std::pair<int16_t, uint32_t>> m_active_text_key;

    // -------------------------------------------------------------------------
    // Window state
    // -------------------------------------------------------------------------

    KeyRepeatConfig m_key_repeat_config;
    WindowCreateInfo m_create_info;
    WindowState m_state;
    InputConfig m_input_config;
    WindowEventCallback m_event_callback;
    Vruta::WindowEventSource m_event_source;

    std::atomic<bool> m_should_close { false };
    std::atomic<bool> m_graphics_registered { false };
    std::atomic<bool> m_pending_configure { false };
    bool m_resizing { false };
    std::atomic<bool> m_capture_enabled { false };
    uint32_t m_pending_width {};
    uint32_t m_pending_height {};

    // -------------------------------------------------------------------------
    // Render tracking
    // -------------------------------------------------------------------------
    mutable std::mutex m_render_tracking_mutex;
    std::vector<std::weak_ptr<Buffers::VKBuffer>> m_rendering_buffers;
    std::vector<uint64_t> m_frame_commands;

    // -------------------------------------------------------------------------
    // Internal helpers
    // -------------------------------------------------------------------------

    /** @brief Signal an event through both the event source and the callback. */
    void emit(const WindowEvent& ev);

    /** @brief Log the display error, mark the window closed, emit WINDOW_CLOSED once. */
    void handle_display_error();

    /** @brief Map an evdev key code to IO::Keys through the scancode table, then the active keymap. */
    [[nodiscard]] IO::Keys resolve_key(uint32_t evdev_key) const;

    /** @brief IO::Modifiers bits for the held modifier keys and the lock state. */
    [[nodiscard]] IO::Modifiers current_mods() const;

    /** @brief Release every held key and modifier, as when focus is lost. */
    void release_held_keys();

    /**
     * @brief Bring the pointer lock and cursor visibility in line with the input config.
     *
     * Locks while the window has keyboard focus in DISABLED or CAPTURED, hides the
     * cursor in every mode but NORMAL, and falls back to NORMAL behaviour when the
     * compositor lacks pointer constraints or relative pointer.
     */
    void apply_cursor_mode();

    /** @brief Create the data device once both the seat and the data device manager are bound. */
    void create_data_device();

    /** @brief Destroy one offer, forgetting its mime types. */
    void discard_offer(wl_data_offer* offer);

    /** @brief Drop the cached selection offer, which is only valid while the window has keyboard focus. */
    void drop_clipboard_offer();

    /** @brief Write pending copy data to the readers that asked for it, without blocking. */
    void advance_clipboard_writes();

    /** @brief Read pending paste data without blocking and deliver finished requests. */
    void advance_clipboard_reads();

    /** @brief Finish every request, close every pipe and release the data device and sources. */
    void release_clipboard();
    void lock_cursor();
    void unlock_cursor();
    void update_cursor_visibility();
    [[nodiscard]] bool cursor_should_hide() const;

    // -------------------------------------------------------------------------
    // Registry listener (static trampoline + instance handler)
    // -------------------------------------------------------------------------
    static void on_registry_global(void* data, wl_registry*, uint32_t name,
        const char* iface, uint32_t version);
    static void on_registry_global_remove(void* data, wl_registry*, uint32_t name);
    static constexpr wl_registry_listener s_registry_listener {
        .global = on_registry_global,
        .global_remove = on_registry_global_remove,
    };

    // -------------------------------------------------------------------------
    // xdg_wm_base listener
    // -------------------------------------------------------------------------
    static void on_wm_base_ping(void* data, xdg_wm_base*, uint32_t serial);
    static constexpr xdg_wm_base_listener s_wm_base_listener {
        .ping = on_wm_base_ping,
    };

    // -------------------------------------------------------------------------
    // xdg_surface listener
    // -------------------------------------------------------------------------
    static void on_xdg_surface_configure(void* data, xdg_surface*, uint32_t serial);
    static constexpr xdg_surface_listener s_xdg_surface_listener {
        .configure = on_xdg_surface_configure,
    };

    // -------------------------------------------------------------------------
    // xdg_toplevel listener
    // -------------------------------------------------------------------------
    static void on_toplevel_configure(void* data, xdg_toplevel*,
        int32_t w, int32_t h, wl_array* states);

    static void on_toplevel_close(void* data, xdg_toplevel*);

    static void on_toplevel_configure_bounds(void* data, xdg_toplevel*,
        int32_t w, int32_t h);

    static void on_toplevel_wm_capabilities(void* data, xdg_toplevel*,
        wl_array* capabilities);

    static constexpr xdg_toplevel_listener s_toplevel_listener {
        .configure = on_toplevel_configure,
        .close = on_toplevel_close,
        .configure_bounds = on_toplevel_configure_bounds,
        .wm_capabilities = on_toplevel_wm_capabilities,
    };

    // -------------------------------------------------------------------------
    // wl_seat listener
    // -------------------------------------------------------------------------
    static void on_seat_capabilities(void* data, wl_seat*, uint32_t caps);
    static void on_seat_name(void* data, wl_seat*, const char* name);
    static constexpr wl_seat_listener s_seat_listener {
        .capabilities = on_seat_capabilities,
        .name = on_seat_name,
    };

    // -------------------------------------------------------------------------
    // wl_keyboard listener
    // -------------------------------------------------------------------------
    static void on_keyboard_keymap(void* data, wl_keyboard*,
        uint32_t fmt, int fd, uint32_t size);
    static void on_keyboard_enter(void* data, wl_keyboard*,
        uint32_t serial, wl_surface*, wl_array* keys);
    static void on_keyboard_leave(void* data, wl_keyboard*,
        uint32_t serial, wl_surface*);
    static void on_keyboard_key(void* data, wl_keyboard*,
        uint32_t serial, uint32_t time, uint32_t key, uint32_t state);
    static void on_keyboard_modifiers(void* data, wl_keyboard*,
        uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched,
        uint32_t mods_locked, uint32_t group);
    static void on_keyboard_repeat_info(void* data, wl_keyboard*,
        int32_t rate, int32_t delay);
    static constexpr wl_keyboard_listener s_keyboard_listener {
        .keymap = on_keyboard_keymap,
        .enter = on_keyboard_enter,
        .leave = on_keyboard_leave,
        .key = on_keyboard_key,
        .modifiers = on_keyboard_modifiers,
        .repeat_info = on_keyboard_repeat_info,
    };

    // -------------------------------------------------------------------------
    // wl_data_device, wl_data_offer and wl_data_source listeners
    // -------------------------------------------------------------------------
    static void on_data_offer(void* data, wl_data_device*, wl_data_offer* offer);
    static void on_data_enter(void* data, wl_data_device*, uint32_t serial,
        wl_surface*, wl_fixed_t x, wl_fixed_t y, wl_data_offer* offer);
    static void on_data_leave(void* data, wl_data_device*);
    static void on_data_motion(void* data, wl_data_device*, uint32_t time,
        wl_fixed_t x, wl_fixed_t y);
    static void on_data_drop(void* data, wl_data_device*);
    static void on_data_selection(void* data, wl_data_device*, wl_data_offer* offer);
    static constexpr wl_data_device_listener s_data_device_listener {
        .data_offer = on_data_offer,
        .enter = on_data_enter,
        .leave = on_data_leave,
        .motion = on_data_motion,
        .drop = on_data_drop,
        .selection = on_data_selection,
    };

    static void on_offer_mime_type(void* data, wl_data_offer* offer, const char* mime_type);
    static void on_offer_source_actions(void* data, wl_data_offer*, uint32_t actions);
    static void on_offer_action(void* data, wl_data_offer*, uint32_t action);
    static constexpr wl_data_offer_listener s_data_offer_listener {
        .offer = on_offer_mime_type,
        .source_actions = on_offer_source_actions,
        .action = on_offer_action,
    };

    static void on_source_target(void* data, wl_data_source*, const char* mime_type);
    static void on_source_send(void* data, wl_data_source* source, const char* mime_type, int32_t fd);
    static void on_source_cancelled(void* data, wl_data_source* source);
    static void on_source_dnd_drop_performed(void* data, wl_data_source*);
    static void on_source_dnd_finished(void* data, wl_data_source*);
    static void on_source_action(void* data, wl_data_source*, uint32_t action);
    static constexpr wl_data_source_listener s_data_source_listener {
        .target = on_source_target,
        .send = on_source_send,
        .cancelled = on_source_cancelled,
        .dnd_drop_performed = on_source_dnd_drop_performed,
        .dnd_finished = on_source_dnd_finished,
        .action = on_source_action,
    };

    // -------------------------------------------------------------------------
    // zwp_locked_pointer_v1 and zwp_relative_pointer_v1 listeners
    // -------------------------------------------------------------------------
    static void on_pointer_locked(void* data, zwp_locked_pointer_v1*);
    static void on_pointer_unlocked(void* data, zwp_locked_pointer_v1*);
    static constexpr zwp_locked_pointer_v1_listener s_locked_pointer_listener {
        .locked = on_pointer_locked,
        .unlocked = on_pointer_unlocked,
    };

    static void on_relative_motion(void* data, zwp_relative_pointer_v1*,
        uint32_t utime_hi, uint32_t utime_lo,
        wl_fixed_t dx, wl_fixed_t dy, wl_fixed_t dx_unaccel, wl_fixed_t dy_unaccel);
    static constexpr zwp_relative_pointer_v1_listener s_relative_pointer_listener {
        .relative_motion = on_relative_motion,
    };

    // -------------------------------------------------------------------------
    // wl_pointer listener
    // -------------------------------------------------------------------------
    static void on_pointer_enter(void* data, wl_pointer*,
        uint32_t serial, wl_surface*, wl_fixed_t sx, wl_fixed_t sy);
    static void on_pointer_leave(void* data, wl_pointer*,
        uint32_t serial, wl_surface*);
    static void on_pointer_motion(void* data, wl_pointer*,
        uint32_t time, wl_fixed_t sx, wl_fixed_t sy);
    static void on_pointer_button(void* data, wl_pointer*,
        uint32_t serial, uint32_t time, uint32_t button, uint32_t state);
    static void on_pointer_axis(void* data, wl_pointer*,
        uint32_t time, uint32_t axis, wl_fixed_t value);
    static void on_pointer_frame(void* data, wl_pointer*);
    static void on_pointer_axis_source(void* data, wl_pointer*, uint32_t source);
    static void on_pointer_axis_stop(void* data, wl_pointer*, uint32_t time, uint32_t axis);
    static void on_pointer_axis_discrete(void* data, wl_pointer*, uint32_t axis, int32_t discrete);
    static constexpr wl_pointer_listener s_pointer_listener {
        .enter = on_pointer_enter,
        .leave = on_pointer_leave,
        .motion = on_pointer_motion,
        .button = on_pointer_button,
        .axis = on_pointer_axis,
        .frame = on_pointer_frame,
        .axis_source = on_pointer_axis_source,
        .axis_stop = on_pointer_axis_stop,
        .axis_discrete = on_pointer_axis_discrete,
    };
};

} // namespace MayaFlux::Core

#endif // MAYAFLUX_PLATFORM_LINUX
