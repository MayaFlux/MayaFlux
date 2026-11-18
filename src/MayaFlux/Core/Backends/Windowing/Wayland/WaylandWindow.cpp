#include "WaylandWindow.hpp"

#ifdef MAYAFLUX_PLATFORM_LINUX

#include "KeyMapping.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <poll.h>
#include <sys/mman.h>
#include <sys/timerfd.h>
#include <unistd.h>

namespace MayaFlux::Core {

namespace {

    ssize_t write_without_sigpipe(int fd, const char* data, size_t size)
    {
        sigset_t blocked;
        sigset_t previous;
        sigemptyset(&blocked);
        sigaddset(&blocked, SIGPIPE);
        pthread_sigmask(SIG_BLOCK, &blocked, &previous);

        const ssize_t written = ::write(fd, data, size);
        const int saved_errno = errno;

        if (written < 0 && saved_errno == EPIPE) {
            const timespec no_wait {};
            sigtimedwait(&blocked, nullptr, &no_wait);
        }

        pthread_sigmask(SIG_SETMASK, &previous, nullptr);
        errno = saved_errno;
        return written;
    }

    const std::string* preferred_text_mime(const std::vector<std::string>& offered)
    {
        constexpr std::array<std::string_view, 3> k_preferred {
            "text/plain;charset=utf-8", "text/plain", "UTF8_STRING"
        };
        for (const auto wanted : k_preferred) {
            const auto found = std::ranges::find(offered, wanted);
            if (found != offered.end())
                return &*found;
        }
        return nullptr;
    }

}

// ============================================================================
// Construction / destruction
// ============================================================================

WaylandWindow::WaylandWindow(const WindowCreateInfo& create_info,
    const GlobalGraphicsConfig& graphics_config)
    : m_display(wl_display_connect(nullptr))
    , m_create_info(create_info)
    , m_key_repeat_config(graphics_config.key_repeat_config)
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
    release_clipboard();

    if (m_repeat_fd >= 0) {
        close(m_repeat_fd);
        m_repeat_fd = -1;
    }

    if (m_text_repeat_fd >= 0) {
        close(m_text_repeat_fd);
        m_text_repeat_fd = -1;
    }

    if (m_keyboard) {
        wl_keyboard_destroy(m_keyboard);
        m_keyboard = nullptr;
    }
    unlock_cursor();
    if (m_cursor_shape_device) {
        wp_cursor_shape_device_v1_destroy(m_cursor_shape_device);
        m_cursor_shape_device = nullptr;
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
    if (m_cursor_shape_manager) {
        wp_cursor_shape_manager_v1_destroy(m_cursor_shape_manager);
        m_cursor_shape_manager = nullptr;
    }
    if (m_relative_pointer_manager) {
        zwp_relative_pointer_manager_v1_destroy(m_relative_pointer_manager);
        m_relative_pointer_manager = nullptr;
    }
    if (m_pointer_constraints) {
        zwp_pointer_constraints_v1_destroy(m_pointer_constraints);
        m_pointer_constraints = nullptr;
    }
    if (m_data_device_manager) {
        wl_data_device_manager_destroy(m_data_device_manager);
        m_data_device_manager = nullptr;
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

void WaylandWindow::request_close()
{
    m_event_source.defer([this]() {
        if (!m_should_close.exchange(true)) {
            WindowEvent ev;
            ev.type = WindowEventType::WINDOW_CLOSED;
            emit(ev);
        }
    });
}

void WaylandWindow::poll()
{
    if (!m_display)
        return;

    if (m_cursor_mode_dirty.exchange(false))
        apply_cursor_mode();

    while (wl_display_prepare_read(m_display) != 0) {
        if (wl_display_dispatch_pending(m_display) < 0) {
            handle_display_error();
            return;
        }
    }

    if (wl_display_flush(m_display) < 0 && errno != EAGAIN) {
        wl_display_cancel_read(m_display);
        handle_display_error();
        return;
    }

    pollfd pfd {};
    pfd.fd = wl_display_get_fd(m_display);
    pfd.events = POLLIN;

    if (::poll(&pfd, 1, 0) > 0 && (pfd.revents & (POLLIN | POLLHUP | POLLERR))) {
        if (wl_display_read_events(m_display) < 0) {
            handle_display_error();
            return;
        }
    } else {
        wl_display_cancel_read(m_display);
    }

    if (wl_display_dispatch_pending(m_display) < 0) {
        handle_display_error();
        return;
    }

    advance_clipboard_writes();
    advance_clipboard_reads();

    if (m_repeat_fd >= 0 && !m_held_keys.empty()) {
        uint64_t expirations = 0;
        if (read(m_repeat_fd, &expirations, sizeof(expirations)) == sizeof(expirations)
            && expirations > 0) {
            const IO::Modifiers mods = current_mods();
            for (const auto& [k, kd] : m_held_keys) {
                WindowEvent::KeyData repeated = kd;
                repeated.mods = mods;
                WindowEvent rev;
                rev.type = WindowEventType::KEY_REPEAT;
                rev.data = repeated;
                emit(rev);
            }
        }
    }

    if (m_text_repeat_fd >= 0 && m_active_text_key) {
        uint64_t expirations = 0;
        if (read(m_text_repeat_fd, &expirations, sizeof(expirations)) == sizeof(expirations)
            && expirations > 0) {
            WindowEvent tev;
            tev.type = WindowEventType::TEXT_INPUT;
            tev.data = WindowEvent::TextData { .codepoint = m_active_text_key->second };
            emit(tev);
        }
    }

    if (m_pending_configure.exchange(false) && m_xdg_surface && m_surface) {
        xdg_surface_set_window_geometry(m_xdg_surface, 0, 0,
            static_cast<int32_t>(m_state.current_width),
            static_cast<int32_t>(m_state.current_height));
        wl_surface_commit(m_surface);
        wl_display_flush(m_display);
    }
}

void WaylandWindow::handle_display_error()
{
    const int err = wl_display_get_error(m_display);

    if (err == EPROTO) {
        const wl_interface* iface = nullptr;
        uint32_t id = 0;
        const uint32_t code = wl_display_get_protocol_error(m_display, &iface, &id);
        MF_ERROR(Journal::Component::Core, Journal::Context::WindowingSubsystem,
            "Wayland protocol error: interface={}, id={}, code={}",
            iface ? iface->name : "unknown", id, code);
    } else {
        MF_ERROR(Journal::Component::Core, Journal::Context::WindowingSubsystem,
            "Wayland display error: {}", err);
    }

    if (!m_should_close.exchange(true)) {
        WindowEvent ev;
        ev.type = WindowEventType::WINDOW_CLOSED;
        emit(ev);
    }
}

void WaylandWindow::set_input_config(const InputConfig& config)
{
    m_input_config = config;
    m_cursor_mode_dirty.store(true);
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

IO::Keys WaylandWindow::resolve_key(uint32_t evdev_key) const
{
    IO::Keys key = from_evdev_scancode(evdev_key);
    if (key == IO::Keys::Unknown && m_xkb_state) {
        const xkb_keysym_t sym = xkb_state_key_get_one_sym(m_xkb_state, evdev_key + 8);
        key = from_xkb_keysym(sym);
    }
    return key;
}

IO::Modifiers WaylandWindow::current_mods() const
{
    IO::Modifiers mods = m_modifier_keys.held();

    if (m_xkb_state) {
        const auto locked = [this](xkb_mod_index_t index) {
            return index != XKB_MOD_INVALID
                && xkb_state_mod_index_is_active(m_xkb_state, index, XKB_STATE_MODS_LOCKED) > 0;
        };
        if (locked(m_mod_caps))
            mods = mods | IO::Modifiers::CapsLock;
        if (locked(m_mod_num))
            mods = mods | IO::Modifiers::NumLock;
    }

    return mods;
}

void WaylandWindow::release_held_keys()
{
    const auto held = std::move(m_held_keys);
    m_held_keys.clear();
    m_active_text_key = std::nullopt;
    m_modifier_keys.clear();

    itimerspec stop {};
    if (m_repeat_fd >= 0)
        timerfd_settime(m_repeat_fd, 0, &stop, nullptr);
    if (m_text_repeat_fd >= 0)
        timerfd_settime(m_text_repeat_fd, 0, &stop, nullptr);

    const IO::Modifiers mods = current_mods();
    for (const auto& [k, kd] : held) {
        WindowEvent::KeyData released = kd;
        released.mods = mods;
        WindowEvent ev;
        ev.type = WindowEventType::KEY_RELEASED;
        ev.data = released;
        emit(ev);
    }
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
        self->create_data_device();
    } else if (std::string_view(iface) == zxdg_decoration_manager_v1_interface.name) {
        self->m_decoration_manager = static_cast<zxdg_decoration_manager_v1*>(
            wl_registry_bind(reg, name, &zxdg_decoration_manager_v1_interface, 1U));
    } else if (std::string_view(iface) == wl_data_device_manager_interface.name) {
        self->m_data_device_manager = static_cast<wl_data_device_manager*>(
            wl_registry_bind(reg, name, &wl_data_device_manager_interface,
                std::min(version, 3U)));
        self->create_data_device();
    } else if (std::string_view(iface) == zwp_pointer_constraints_v1_interface.name) {
        self->m_pointer_constraints = static_cast<zwp_pointer_constraints_v1*>(
            wl_registry_bind(reg, name, &zwp_pointer_constraints_v1_interface, 1U));
    } else if (std::string_view(iface) == zwp_relative_pointer_manager_v1_interface.name) {
        self->m_relative_pointer_manager = static_cast<zwp_relative_pointer_manager_v1*>(
            wl_registry_bind(reg, name, &zwp_relative_pointer_manager_v1_interface, 1U));
    } else if (std::string_view(iface) == wp_cursor_shape_manager_v1_interface.name) {
        self->m_cursor_shape_manager = static_cast<wp_cursor_shape_manager_v1*>(
            wl_registry_bind(reg, name, &wp_cursor_shape_manager_v1_interface, 1U));
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
    int32_t w, int32_t h, wl_array* states)
{
    auto* self = static_cast<WaylandWindow*>(data);

    bool now_resizing = false;
    const auto* state_begin = static_cast<const uint32_t*>(states->data);
    const auto* state_end = reinterpret_cast<const uint32_t*>(
        static_cast<const uint8_t*>(states->data) + states->size);
    for (const auto* state = state_begin; state != state_end; ++state) {
        if (*state == XDG_TOPLEVEL_STATE_RESIZING) {
            now_resizing = true;
            break;
        }
    }
    const bool gesture_ended = self->m_resizing && !now_resizing;
    self->m_resizing = now_resizing;

    if (w <= 0 || h <= 0)
        return;

    const bool size_changed = static_cast<uint32_t>(w) != self->m_state.current_width
        || static_cast<uint32_t>(h) != self->m_state.current_height;

    if (!size_changed && !gesture_ended)
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

    if (self->m_event_callback)
        self->m_event_callback(ev);

    if (!now_resizing)
        self->m_event_source.signal(ev);
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
        self->m_cursor_mode_dirty.store(true);
    } else if (!has_ptr && self->m_pointer) {
        self->unlock_cursor();
        if (self->m_cursor_shape_device) {
            wp_cursor_shape_device_v1_destroy(self->m_cursor_shape_device);
            self->m_cursor_shape_device = nullptr;
        }
        wl_pointer_destroy(self->m_pointer);
        self->m_pointer = nullptr;
        self->m_pointer_inside = false;
        self->m_cursor_hidden = false;
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

    if (self->m_xkb_keymap) {
        self->m_xkb_state = xkb_state_new(self->m_xkb_keymap);
        self->m_mod_caps = xkb_keymap_mod_get_index(self->m_xkb_keymap, XKB_MOD_NAME_CAPS);
        self->m_mod_num = xkb_keymap_mod_get_index(self->m_xkb_keymap, XKB_MOD_NAME_NUM);
    } else {
        self->m_mod_caps = XKB_MOD_INVALID;
        self->m_mod_num = XKB_MOD_INVALID;
    }
}

void WaylandWindow::on_keyboard_enter(void* data, wl_keyboard*,
    uint32_t, wl_surface*, wl_array* keys)
{
    auto* self = static_cast<WaylandWindow*>(data);

    self->m_modifier_keys.clear();
    const auto* pressed_begin = static_cast<const uint32_t*>(keys->data);
    const auto* pressed_end = reinterpret_cast<const uint32_t*>(
        static_cast<const uint8_t*>(keys->data) + keys->size);
    for (const auto* code = pressed_begin; code != pressed_end; ++code)
        self->m_modifier_keys.set(self->resolve_key(*code), true);

    self->m_state.is_focused = true;
    self->apply_cursor_mode();
    WindowEvent ev;
    ev.type = WindowEventType::WINDOW_FOCUS_GAINED;
    self->emit(ev);
}

void WaylandWindow::on_keyboard_leave(void* data, wl_keyboard*,
    uint32_t, wl_surface*)
{
    auto* self = static_cast<WaylandWindow*>(data);
    self->release_held_keys();
    self->drop_clipboard_offer();
    self->m_state.is_focused = false;
    self->apply_cursor_mode();
    WindowEvent ev;
    ev.type = WindowEventType::WINDOW_FOCUS_LOST;
    self->emit(ev);
}

void WaylandWindow::on_keyboard_key(void* data, wl_keyboard*,
    uint32_t serial, uint32_t, uint32_t key, uint32_t state)
{
    auto* self = static_cast<WaylandWindow*>(data);
    self->m_input_serial = serial;
    if (!self->m_xkb_state)
        return;

    const xkb_keycode_t xkb_key = key + 8;

    const IO::Keys mf_key = self->resolve_key(key);
    self->m_modifier_keys.set(mf_key, state == WL_KEYBOARD_KEY_STATE_PRESSED);

    const WindowEvent::KeyData kd {
        .key = static_cast<int16_t>(mf_key),
        .scancode = static_cast<int32_t>(key),
        .mods = self->current_mods()
    };

    WindowEvent ev;
    ev.data = kd;

    if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        ev.type = WindowEventType::KEY_PRESSED;
        self->emit(ev);

        if (const uint32_t codepoint = xkb_state_key_get_utf32(self->m_xkb_state, xkb_key);
            is_text_input_codepoint(codepoint)) {
            WindowEvent tev;
            tev.type = WindowEventType::TEXT_INPUT;
            tev.data = WindowEvent::TextData { .codepoint = codepoint };
            self->emit(tev);

            self->m_active_text_key = { kd.key, codepoint };

            if (self->m_text_repeat_fd < 0)
                self->m_text_repeat_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);

            const long t_delay_ns = static_cast<long>(self->m_key_repeat_config.text_initial_delay_ms) * 1'000'000L;
            const long t_repeat_ns = self->m_key_repeat_config.text_interval_ms > 0
                ? static_cast<long>(self->m_key_repeat_config.text_interval_ms) * 1'000'000L
                : 33'333'333L;

            itimerspec ts {};
            ts.it_value.tv_sec = 0;
            ts.it_value.tv_nsec = t_delay_ns;
            ts.it_interval.tv_sec = 0;
            ts.it_interval.tv_nsec = t_repeat_ns;
            timerfd_settime(self->m_text_repeat_fd, 0, &ts, nullptr);
        }

        self->m_held_keys[kd.key] = kd;

        if (self->m_repeat_fd < 0)
            self->m_repeat_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);

        const long delay_ns = static_cast<long>(self->m_key_repeat_config.initial_delay_ms) * 1'000'000L;
        const long repeat_ns = self->m_key_repeat_config.interval_ms > 0
            ? static_cast<long>(self->m_key_repeat_config.interval_ms) * 1'000'000L
            : 16'666'667L;

        itimerspec ts {};
        ts.it_value.tv_sec = 0;
        ts.it_value.tv_nsec = delay_ns;
        ts.it_interval.tv_sec = 0;
        ts.it_interval.tv_nsec = repeat_ns;
        timerfd_settime(self->m_repeat_fd, 0, &ts, nullptr);

    } else {
        if (self->m_active_text_key && self->m_active_text_key->first == kd.key) {
            self->m_active_text_key = std::nullopt;
            if (self->m_text_repeat_fd >= 0) {
                itimerspec ts {};
                timerfd_settime(self->m_text_repeat_fd, 0, &ts, nullptr);
            }
        }

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
    if (!self->m_key_repeat_config.allow_compositor_override)
        return;

    if (rate > 0) {
        self->m_key_repeat_config.interval_ms = 1000U / static_cast<uint32_t>(rate);
        self->m_key_repeat_config.text_interval_ms = 1000U / static_cast<uint32_t>(rate);
    }
    if (delay > 0) {
        self->m_key_repeat_config.initial_delay_ms = static_cast<uint32_t>(delay);
        self->m_key_repeat_config.text_initial_delay_ms = static_cast<uint32_t>(delay);
    }
}

// ============================================================================
// Clipboard
// ============================================================================

void WaylandWindow::create_data_device()
{
    if (m_data_device || !m_data_device_manager || !m_seat)
        return;

    m_data_device = wl_data_device_manager_get_data_device(m_data_device_manager, m_seat);
    wl_data_device_add_listener(m_data_device, &s_data_device_listener, this);
}

void WaylandWindow::discard_offer(wl_data_offer* offer)
{
    if (!offer)
        return;

    m_offer_mime_types.erase(offer);
    wl_data_offer_destroy(offer);
}

void WaylandWindow::drop_clipboard_offer()
{
    discard_offer(m_clipboard_offer);
    m_clipboard_offer = nullptr;
}

void WaylandWindow::request_clipboard_text(ClipboardTextCallback callback)
{
    if (!callback)
        return;

    ClipboardRead read;
    read.callback = std::move(callback);

    if (m_clipboard_source) {
        read.data = m_clipboard_text;
        read.ready = true;
        read.ok = true;
    } else if (m_clipboard_offer && m_display) {
        const auto types = m_offer_mime_types.find(m_clipboard_offer);
        const std::string* mime = types != m_offer_mime_types.end()
            ? preferred_text_mime(types->second)
            : nullptr;

        int pipe_fds[2] {};
        if (mime && pipe2(pipe_fds, O_CLOEXEC) == 0) {
            wl_data_offer_receive(m_clipboard_offer, mime->c_str(), pipe_fds[1]);
            close(pipe_fds[1]);
            wl_display_flush(m_display);
            fcntl(pipe_fds[0], F_SETFL, O_NONBLOCK);
            read.fd = pipe_fds[0];
        } else {
            read.ready = true;
        }
    } else {
        read.ready = true;
    }

    m_clipboard_reads.push_back(std::move(read));
}

void WaylandWindow::set_clipboard_text(const std::string& text)
{
    if (!m_data_device || !m_data_device_manager || !m_display)
        return;

    wl_data_source* source = wl_data_device_manager_create_data_source(m_data_device_manager);
    wl_data_source_add_listener(source, &s_data_source_listener, this);
    wl_data_source_offer(source, "text/plain;charset=utf-8");
    wl_data_source_offer(source, "text/plain");
    wl_data_source_offer(source, "UTF8_STRING");

    m_clipboard_text = text;
    wl_data_device_set_selection(m_data_device, source, m_input_serial);

    if (m_clipboard_source)
        wl_data_source_destroy(m_clipboard_source);
    m_clipboard_source = source;

    wl_display_flush(m_display);
}

void WaylandWindow::advance_clipboard_writes()
{
    for (auto& write : m_clipboard_writes) {
        while (write.fd >= 0 && write.offset < write.data.size()) {
            const ssize_t written = write_without_sigpipe(write.fd,
                write.data.data() + write.offset, write.data.size() - write.offset);
            if (written > 0) {
                write.offset += static_cast<size_t>(written);
            } else if (written < 0 && errno == EINTR) {
                continue;
            } else if (written < 0 && errno == EAGAIN) {
                break;
            } else {
                write.offset = write.data.size();
            }
        }

        if (write.offset >= write.data.size() && write.fd >= 0) {
            close(write.fd);
            write.fd = -1;
        }
    }

    std::erase_if(m_clipboard_writes, [](const ClipboardWrite& write) { return write.fd < 0; });
}

void WaylandWindow::advance_clipboard_reads()
{
    for (auto& read : m_clipboard_reads) {
        while (read.fd >= 0 && !read.ready) {
            std::array<char, 4096> buffer {};
            const ssize_t count = ::read(read.fd, buffer.data(), buffer.size());
            if (count > 0) {
                read.data.append(buffer.data(), static_cast<size_t>(count));
            } else if (count < 0 && errno == EINTR) {
                continue;
            } else if (count < 0 && errno == EAGAIN) {
                break;
            } else {
                read.ok = count == 0;
                read.ready = true;
                close(read.fd);
                read.fd = -1;
            }
        }
    }

    std::vector<ClipboardRead> finished;
    for (auto& read : m_clipboard_reads) {
        if (read.ready)
            finished.push_back(std::move(read));
    }
    std::erase_if(m_clipboard_reads, [](const ClipboardRead& read) { return read.ready || !read.callback; });

    for (auto& read : finished) {
        read.callback(read.ok ? std::optional<std::string>(std::move(read.data)) : std::nullopt);
    }
}

void WaylandWindow::release_clipboard()
{
    for (auto& write : m_clipboard_writes) {
        if (write.fd >= 0)
            close(write.fd);
    }
    m_clipboard_writes.clear();

    auto reads = std::move(m_clipboard_reads);
    m_clipboard_reads.clear();
    for (auto& read : reads) {
        if (read.fd >= 0) {
            close(read.fd);
            read.fd = -1;
        }
        read.callback(read.ready && read.ok ? std::optional<std::string>(std::move(read.data)) : std::nullopt);
    }

    if (m_clipboard_source) {
        wl_data_source_destroy(m_clipboard_source);
        m_clipboard_source = nullptr;
    }
    m_clipboard_text.clear();

    m_clipboard_offer = nullptr;
    for (const auto& entry : m_offer_mime_types)
        wl_data_offer_destroy(entry.first);
    m_offer_mime_types.clear();

    if (m_data_device) {
        if (wl_data_device_get_version(m_data_device) >= WL_DATA_DEVICE_RELEASE_SINCE_VERSION)
            wl_data_device_release(m_data_device);
        else
            wl_data_device_destroy(m_data_device);
        m_data_device = nullptr;
    }
}

void WaylandWindow::on_data_offer(void* data, wl_data_device*, wl_data_offer* offer)
{
    auto* self = static_cast<WaylandWindow*>(data);
    self->m_offer_mime_types[offer];
    wl_data_offer_add_listener(offer, &s_data_offer_listener, self);
}

void WaylandWindow::on_data_enter(void* data, wl_data_device*, uint32_t,
    wl_surface*, wl_fixed_t, wl_fixed_t, wl_data_offer* offer)
{
    static_cast<WaylandWindow*>(data)->discard_offer(offer);
}

void WaylandWindow::on_data_leave(void*, wl_data_device*) { }
void WaylandWindow::on_data_motion(void*, wl_data_device*, uint32_t, wl_fixed_t, wl_fixed_t) { }
void WaylandWindow::on_data_drop(void*, wl_data_device*) { }

void WaylandWindow::on_data_selection(void* data, wl_data_device*, wl_data_offer* offer)
{
    auto* self = static_cast<WaylandWindow*>(data);
    if (self->m_clipboard_offer != offer)
        self->drop_clipboard_offer();
    self->m_clipboard_offer = offer;
}

void WaylandWindow::on_offer_mime_type(void* data, wl_data_offer* offer, const char* mime_type)
{
    auto* self = static_cast<WaylandWindow*>(data);
    const auto types = self->m_offer_mime_types.find(offer);
    if (types != self->m_offer_mime_types.end())
        types->second.emplace_back(mime_type);
}

void WaylandWindow::on_offer_source_actions(void*, wl_data_offer*, uint32_t) { }
void WaylandWindow::on_offer_action(void*, wl_data_offer*, uint32_t) { }

void WaylandWindow::on_source_target(void*, wl_data_source*, const char*) { }

void WaylandWindow::on_source_send(void* data, wl_data_source* source, const char*, int32_t fd)
{
    auto* self = static_cast<WaylandWindow*>(data);
    if (source != self->m_clipboard_source) {
        close(fd);
        return;
    }

    fcntl(fd, F_SETFL, O_NONBLOCK);
    self->m_clipboard_writes.push_back({ .fd = fd, .data = self->m_clipboard_text, .offset = 0 });
    self->advance_clipboard_writes();
}

void WaylandWindow::on_source_cancelled(void* data, wl_data_source* source)
{
    auto* self = static_cast<WaylandWindow*>(data);
    wl_data_source_destroy(source);
    if (source == self->m_clipboard_source) {
        self->m_clipboard_source = nullptr;
        self->m_clipboard_text.clear();
    }
}

void WaylandWindow::on_source_dnd_drop_performed(void*, wl_data_source*) { }
void WaylandWindow::on_source_dnd_finished(void*, wl_data_source*) { }
void WaylandWindow::on_source_action(void*, wl_data_source*, uint32_t) { }

// ============================================================================
// Cursor mode
// ============================================================================

bool WaylandWindow::cursor_should_hide() const
{
    switch (m_input_config.cursor_mode) {
    case CursorMode::HIDDEN:
        return true;
    case CursorMode::DISABLED:
    case CursorMode::CAPTURED:
        return m_pointer_constraints && m_relative_pointer_manager;
    case CursorMode::NORMAL:
        break;
    }
    return false;
}

void WaylandWindow::apply_cursor_mode()
{
    const CursorMode mode = m_input_config.cursor_mode;
    const bool wants_lock = mode == CursorMode::DISABLED || mode == CursorMode::CAPTURED;

    if (wants_lock && !(m_pointer_constraints && m_relative_pointer_manager)) {
        MF_WARN_ONCE(Journal::Component::Core, Journal::Context::WindowingSubsystem,
            "Compositor offers no pointer constraints or relative pointer; cursor_mode DISABLED and CAPTURED behave as NORMAL");
    }

    if (wants_lock && m_state.is_focused)
        lock_cursor();
    else
        unlock_cursor();

    update_cursor_visibility();
}

void WaylandWindow::lock_cursor()
{
    if (m_locked_pointer || !m_pointer || !m_surface
        || !m_pointer_constraints || !m_relative_pointer_manager) {
        return;
    }

    m_locked_pointer = zwp_pointer_constraints_v1_lock_pointer(
        m_pointer_constraints, m_surface, m_pointer, nullptr,
        ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT);
    zwp_locked_pointer_v1_add_listener(m_locked_pointer, &s_locked_pointer_listener, this);

    m_relative_pointer = zwp_relative_pointer_manager_v1_get_relative_pointer(
        m_relative_pointer_manager, m_pointer);
    zwp_relative_pointer_v1_add_listener(m_relative_pointer, &s_relative_pointer_listener, this);
}

void WaylandWindow::unlock_cursor()
{
    if (m_relative_pointer) {
        zwp_relative_pointer_v1_destroy(m_relative_pointer);
        m_relative_pointer = nullptr;
    }
    if (m_locked_pointer) {
        zwp_locked_pointer_v1_destroy(m_locked_pointer);
        m_locked_pointer = nullptr;
    }
    m_cursor_locked = false;
}

void WaylandWindow::update_cursor_visibility()
{
    if (!m_pointer || !m_pointer_inside)
        return;

    const bool hide = cursor_should_hide();

    if (hide && !m_cursor_hidden) {
        wl_pointer_set_cursor(m_pointer, m_pointer_enter_serial, nullptr, 0, 0);
        m_cursor_hidden = true;
        return;
    }

    if (hide || !m_cursor_hidden)
        return;

    if (!m_cursor_shape_device && m_cursor_shape_manager) {
        m_cursor_shape_device = wp_cursor_shape_manager_v1_get_pointer(
            m_cursor_shape_manager, m_pointer);
    }

    if (!m_cursor_shape_device) {
        MF_WARN_ONCE(Journal::Component::Core, Journal::Context::WindowingSubsystem,
            "Compositor offers no cursor shape protocol; a hidden cursor returns when the pointer next enters the window");
        return;
    }

    wp_cursor_shape_device_v1_set_shape(m_cursor_shape_device, m_pointer_enter_serial,
        WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT);
    m_cursor_hidden = false;
}

void WaylandWindow::on_pointer_locked(void* data, zwp_locked_pointer_v1*)
{
    auto* self = static_cast<WaylandWindow*>(data);
    self->m_cursor_locked = true;
    self->m_virtual_x = static_cast<double>(self->m_state.current_width) / 2.0;
    self->m_virtual_y = static_cast<double>(self->m_state.current_height) / 2.0;
}

void WaylandWindow::on_pointer_unlocked(void* data, zwp_locked_pointer_v1*)
{
    static_cast<WaylandWindow*>(data)->m_cursor_locked = false;
}

void WaylandWindow::on_relative_motion(void* data, zwp_relative_pointer_v1*,
    uint32_t, uint32_t, wl_fixed_t dx, wl_fixed_t dy,
    wl_fixed_t dx_unaccel, wl_fixed_t dy_unaccel)
{
    auto* self = static_cast<WaylandWindow*>(data);
    if (!self->m_cursor_locked)
        return;

    const bool unaccelerated = self->m_input_config.cursor_mode == CursorMode::CAPTURED
        || self->m_input_config.raw_mouse_motion;
    self->m_virtual_x += wl_fixed_to_double(unaccelerated ? dx_unaccel : dx);
    self->m_virtual_y += wl_fixed_to_double(unaccelerated ? dy_unaccel : dy);

    WindowEvent ev;
    ev.type = WindowEventType::MOUSE_MOTION;
    ev.data = WindowEvent::MousePosData {
        .x = self->m_virtual_x,
        .y = self->m_virtual_y
    };
    self->emit(ev);
}

// ============================================================================
// wl_pointer
// ============================================================================

void WaylandWindow::on_pointer_enter(void* data, wl_pointer*,
    uint32_t serial, wl_surface*, wl_fixed_t sx, wl_fixed_t sy)
{
    auto* self = static_cast<WaylandWindow*>(data);
    self->m_pointer_enter_serial = serial;
    self->m_pointer_inside = true;
    self->m_cursor_hidden = false;
    self->update_cursor_visibility();
    self->m_pointer_x = wl_fixed_to_double(sx);
    self->m_pointer_y = wl_fixed_to_double(sy);
    WindowEvent ev;
    ev.type = WindowEventType::MOUSE_ENTERED;
    self->emit(ev);
}

void WaylandWindow::on_pointer_leave(void* data, wl_pointer*, uint32_t, wl_surface*)
{
    auto* self = static_cast<WaylandWindow*>(data);
    self->m_pointer_inside = false;
    self->m_cursor_hidden = false;
    WindowEvent ev;
    ev.type = WindowEventType::MOUSE_EXITED;
    self->emit(ev);
}

void WaylandWindow::on_pointer_motion(void* data, wl_pointer*,
    uint32_t, wl_fixed_t sx, wl_fixed_t sy)
{
    auto* self = static_cast<WaylandWindow*>(data);
    if (self->m_cursor_locked)
        return;
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
    uint32_t serial, uint32_t, uint32_t button, uint32_t state)
{
    auto* self = static_cast<WaylandWindow*>(data);
    self->m_input_serial = serial;

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
        .mods = self->current_mods()
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
