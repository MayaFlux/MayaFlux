#ifdef MAYAFLUX_PLATFORM_WINDOWS

#include "Win32Window.hpp"
#include "KeyMapping.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#include <windowsx.h>

namespace MayaFlux::Core {

// ============================================================================
// Window class registration
// ============================================================================

static constexpr const wchar_t* k_class_name = L"MayaFluxWindow";

static void register_window_class(HINSTANCE hinstance)
{
    static std::once_flag s_flag;
    std::call_once(s_flag, [&]() {
        WNDCLASSEXW wc {};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = Win32Window::wnd_proc;
        wc.hInstance = hinstance;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = k_class_name;
        RegisterClassExW(&wc);
    });
}

// ============================================================================
// Constructor / destructor
// ============================================================================

Win32Window::Win32Window(const WindowCreateInfo& create_info,
    const GraphicsSurfaceInfo& /*surface_info*/,
    GlobalGraphicsConfig::GraphicsApi /*api*/)
    : m_create_info(create_info)
{
    m_hinstance = GetModuleHandleW(nullptr);

    m_ui_thread = std::thread([this]() { ui_thread_main(); });

    // Block until HWND is valid so callers always receive a live window.
    m_hwnd_ready.wait(false);
}

Win32Window::~Win32Window()
{
    destroy();
}

// ============================================================================
// UI thread
// ============================================================================

void Win32Window::ui_thread_main()
{
    register_window_class(m_hinstance);

    auto wtitle = std::wstring(m_create_info.title.begin(), m_create_info.title.end());

    m_hwnd = CreateWindowExW(
        0,
        k_class_name,
        wtitle.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        static_cast<int>(m_create_info.width),
        static_cast<int>(m_create_info.height),
        nullptr, nullptr, m_hinstance, nullptr);

    SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    m_hwnd_ready.store(true);
    m_hwnd_ready.notify_all();

    MSG msg {};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

// ============================================================================
// WndProc
// ============================================================================

LRESULT CALLBACK Win32Window::wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    auto* self = reinterpret_cast<Win32Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (!self)
        return DefWindowProcW(hwnd, msg, wp, lp);

    switch (msg) {
    case WM_CLOSE:
        self->m_should_close.store(true);
        {
            WindowEvent ev;
            ev.type = WindowEventType::WINDOW_CLOSED;
            ev.timestamp = 0.0;
            self->push_event(ev);
        }
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_SIZE: {
        auto w = static_cast<uint32_t>(LOWORD(lp));
        auto h = static_cast<uint32_t>(HIWORD(lp));
        self->m_state.current_width = static_cast<int>(w);
        self->m_state.current_height = static_cast<int>(h);
        WindowEvent ev;
        ev.type = WindowEventType::WINDOW_RESIZED;
        ev.timestamp = 0.0;
        ev.data = WindowEvent::ResizeData { .width = w, .height = h };
        self->push_event(ev);
        return 0;
    }

    case WM_SETFOCUS: {
        self->m_state.is_focused = true;
        WindowEvent ev;
        ev.type = WindowEventType::WINDOW_FOCUS_GAINED;
        ev.timestamp = 0.0;
        self->push_event(ev);
        return 0;
    }

    case WM_KILLFOCUS: {
        self->m_state.is_focused = false;
        WindowEvent ev;
        ev.type = WindowEventType::WINDOW_FOCUS_LOST;
        ev.timestamp = 0.0;
        self->push_event(ev);
        return 0;
    }

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        if (lp & (1 << 30))
            return 0;

        const WindowEvent::KeyData kd {
            .key = static_cast<int16_t>(from_win32_key(wp)),
            .scancode = static_cast<int>(HIWORD(lp) & 0x1FF),
            .mods = 0
        };

        self->m_held_keys_ui[kd.key] = kd;
        self->m_keys_dirty.store(true, std::memory_order_release);
        WindowEvent ev;
        ev.type = WindowEventType::KEY_PRESSED;
        ev.timestamp = 0.0;
        ev.data = kd;
        self->push_event(ev);
        return 0;
    }

    case WM_KEYUP:
    case WM_SYSKEYUP: {
        const WindowEvent::KeyData kd {
            .key = static_cast<int16_t>(from_win32_key(wp)),
            .scancode = static_cast<int>(HIWORD(lp) & 0x1FF),
            .mods = 0
        };

        self->m_held_keys_ui.erase(kd.key);
        self->m_keys_dirty.store(true, std::memory_order_release);
        WindowEvent ev;
        ev.type = WindowEventType::KEY_RELEASED;
        ev.timestamp = 0.0;
        ev.data = kd;
        self->push_event(ev);
        return 0;
    }

    case WM_MOUSEMOVE: {
        WindowEvent ev;
        ev.type = WindowEventType::MOUSE_MOTION;
        ev.timestamp = 0.0;
        ev.data = WindowEvent::MousePosData {
            .x = static_cast<double>(GET_X_LPARAM(lp)),
            .y = static_cast<double>(GET_Y_LPARAM(lp))
        };
        self->push_event(ev);
        return 0;
    }

    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN: {
        IO::MouseButtons btn = (msg == WM_LBUTTONDOWN) ? IO::MouseButtons::Left
            : (msg == WM_RBUTTONDOWN)                  ? IO::MouseButtons::Right
                                                       : IO::MouseButtons::Middle;
        WindowEvent ev;
        ev.type = WindowEventType::MOUSE_BUTTON_PRESSED;
        ev.timestamp = 0.0;
        ev.data = WindowEvent::MouseButtonData {
            .button = static_cast<int8_t>(btn),
            .mods = 0
        };
        self->push_event(ev);
        return 0;
    }

    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP: {
        IO::MouseButtons btn = (msg == WM_LBUTTONUP) ? IO::MouseButtons::Left
            : (msg == WM_RBUTTONUP)                  ? IO::MouseButtons::Right
                                                     : IO::MouseButtons::Middle;
        WindowEvent ev;
        ev.type = WindowEventType::MOUSE_BUTTON_RELEASED;
        ev.timestamp = 0.0;
        ev.data = WindowEvent::MouseButtonData {
            .button = static_cast<int8_t>(btn),
            .mods = 0
        };
        self->push_event(ev);
        return 0;
    }

    case WM_MOUSEWHEEL: {
        double delta = static_cast<double>(GET_WHEEL_DELTA_WPARAM(wp)) / WHEEL_DELTA;
        WindowEvent ev;
        ev.type = WindowEventType::MOUSE_SCROLLED;
        ev.timestamp = 0.0;
        ev.data = WindowEvent::ScrollData { .x_offset = 0.0, .y_offset = delta };
        self->push_event(ev);
        return 0;
    }

    default:
        break;
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ============================================================================
// push_event (UI thread)
// ============================================================================

void Win32Window::push_event(WindowEvent ev)
{
    (void)m_event_queue.push(ev);
}

// ============================================================================
// poll (graphics thread)
// ============================================================================

void Win32Window::poll()
{
    if (m_keys_dirty.exchange(false, std::memory_order_acq_rel)) {
        const bool was_empty = m_held_keys.empty();
        m_held_keys = m_held_keys_ui;
        if (was_empty && !m_held_keys.empty())
            m_repeat_next_tick = GetTickCount64() + 90;
    }

    while (auto ev = m_event_queue.pop()) {
        m_event_source.signal(*ev);
        if (m_event_callback)
            m_event_callback(*ev);
    }

    const ULONGLONG now = GetTickCount64();
    if (!m_held_keys.empty() && now >= m_repeat_next_tick) {
        m_repeat_next_tick = now + 16;
        for (const auto& [k, kd] : m_held_keys) {
            WindowEvent rev;
            rev.type = WindowEventType::KEY_REPEAT;
            rev.timestamp = 0.0;
            rev.data = kd;
            m_event_source.signal(rev);
            if (m_event_callback)
                m_event_callback(rev);
        }
    }
}

// ============================================================================
// Window interface
// ============================================================================

void Win32Window::show()
{
    if (m_hwnd)
        ShowWindow(m_hwnd, SW_SHOW);
}

void Win32Window::hide()
{
    if (m_hwnd)
        ShowWindow(m_hwnd, SW_HIDE);
}

void Win32Window::destroy()
{
    if (m_hwnd) {
        PostMessageW(m_hwnd, WM_CLOSE, 0, 0);
        m_hwnd = nullptr;
    }
    if (m_ui_thread.joinable())
        m_ui_thread.join();
}

bool Win32Window::should_close() const
{
    return m_should_close.load();
}

void Win32Window::set_event_callback(WindowEventCallback callback)
{
    m_event_callback = std::move(callback);
}

void Win32Window::set_title(const std::string& title)
{
    m_create_info.title = title;
    if (m_hwnd) {
        auto wt = std::wstring(title.begin(), title.end());
        SetWindowTextW(m_hwnd, wt.c_str());
    }
}

void Win32Window::set_size(uint32_t width, uint32_t height)
{
    m_create_info.width = width;
    m_create_info.height = height;
    if (m_hwnd)
        SetWindowPos(m_hwnd, nullptr, 0, 0,
            static_cast<int>(width), static_cast<int>(height),
            SWP_NOMOVE | SWP_NOZORDER);
}

void Win32Window::set_position(uint32_t x, uint32_t y)
{
    if (m_hwnd)
        SetWindowPos(m_hwnd, nullptr,
            static_cast<int>(x), static_cast<int>(y),
            0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

void Win32Window::set_color(const std::array<float, 4>& color)
{
    m_create_info.clear_color = color;
}

// ============================================================================
// Render tracking (mirrors GlfwWindow)
// ============================================================================

void Win32Window::register_rendering_buffer(std::shared_ptr<Buffers::VKBuffer> buffer)
{
    std::lock_guard lock(m_render_tracking_mutex);
    m_rendering_buffers.push_back(buffer);
}

void Win32Window::unregister_rendering_buffer(std::shared_ptr<Buffers::VKBuffer> buffer)
{
    std::lock_guard lock(m_render_tracking_mutex);
    std::erase_if(m_rendering_buffers,
        [&buffer](const auto& wp) {
            auto sp = wp.lock();
            return !sp || sp == buffer;
        });
}

void Win32Window::track_frame_command(uint64_t cmd_id)
{
    m_frame_commands.push_back(cmd_id);
}

const std::vector<uint64_t>& Win32Window::get_frame_commands() const
{
    return m_frame_commands;
}

void Win32Window::clear_frame_commands()
{
    m_frame_commands.clear();
}

std::vector<std::shared_ptr<Buffers::VKBuffer>> Win32Window::get_rendering_buffers() const
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

} // namespace MayaFlux::Core

#endif // MAYAFLUX_PLATFORM_WINDOWS
