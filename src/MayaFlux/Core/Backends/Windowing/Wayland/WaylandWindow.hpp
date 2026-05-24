#pragma once

#ifdef MAYAFLUX_PLATFORM_LINUX

#include "MayaFlux/Core/Backends/Windowing/Window.hpp"
#include "MayaFlux/Vruta/WindowEventSource.hpp"

#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

#include "wayland-xdg-decoration-unstable-v1-client-protocol.h"
#include "wayland-xdg-shell-client-protocol.h"

namespace MayaFlux::Core {

/**
 * @class WaylandWindow
 * @brief Native Wayland window backend, no GLFW dependency.
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
        const GraphicsSurfaceInfo& surface_info,
        GlobalGraphicsConfig::GraphicsApi api);

    ~WaylandWindow() override;

    WaylandWindow(const WaylandWindow&) = delete;
    WaylandWindow& operator=(const WaylandWindow&) = delete;
    WaylandWindow(WaylandWindow&&) = delete;
    WaylandWindow& operator=(WaylandWindow&&) = delete;

    void show() override;
    void hide() override;
    void destroy() override;
    void poll() override;

    [[nodiscard]] bool should_close() const override;

    [[nodiscard]] const WindowState& get_state() const override { return m_state; }
    [[nodiscard]] const WindowCreateInfo& get_create_info() const override { return m_create_info; }

    void set_input_config(const InputConfig& config) override { m_input_config = config; }
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

    double m_pointer_x {};
    double m_pointer_y {};

    // -------------------------------------------------------------------------
    // Window state
    // -------------------------------------------------------------------------
    WindowCreateInfo m_create_info;
    WindowState m_state;
    InputConfig m_input_config;
    WindowEventCallback m_event_callback;
    Vruta::WindowEventSource m_event_source;

    std::atomic<bool> m_should_close { false };
    std::atomic<bool> m_graphics_registered { false };
    std::atomic<bool> m_pending_configure { false };
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
