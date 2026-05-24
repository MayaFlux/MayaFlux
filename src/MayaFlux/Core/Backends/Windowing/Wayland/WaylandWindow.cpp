#include "WaylandWindow.hpp"

#ifdef MAYAFLUX_PLATFORM_LINUX

#include "KeyMapping.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

#include <linux/input-event-codes.h>
#include <sys/mman.h>
#include <sys/timerfd.h>
#include <unistd.h>

namespace MayaFlux::Core {

// ============================================================================
// Construction / destruction
// ============================================================================

WaylandWindow::WaylandWindow(const WindowCreateInfo& create_info,
    const GraphicsSurfaceInfo&,
    GlobalGraphicsConfig::GraphicsApi)
    : m_display(wl_display_connect(nullptr))
    , m_create_info(create_info)
{

    if (!m_display) {
        MF_ERROR(Journal::Component::Core, Journal::Context::WindowingSubsystem,
            "wl_display_connect failed");
        return;
    }

    m_xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

    m_registry = wl_display_get_registry(m_display);
    wl_registry_add_listener(m_registry, &s_registry_listener, this);
    wl_display_roundtrip(m_display);

    if (!m_compositor || !m_xdg_wm_base) {
        MF_ERROR(Journal::Component::Core, Journal::Context::WindowingSubsystem,
            "Required Wayland globals not found (compositor={}, xdg_wm_base={})",
            static_cast<void*>(m_compositor), static_cast<void*>(m_xdg_wm_base));
        return;
    }

    m_surface = wl_compositor_create_surface(m_compositor);
    m_xdg_surface = xdg_wm_base_get_xdg_surface(m_xdg_wm_base, m_surface);
    xdg_surface_add_listener(m_xdg_surface, &s_xdg_surface_listener, this);

    m_xdg_toplevel = xdg_surface_get_toplevel(m_xdg_surface);
    xdg_toplevel_add_listener(m_xdg_toplevel, &s_toplevel_listener, this);
    xdg_toplevel_set_title(m_xdg_toplevel, create_info.title.c_str());
    xdg_toplevel_set_app_id(m_xdg_toplevel, "mayaflux");

    if (m_decoration_manager) {
        m_decoration = zxdg_decoration_manager_v1_get_toplevel_decoration(
            m_decoration_manager, m_xdg_toplevel);
        zxdg_toplevel_decoration_v1_set_mode(m_decoration,
            ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
    }

    wl_surface_commit(m_surface);
    wl_display_roundtrip(m_display);

    m_state.current_width = static_cast<int>(create_info.width);
    m_state.current_height = static_cast<int>(create_info.height);
}

WaylandWindow::~WaylandWindow()
{
    destroy();
    if (m_display) {
        wl_display_disconnect(m_display);
        m_display = nullptr;
    }
}

// ============================================================================
// Window interface
// ============================================================================

void WaylandWindow::show()
{
    if (m_surface)
        wl_surface_commit(m_surface);
}

void WaylandWindow::hide()
{
    if (m_surface) {
        wl_surface_attach(m_surface, nullptr, 0, 0);
        wl_surface_commit(m_surface);
    }
}

void WaylandWindow::destroy()
{
    if (m_repeat_fd >= 0) {
        close(m_repeat_fd);
        m_repeat_fd = -1;
    }

    if (m_keyboard) {
        wl_keyboard_destroy(m_keyboard);
        m_keyboard = nullptr;
    }
    if (m_pointer) {
        wl_pointer_destroy(m_pointer);
        m_pointer = nullptr;
    }

    if (m_xkb_state) {
        xkb_state_unref(m_xkb_state);
        m_xkb_state = nullptr;
    }
    if (m_xkb_keymap) {
        xkb_keymap_unref(m_xkb_keymap);
        m_xkb_keymap = nullptr;
    }
    if (m_xkb_context) {
        xkb_context_unref(m_xkb_context);
        m_xkb_context = nullptr;
    }

    if (m_decoration) {
        zxdg_toplevel_decoration_v1_destroy(m_decoration);
        m_decoration = nullptr;
    }
    if (m_decoration_manager) {
        zxdg_decoration_manager_v1_destroy(m_decoration_manager);
        m_decoration_manager = nullptr;
    }
    if (m_xdg_toplevel) {
        xdg_toplevel_destroy(m_xdg_toplevel);
        m_xdg_toplevel = nullptr;
    }
    if (m_xdg_surface) {
        xdg_surface_destroy(m_xdg_surface);
        m_xdg_surface = nullptr;
    }
    if (m_surface) {
        wl_surface_destroy(m_surface);
        m_surface = nullptr;
    }
    if (m_xdg_wm_base) {
        xdg_wm_base_destroy(m_xdg_wm_base);
        m_xdg_wm_base = nullptr;
    }
    if (m_seat) {
        wl_seat_destroy(m_seat);
        m_seat = nullptr;
    }
    if (m_compositor) {
        wl_compositor_destroy(m_compositor);
        m_compositor = nullptr;
    }
    if (m_registry) {
        wl_registry_destroy(m_registry);
        m_registry = nullptr;
    }
}

bool WaylandWindow::should_close() const
{
    return m_should_close.load();
}

void WaylandWindow::poll()
{
    wl_display_dispatch_pending(m_display);
    wl_display_flush(m_display);

    if (m_repeat_fd >= 0 && !m_held_keys.empty()) {
        uint64_t expirations = 0;
        if (read(m_repeat_fd, &expirations, sizeof(expirations)) == sizeof(expirations)
            && expirations > 0) {
            for (const auto& [k, kd] : m_held_keys) {
                WindowEvent rev;
                rev.type = WindowEventType::KEY_REPEAT;
                rev.data = kd;
                emit(rev);
            }
        }
    }

    if (m_pending_configure.exchange(false)) {
        xdg_surface_set_window_geometry(m_xdg_surface, 0, 0,
            static_cast<int32_t>(m_state.current_width),
            static_cast<int32_t>(m_state.current_height));
        wl_surface_commit(m_surface);
        wl_display_flush(m_display);
    }
}

void WaylandWindow::set_event_callback(WindowEventCallback callback)
{
    m_event_callback = std::move(callback);
}

void* WaylandWindow::get_native_handle() const
{
    return m_surface;
}

void* WaylandWindow::get_native_display() const
{
    return m_display;
}

void WaylandWindow::set_title(const std::string& title)
{
    m_create_info.title = title;
    if (m_xdg_toplevel)
        xdg_toplevel_set_title(m_xdg_toplevel, title.c_str());
}

void WaylandWindow::set_size(uint32_t w, uint32_t h)
{
    m_create_info.width = w;
    m_create_info.height = h;
    if (m_xdg_toplevel && m_surface) {
        xdg_toplevel_set_min_size(m_xdg_toplevel,
            static_cast<int32_t>(w), static_cast<int32_t>(h));
        xdg_toplevel_set_max_size(m_xdg_toplevel,
            static_cast<int32_t>(w), static_cast<int32_t>(h));
        wl_surface_commit(m_surface);
    }
}

void WaylandWindow::set_position(uint32_t, uint32_t)
{
    // Wayland does not allow client-side window positioning.
}

void WaylandWindow::set_color(const std::array<float, 4>& color)
{
    m_create_info.clear_color = color;
}

// ============================================================================
// Render tracking
// ============================================================================

void WaylandWindow::register_rendering_buffer(std::shared_ptr<Buffers::VKBuffer> buf)
{
    std::lock_guard lock(m_render_tracking_mutex);
    m_rendering_buffers.push_back(buf);
}

void WaylandWindow::unregister_rendering_buffer(std::shared_ptr<Buffers::VKBuffer> buf)
{
    std::lock_guard lock(m_render_tracking_mutex);
    std::erase_if(m_rendering_buffers, [&buf](const auto& wp) {
        auto sp = wp.lock();
        return !sp || sp == buf;
    });
}

void WaylandWindow::track_frame_command(uint64_t id)
{
    m_frame_commands.push_back(id);
}

const std::vector<uint64_t>& WaylandWindow::get_frame_commands() const
{
    return m_frame_commands;
}

void WaylandWindow::clear_frame_commands()
{
    m_frame_commands.clear();
}

std::vector<std::shared_ptr<Buffers::VKBuffer>> WaylandWindow::get_rendering_buffers() const
{
    std::lock_guard lock(m_render_tracking_mutex);
    std::vector<std::shared_ptr<Buffers::VKBuffer>> out;
    out.reserve(m_rendering_buffers.size());
    for (auto& wp : m_rendering_buffers) {
        if (auto sp = wp.lock())
            out.push_back(sp);
    }
    return out;
}

// ============================================================================
// Internal helpers
// ============================================================================

void WaylandWindow::emit(const WindowEvent& ev)
{
    m_event_source.signal(ev);
    if (m_event_callback)
        m_event_callback(ev);
}

// ============================================================================
// Registry
// ============================================================================

void WaylandWindow::on_registry_global(void* data, wl_registry* reg,
    uint32_t name, const char* iface, uint32_t version)
{
    auto* self = static_cast<WaylandWindow*>(data);

    if (std::string_view(iface) == wl_compositor_interface.name) {
        self->m_compositor = static_cast<wl_compositor*>(
            wl_registry_bind(reg, name, &wl_compositor_interface,
                std::min(version, 4U)));
    } else if (std::string_view(iface) == xdg_wm_base_interface.name) {
        self->m_xdg_wm_base = static_cast<xdg_wm_base*>(
            wl_registry_bind(reg, name, &xdg_wm_base_interface,
                std::min(version, 6U)));
        xdg_wm_base_add_listener(self->m_xdg_wm_base, &s_wm_base_listener, self);
    } else if (std::string_view(iface) == wl_seat_interface.name) {
        self->m_seat = static_cast<wl_seat*>(
            wl_registry_bind(reg, name, &wl_seat_interface,
                std::min(version, 7U)));
        wl_seat_add_listener(self->m_seat, &s_seat_listener, self);
    } else if (std::string_view(iface) == zxdg_decoration_manager_v1_interface.name) {
        self->m_decoration_manager = static_cast<zxdg_decoration_manager_v1*>(
            wl_registry_bind(reg, name, &zxdg_decoration_manager_v1_interface, 1U));
    }
}

void WaylandWindow::on_registry_global_remove(void*, wl_registry*, uint32_t) { }

// ============================================================================
// xdg_wm_base
// ============================================================================

void WaylandWindow::on_wm_base_ping(void*, xdg_wm_base* base, uint32_t serial)
{
    xdg_wm_base_pong(base, serial);
}

// ============================================================================
// xdg_surface
// ============================================================================

void WaylandWindow::on_xdg_surface_configure(void* data, xdg_surface* surf, uint32_t serial)
{
    auto* self = static_cast<WaylandWindow*>(data);
    xdg_surface_ack_configure(surf, serial);
    self->m_pending_configure.store(true);
}

// ============================================================================
// xdg_toplevel
// ============================================================================

void WaylandWindow::on_toplevel_configure(void* data, xdg_toplevel*,
    int32_t w, int32_t h, wl_array*)
{
    auto* self = static_cast<WaylandWindow*>(data);
    if (w <= 0 || h <= 0)
        return;

    if (static_cast<uint32_t>(w) == self->m_state.current_width && static_cast<uint32_t>(h) == self->m_state.current_height)
        return;

    self->m_state.current_width = static_cast<uint32_t>(w);
    self->m_state.current_height = static_cast<uint32_t>(h);
    self->m_create_info.width = static_cast<uint32_t>(w);
    self->m_create_info.height = static_cast<uint32_t>(h);
    WindowEvent ev;
    ev.type = WindowEventType::WINDOW_RESIZED;
    ev.data = WindowEvent::ResizeData {
        .width = static_cast<uint32_t>(w),
        .height = static_cast<uint32_t>(h)
    };
    self->emit(ev);
}

void WaylandWindow::on_toplevel_close(void* data, xdg_toplevel*)
{
    auto* self = static_cast<WaylandWindow*>(data);
    self->m_should_close.store(true);
    WindowEvent ev;
    ev.type = WindowEventType::WINDOW_CLOSED;
    self->emit(ev);
}

void WaylandWindow::on_toplevel_configure_bounds(void*, xdg_toplevel*,
    int32_t, int32_t) { }

void WaylandWindow::on_toplevel_wm_capabilities(void*, xdg_toplevel*,
    wl_array*) { }

// ============================================================================
// wl_seat
// ============================================================================

void WaylandWindow::on_seat_capabilities(void* data, wl_seat* seat, uint32_t caps)
{
    auto* self = static_cast<WaylandWindow*>(data);

    const bool has_kb = caps & WL_SEAT_CAPABILITY_KEYBOARD;
    const bool has_ptr = caps & WL_SEAT_CAPABILITY_POINTER;

    if (has_kb && !self->m_keyboard) {
        self->m_keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(self->m_keyboard, &s_keyboard_listener, self);
    } else if (!has_kb && self->m_keyboard) {
        wl_keyboard_destroy(self->m_keyboard);
        self->m_keyboard = nullptr;
    }

    if (has_ptr && !self->m_pointer) {
        self->m_pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(self->m_pointer, &s_pointer_listener, self);
    } else if (!has_ptr && self->m_pointer) {
        wl_pointer_destroy(self->m_pointer);
        self->m_pointer = nullptr;
    }
}

void WaylandWindow::on_seat_name(void*, wl_seat*, const char*) { }

// ============================================================================
// wl_keyboard
// ============================================================================

void WaylandWindow::on_keyboard_keymap(void* data, wl_keyboard*,
    uint32_t fmt, int fd, uint32_t size)
{
    auto* self = static_cast<WaylandWindow*>(data);

    if (fmt != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        close(fd);
        return;
    }

    void* buf = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (buf == MAP_FAILED)
        return;

    if (self->m_xkb_state) {
        xkb_state_unref(self->m_xkb_state);
        self->m_xkb_state = nullptr;
    }
    if (self->m_xkb_keymap) {
        xkb_keymap_unref(self->m_xkb_keymap);
        self->m_xkb_keymap = nullptr;
    }

    self->m_xkb_keymap = xkb_keymap_new_from_string(self->m_xkb_context,
        static_cast<const char*>(buf),
        XKB_KEYMAP_FORMAT_TEXT_V1,
        XKB_KEYMAP_COMPILE_NO_FLAGS);

    munmap(buf, size);

    if (self->m_xkb_keymap)
        self->m_xkb_state = xkb_state_new(self->m_xkb_keymap);
}

void WaylandWindow::on_keyboard_enter(void* data, wl_keyboard*,
    uint32_t, wl_surface*, wl_array*)
{
    auto* self = static_cast<WaylandWindow*>(data);
    self->m_state.is_focused = true;
    WindowEvent ev;
    ev.type = WindowEventType::WINDOW_FOCUS_GAINED;
    self->emit(ev);
}

void WaylandWindow::on_keyboard_leave(void* data, wl_keyboard*,
    uint32_t, wl_surface*)
{
    auto* self = static_cast<WaylandWindow*>(data);
    self->m_state.is_focused = false;
    WindowEvent ev;
    ev.type = WindowEventType::WINDOW_FOCUS_LOST;
    self->emit(ev);
}

void WaylandWindow::on_keyboard_key(void* data, wl_keyboard*,
    uint32_t, uint32_t, uint32_t key, uint32_t state)
{
    auto* self = static_cast<WaylandWindow*>(data);
    if (!self->m_xkb_state)
        return;

    IO::Keys mf_key = from_evdev_scancode(key);
    if (mf_key == IO::Keys::Unknown) {
        xkb_keycode_t xkb_key = key + 8;
        xkb_keysym_t sym = xkb_state_key_get_one_sym(self->m_xkb_state, xkb_key);
        mf_key = from_xkb_keysym(sym);
    }

    const WindowEvent::KeyData kd {
        .key = static_cast<int16_t>(mf_key),
        .scancode = static_cast<int32_t>(key),
        .mods = 0
    };

    WindowEvent ev;
    ev.data = kd;

    if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        ev.type = WindowEventType::KEY_PRESSED;
        self->emit(ev);

        self->m_held_keys[kd.key] = kd;

        if (self->m_repeat_fd < 0)
            self->m_repeat_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);

        const long delay_ns = 90'000'000L;
        const long repeat_ns = self->m_repeat_rate > 0
            ? 1'000'000'000L / self->m_repeat_rate
            : 16'666'667L;

        itimerspec ts {};
        ts.it_value.tv_sec = 0;
        ts.it_value.tv_nsec = delay_ns;
        ts.it_interval.tv_sec = 0;
        ts.it_interval.tv_nsec = repeat_ns;
        timerfd_settime(self->m_repeat_fd, 0, &ts, nullptr);

    } else {
        self->m_held_keys.erase(kd.key);

        if (self->m_held_keys.empty() && self->m_repeat_fd >= 0) {
            itimerspec ts {};
            timerfd_settime(self->m_repeat_fd, 0, &ts, nullptr);
        }

        ev.type = WindowEventType::KEY_RELEASED;
        self->emit(ev);
    }
}

void WaylandWindow::on_keyboard_modifiers(void* data, wl_keyboard*,
    uint32_t, uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group)
{
    auto* self = static_cast<WaylandWindow*>(data);
    if (self->m_xkb_state) {
        xkb_state_update_mask(self->m_xkb_state,
            depressed, latched, locked, 0, 0, group);
    }
}

void WaylandWindow::on_keyboard_repeat_info(void* data, wl_keyboard*,
    int32_t rate, int32_t delay)
{
    auto* self = static_cast<WaylandWindow*>(data);
    (void)rate;
    (void)delay;
    self->m_repeat_rate = 60;
    self->m_repeat_delay = 90;
}

// ============================================================================
// wl_pointer
// ============================================================================

void WaylandWindow::on_pointer_enter(void* data, wl_pointer*,
    uint32_t, wl_surface*, wl_fixed_t sx, wl_fixed_t sy)
{
    auto* self = static_cast<WaylandWindow*>(data);
    self->m_pointer_x = wl_fixed_to_double(sx);
    self->m_pointer_y = wl_fixed_to_double(sy);
    WindowEvent ev;
    ev.type = WindowEventType::MOUSE_ENTERED;
    self->emit(ev);
}

void WaylandWindow::on_pointer_leave(void* data, wl_pointer*, uint32_t, wl_surface*)
{
    auto* self = static_cast<WaylandWindow*>(data);
    WindowEvent ev;
    ev.type = WindowEventType::MOUSE_EXITED;
    self->emit(ev);
}

void WaylandWindow::on_pointer_motion(void* data, wl_pointer*,
    uint32_t, wl_fixed_t sx, wl_fixed_t sy)
{
    auto* self = static_cast<WaylandWindow*>(data);
    self->m_pointer_x = wl_fixed_to_double(sx);
    self->m_pointer_y = wl_fixed_to_double(sy);
    WindowEvent ev;
    ev.type = WindowEventType::MOUSE_MOTION;
    ev.data = WindowEvent::MousePosData {
        .x = self->m_pointer_x,
        .y = self->m_pointer_y
    };
    self->emit(ev);
}

void WaylandWindow::on_pointer_button(void* data, wl_pointer*,
    uint32_t, uint32_t, uint32_t button, uint32_t state)
{
    auto* self = static_cast<WaylandWindow*>(data);

    IO::MouseButtons btn = IO::MouseButtons::Unknown;
    switch (button) {
    case BTN_LEFT:
        btn = IO::MouseButtons::Left;
        break;
    case BTN_RIGHT:
        btn = IO::MouseButtons::Right;
        break;
    case BTN_MIDDLE:
        btn = IO::MouseButtons::Middle;
        break;
    case BTN_SIDE:
        btn = IO::MouseButtons::Button4;
        break;
    case BTN_EXTRA:
        btn = IO::MouseButtons::Button5;
        break;
    default:
        break;
    }

    WindowEvent ev;
    ev.type = (state == WL_POINTER_BUTTON_STATE_PRESSED)
        ? WindowEventType::MOUSE_BUTTON_PRESSED
        : WindowEventType::MOUSE_BUTTON_RELEASED;
    ev.data = WindowEvent::MouseButtonData {
        .button = static_cast<int8_t>(btn),
        .mods = 0
    };
    self->emit(ev);
}

void WaylandWindow::on_pointer_axis(void* data, wl_pointer*,
    uint32_t, uint32_t axis, wl_fixed_t value)
{
    auto* self = static_cast<WaylandWindow*>(data);
    const double v = wl_fixed_to_double(value) / 10.0;
    WindowEvent ev;
    ev.type = WindowEventType::MOUSE_SCROLLED;
    ev.data = WindowEvent::ScrollData {
        .x_offset = (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL) ? v : 0.0,
        .y_offset = (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) ? -v : 0.0
    };
    self->emit(ev);
}

void WaylandWindow::on_pointer_frame(void*, wl_pointer*) { }
void WaylandWindow::on_pointer_axis_source(void*, wl_pointer*, uint32_t) { }
void WaylandWindow::on_pointer_axis_stop(void*, wl_pointer*, uint32_t, uint32_t) { }
void WaylandWindow::on_pointer_axis_discrete(void*, wl_pointer*, uint32_t, int32_t) { }

} // namespace MayaFlux::Core

#endif // MAYAFLUX_PLATFORM_LINUX
