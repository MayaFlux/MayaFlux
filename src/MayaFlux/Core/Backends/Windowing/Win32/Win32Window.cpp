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
// Raw mouse and clipboard helpers
// ============================================================================

std::optional<std::pair<double, double>> raw_mouse_delta(
    const RAWMOUSE& mouse, RawMouseState& state, SIZE primary, SIZE virtual_screen) noexcept
{
    if (mouse.usFlags & MOUSE_MOVE_ABSOLUTE) {
        const SIZE span = (mouse.usFlags & MOUSE_VIRTUAL_DESKTOP) ? virtual_screen : primary;
        const bool had_previous = state.absolute_seen;
        const LONG previous_x = state.last_x;
        const LONG previous_y = state.last_y;

        state.absolute_seen = true;
        state.last_x = mouse.lLastX;
        state.last_y = mouse.lLastY;

        if (!had_previous)
            return std::nullopt;

        const double dx = static_cast<double>(mouse.lLastX - previous_x) * static_cast<double>(span.cx) / 65535.0;
        const double dy = static_cast<double>(mouse.lLastY - previous_y) * static_cast<double>(span.cy) / 65535.0;
        if (dx == 0.0 && dy == 0.0)
            return std::nullopt;
        return std::pair<double, double> { dx, dy };
    }

    if (mouse.lLastX == 0 && mouse.lLastY == 0)
        return std::nullopt;
    return std::pair<double, double> { static_cast<double>(mouse.lLastX), static_cast<double>(mouse.lLastY) };
}

namespace {

    std::optional<std::wstring> utf8_to_clipboard_text(const std::string& utf8)
    {
        std::string normalised;
        normalised.reserve(utf8.size());
        for (size_t i = 0; i < utf8.size(); ++i) {
            if (utf8[i] == '\n' && (i == 0 || utf8[i - 1] != '\r'))
                normalised += '\r';
            normalised += utf8[i];
        }

        if (normalised.empty())
            return std::wstring {};

        const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            normalised.data(), static_cast<int>(normalised.size()), nullptr, 0);
        if (count <= 0)
            return std::nullopt;

        std::wstring wide(static_cast<size_t>(count), L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            normalised.data(), static_cast<int>(normalised.size()), wide.data(), count);
        return wide;
    }

    std::string clipboard_text_to_utf8(const wchar_t* wide)
    {
        const int count = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
        if (count <= 1)
            return {};

        std::string utf8(static_cast<size_t>(count - 1), '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8.data(), count, nullptr, nullptr);

        std::string text;
        text.reserve(utf8.size());
        for (size_t i = 0; i < utf8.size(); ++i) {
            if (utf8[i] == '\r' && i + 1 < utf8.size() && utf8[i + 1] == '\n')
                continue;
            text += utf8[i];
        }
        return text;
    }

    bool write_clipboard_text(HWND hwnd, const std::string& text)
    {
        const auto wide = utf8_to_clipboard_text(text);
        if (!wide)
            return true;

        const size_t bytes = (wide->size() + 1) * sizeof(wchar_t);
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (!memory)
            return true;

        void* locked = GlobalLock(memory);
        if (!locked) {
            GlobalFree(memory);
            return true;
        }
        std::memcpy(locked, wide->c_str(), bytes);
        GlobalUnlock(memory);

        if (!OpenClipboard(hwnd)) {
            GlobalFree(memory);
            return false;
        }

        EmptyClipboard();
        if (!SetClipboardData(CF_UNICODETEXT, memory))
            GlobalFree(memory);
        CloseClipboard();
        return true;
    }

    enum class ClipboardRead : uint8_t { Busy,
        Empty,
        Text };

    ClipboardRead read_clipboard_text(HWND hwnd, std::string& text)
    {
        if (!IsClipboardFormatAvailable(CF_UNICODETEXT))
            return ClipboardRead::Empty;

        if (!OpenClipboard(hwnd))
            return ClipboardRead::Busy;

        ClipboardRead result = ClipboardRead::Empty;
        if (HANDLE handle = GetClipboardData(CF_UNICODETEXT)) {
            if (const auto* wide = static_cast<const wchar_t*>(GlobalLock(handle))) {
                text = clipboard_text_to_utf8(wide);
                GlobalUnlock(handle);
                result = ClipboardRead::Text;
            }
        }

        CloseClipboard();
        return result;
    }

}

// ============================================================================
// Constructor / destructor
// ============================================================================

Win32Window::Win32Window(const WindowCreateInfo& create_info,
    const GlobalGraphicsConfig& graphics_config)
    : m_create_info(create_info)
    , m_key_repeat_config(graphics_config.key_repeat_config)
{
    m_hinstance = GetModuleHandleW(nullptr);

    m_ui_thread = std::thread([this]() { ui_thread_main(); });

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

    m_ui_hwnd = m_hwnd;
    m_ui_config = m_input_config;
    m_ui_alive.store(true);

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
    case k_message_input_config: {
        self->m_config_wake_posted.store(false);
        {
            std::lock_guard lock(self->m_config_mutex);
            self->m_ui_config = self->m_posted_config;
        }
        self->apply_cursor_mode();
        return 0;
    }

    case k_message_clipboard:
        self->m_clipboard_wake_posted.store(false);
        self->service_clipboard(false);
        return 0;

    case WM_TIMER:
        if (wp == k_timer_clipboard) {
            self->service_clipboard(false);
            return 0;
        }
        break;

    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT && self->m_ui_config.cursor_mode != CursorMode::NORMAL) {
            SetCursor(nullptr);
            return TRUE;
        }
        break;

    case WM_INPUT:
        if (self->m_cursor_locked_ui && self->m_lock_uses_raw_ui)
            self->handle_raw_input(reinterpret_cast<HRAWINPUT>(lp));
        return DefWindowProcW(hwnd, msg, wp, lp);

    case WM_MOVE:
        if (self->m_cursor_locked_ui && !self->update_clip())
            self->m_cursor_locked_ui = false;
        return 0;

    case WM_CLOSE:
        self->service_clipboard(true);
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
        self->m_ui_alive.store(false);
        self->service_clipboard(true);
        self->release_cursor_state();
        PostQuitMessage(0);
        return 0;

    case WM_ENTERSIZEMOVE:
        self->m_in_size_move.store(true, std::memory_order_release);
        return 0;

    case WM_EXITSIZEMOVE: {
        self->m_in_size_move.store(false, std::memory_order_release);
        WindowEvent ev;
        ev.type = WindowEventType::WINDOW_RESIZED;
        ev.timestamp = 0.0;
        ev.data = WindowEvent::ResizeData {
            .width = static_cast<uint32_t>(self->m_state.current_width),
            .height = static_cast<uint32_t>(self->m_state.current_height)
        };
        self->push_event(ev);
        return 0;
    }

    case WM_SIZE: {
        auto w = static_cast<uint32_t>(LOWORD(lp));
        auto h = static_cast<uint32_t>(HIWORD(lp));
        self->m_state.current_width = static_cast<int>(w);
        self->m_state.current_height = static_cast<int>(h);
        if (self->m_cursor_locked_ui && !self->update_clip())
            self->m_cursor_locked_ui = false;
        WindowEvent ev;
        ev.type = WindowEventType::WINDOW_RESIZED;
        ev.timestamp = 0.0;
        ev.data = WindowEvent::ResizeData { .width = w, .height = h };
        self->push_event(ev);
        return 0;
    }

    case WM_SETFOCUS: {
        self->seed_modifier_keys();
        self->m_state.is_focused = true;
        self->apply_cursor_mode();
        WindowEvent ev;
        ev.type = WindowEventType::WINDOW_FOCUS_GAINED;
        ev.timestamp = 0.0;
        self->push_event(ev);
        return 0;
    }

    case WM_KILLFOCUS: {
        self->release_held_keys();
        self->m_state.is_focused = false;
        self->apply_cursor_mode();
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

        const uint32_t scancode = static_cast<uint32_t>(HIWORD(lp) & 0x1FF);
        IO::Keys key = from_win32_scancode(scancode);
        if (key == IO::Keys::Unknown)
            key = from_win32_key(wp);

        self->m_modifier_keys_ui.set(key, true);
        const WindowEvent::KeyData kd {
            .key = static_cast<int16_t>(key),
            .scancode = static_cast<int32_t>(scancode),
            .mods = self->update_mods()
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
        const uint32_t scancode = static_cast<uint32_t>(HIWORD(lp) & 0x1FF);
        IO::Keys key = from_win32_scancode(scancode);
        if (key == IO::Keys::Unknown)
            key = from_win32_key(wp);

        self->m_modifier_keys_ui.set(key, false);
        const WindowEvent::KeyData kd {
            .key = static_cast<int16_t>(key),
            .scancode = static_cast<int32_t>(scancode),
            .mods = self->update_mods()
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

    case WM_CHAR: {
        const auto unit = static_cast<uint16_t>(wp);

        if (unit >= 0xD800 && unit <= 0xDBFF) {
            self->m_pending_high_surrogate = unit;
            return 0;
        }

        uint32_t codepoint = unit;
        if (unit >= 0xDC00 && unit <= 0xDFFF && self->m_pending_high_surrogate != 0) {
            codepoint = 0x10000
                + (static_cast<uint32_t>(self->m_pending_high_surrogate - 0xD800) << 10)
                + (static_cast<uint32_t>(unit - 0xDC00));
            self->m_pending_high_surrogate = 0;
        } else if (unit >= 0xDC00 && unit <= 0xDFFF) {
            self->m_pending_high_surrogate = 0;
            return 0;
        } else {
            self->m_pending_high_surrogate = 0;
        }

        if (!is_text_input_codepoint(codepoint)) {
            return 0;
        }

        WindowEvent ev;
        ev.type = WindowEventType::TEXT_INPUT;
        ev.timestamp = 0.0;
        ev.data = WindowEvent::TextData { .codepoint = codepoint };
        self->push_event(ev);
        return 0;
    }

    case WM_MOUSEMOVE: {
        if (self->m_cursor_locked_ui) {
            self->handle_locked_motion(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            return 0;
        }

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
            .mods = self->update_mods()
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
            .mods = self->update_mods()
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

IO::Modifiers Win32Window::update_mods()
{
    IO::Modifiers mods = m_modifier_keys_ui.held();
    if (GetKeyState(VK_CAPITAL) & 1)
        mods = mods | IO::Modifiers::CapsLock;
    if (GetKeyState(VK_NUMLOCK) & 1)
        mods = mods | IO::Modifiers::NumLock;

    m_mods.store(mods, std::memory_order_release);
    return mods;
}

void Win32Window::seed_modifier_keys()
{
    constexpr std::array<std::pair<int, IO::Keys>, 8> k_modifier_vks { {
        { VK_LSHIFT, IO::Keys::LShift },
        { VK_RSHIFT, IO::Keys::RShift },
        { VK_LCONTROL, IO::Keys::LCtrl },
        { VK_RCONTROL, IO::Keys::RCtrl },
        { VK_LMENU, IO::Keys::LAlt },
        { VK_RMENU, IO::Keys::RAlt },
        { VK_LWIN, IO::Keys::LSuper },
        { VK_RWIN, IO::Keys::RSuper },
    } };

    m_modifier_keys_ui.clear();
    for (const auto& [vk, key] : k_modifier_vks)
        m_modifier_keys_ui.set(key, (GetKeyState(vk) & 0x8000) != 0);
    update_mods();
}

void Win32Window::release_held_keys()
{
    const auto held = std::move(m_held_keys_ui);
    m_held_keys_ui.clear();
    m_modifier_keys_ui.clear();
    m_keys_dirty.store(true, std::memory_order_release);
    const IO::Modifiers mods = update_mods();

    for (const auto& [k, kd] : held) {
        WindowEvent::KeyData released = kd;
        released.mods = mods;
        WindowEvent ev;
        ev.type = WindowEventType::KEY_RELEASED;
        ev.timestamp = 0.0;
        ev.data = released;
        push_event(ev);
    }
}

// ============================================================================
// Cursor modes (UI thread)
// ============================================================================

void Win32Window::apply_cursor_mode()
{
    const CursorMode mode = m_ui_config.cursor_mode;
    const bool wants_lock = mode == CursorMode::DISABLED || mode == CursorMode::CAPTURED;
    const bool wants_raw = wants_lock && (mode == CursorMode::CAPTURED || m_ui_config.raw_mouse_motion);

    if (wants_lock && m_state.is_focused) {
        if (wants_raw)
            register_raw_mouse();
        else
            unregister_raw_mouse();
        m_lock_uses_raw_ui = m_raw_registered_ui;
        lock_cursor();
    } else {
        unregister_raw_mouse();
        unlock_cursor();
        m_lock_uses_raw_ui = false;
    }

    update_cursor_visibility();
}

bool Win32Window::update_clip()
{
    RECT client {};
    if (!GetClientRect(m_ui_hwnd, &client))
        return false;

    POINT top_left { client.left, client.top };
    POINT bottom_right { client.right, client.bottom };
    ClientToScreen(m_ui_hwnd, &top_left);
    ClientToScreen(m_ui_hwnd, &bottom_right);

    const RECT screen { top_left.x, top_left.y, bottom_right.x, bottom_right.y };
    if (!ClipCursor(&screen)) {
        MF_WARN_ONCE(Journal::Component::Core, Journal::Context::WindowingSubsystem,
            "ClipCursor failed; cursor_mode DISABLED and CAPTURED behave as NORMAL");
        return false;
    }

    m_center_client_ui = { client.right / 2, client.bottom / 2 };
    m_center_screen_ui = { top_left.x + (bottom_right.x - top_left.x) / 2,
        top_left.y + (bottom_right.y - top_left.y) / 2 };
    return true;
}

void Win32Window::lock_cursor()
{
    if (!update_clip()) {
        m_cursor_locked_ui = false;
        return;
    }

    if (m_cursor_locked_ui)
        return;

    m_virtual_x = static_cast<double>(m_center_client_ui.x);
    m_virtual_y = static_cast<double>(m_center_client_ui.y);
    m_raw_state_ui = {};
    SetCursorPos(m_center_screen_ui.x, m_center_screen_ui.y);
    m_cursor_locked_ui = true;
}

void Win32Window::unlock_cursor()
{
    if (!m_cursor_locked_ui)
        return;

    ClipCursor(nullptr);
    m_cursor_locked_ui = false;
}

void Win32Window::register_raw_mouse()
{
    if (m_raw_registered_ui)
        return;

    UINT count = 0;
    GetRegisteredRawInputDevices(nullptr, &count, sizeof(RAWINPUTDEVICE));
    std::vector<RAWINPUTDEVICE> existing(count);
    if (count > 0
        && GetRegisteredRawInputDevices(existing.data(), &count, sizeof(RAWINPUTDEVICE)) == static_cast<UINT>(-1)) {
        existing.clear();
    }

    m_saved_raw_mouse_ui.reset();
    for (const auto& device : existing) {
        if (device.usUsagePage == 0x01 && device.usUsage == 0x02)
            m_saved_raw_mouse_ui = device;
    }

    const RAWINPUTDEVICE mouse { 0x01, 0x02, 0, m_ui_hwnd };
    if (!RegisterRawInputDevices(&mouse, 1, sizeof(mouse))) {
        MF_WARN_ONCE(Journal::Component::Core, Journal::Context::WindowingSubsystem,
            "RegisterRawInputDevices failed; unaccelerated motion falls back to window motion");
        m_saved_raw_mouse_ui.reset();
        return;
    }

    m_raw_registered_ui = true;
}

void Win32Window::unregister_raw_mouse()
{
    if (!m_raw_registered_ui)
        return;

    const RAWINPUTDEVICE restore = m_saved_raw_mouse_ui
        ? *m_saved_raw_mouse_ui
        : RAWINPUTDEVICE { 0x01, 0x02, RIDEV_REMOVE, nullptr };
    RegisterRawInputDevices(&restore, 1, sizeof(restore));

    m_saved_raw_mouse_ui.reset();
    m_raw_registered_ui = false;
}

void Win32Window::update_cursor_visibility()
{
    const bool hide = m_ui_config.cursor_mode != CursorMode::NORMAL;
    if (hide == m_cursor_hidden_ui)
        return;

    m_cursor_hidden_ui = hide;
    refresh_cursor();
}

void Win32Window::refresh_cursor()
{
    POINT cursor {};
    if (!GetCursorPos(&cursor) || WindowFromPoint(cursor) != m_ui_hwnd)
        return;

    const LRESULT hit = SendMessageW(m_ui_hwnd, WM_NCHITTEST, 0,
        MAKELPARAM(static_cast<WORD>(cursor.x), static_cast<WORD>(cursor.y)));
    SendMessageW(m_ui_hwnd, WM_SETCURSOR, reinterpret_cast<WPARAM>(m_ui_hwnd),
        MAKELPARAM(static_cast<WORD>(LOWORD(hit)), WM_MOUSEMOVE));
}

void Win32Window::emit_virtual_motion()
{
    WindowEvent ev;
    ev.type = WindowEventType::MOUSE_MOTION;
    ev.timestamp = 0.0;
    ev.data = WindowEvent::MousePosData { .x = m_virtual_x, .y = m_virtual_y };
    push_event(ev);
}

void Win32Window::handle_locked_motion(int x, int y)
{
    if (m_lock_uses_raw_ui)
        return;

    if (x == m_center_client_ui.x && y == m_center_client_ui.y)
        return;

    m_virtual_x += static_cast<double>(x - m_center_client_ui.x);
    m_virtual_y += static_cast<double>(y - m_center_client_ui.y);
    emit_virtual_motion();
    SetCursorPos(m_center_screen_ui.x, m_center_screen_ui.y);
}

void Win32Window::handle_raw_input(HRAWINPUT handle)
{
    UINT size = 0;
    if (GetRawInputData(handle, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER)) != 0 || size == 0)
        return;

    std::vector<BYTE> buffer(size);
    if (GetRawInputData(handle, RID_INPUT, buffer.data(), &size, sizeof(RAWINPUTHEADER)) != size)
        return;

    const auto* input = reinterpret_cast<const RAWINPUT*>(buffer.data());
    if (input->header.dwType != RIM_TYPEMOUSE)
        return;

    const SIZE primary { GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
    const SIZE virtual_screen { GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN) };

    const auto delta = raw_mouse_delta(input->data.mouse, m_raw_state_ui, primary, virtual_screen);
    if (!delta)
        return;

    m_virtual_x += delta->first;
    m_virtual_y += delta->second;
    emit_virtual_motion();
}

void Win32Window::release_cursor_state()
{
    unregister_raw_mouse();
    unlock_cursor();
    m_lock_uses_raw_ui = false;
}

void Win32Window::set_input_config(const InputConfig& config)
{
    m_input_config = config;
    {
        std::lock_guard lock(m_config_mutex);
        m_posted_config = config;
    }
    wake_input_config();
}

void Win32Window::wake_input_config()
{
    if (!m_ui_alive.load() || m_config_wake_posted.exchange(true))
        return;

    if (!PostMessageW(m_ui_hwnd, k_message_input_config, 0, 0))
        m_config_wake_posted.store(false);
}

// ============================================================================
// Clipboard
// ============================================================================

void Win32Window::request_clipboard_text(ClipboardTextCallback callback)
{
    if (!callback)
        return;

    {
        std::lock_guard lock(m_clipboard_mutex);
        m_clipboard_reads.push_back({ .callback = std::move(callback), .attempts = 0 });
    }
    wake_clipboard();
}

void Win32Window::set_clipboard_text(const std::string& text)
{
    {
        std::lock_guard lock(m_clipboard_mutex);
        m_clipboard_writes.push_back({ .text = text, .attempts = 0 });
    }
    wake_clipboard();
}

void Win32Window::wake_clipboard()
{
    if (!m_ui_alive.load()) {
        fail_pending_clipboard();
        return;
    }

    if (m_clipboard_wake_posted.exchange(true))
        return;

    if (!PostMessageW(m_ui_hwnd, k_message_clipboard, 0, 0)) {
        m_clipboard_wake_posted.store(false);
        fail_pending_clipboard();
    }
}

void Win32Window::fail_pending_clipboard()
{
    std::lock_guard lock(m_clipboard_mutex);
    m_clipboard_writes.clear();
    for (auto& job : m_clipboard_reads)
        m_clipboard_done.push_back({ .callback = std::move(job.callback), .text = std::nullopt });
    m_clipboard_reads.clear();
}

void Win32Window::service_clipboard(bool final)
{
    std::vector<ClipboardWriteJob> writes;
    std::vector<ClipboardReadJob> reads;
    {
        std::lock_guard lock(m_clipboard_mutex);
        writes.swap(m_clipboard_writes);
        reads.swap(m_clipboard_reads);
    }

    std::vector<ClipboardWriteJob> writes_left;
    std::vector<ClipboardReadJob> reads_left;
    std::vector<ClipboardDelivery> done;
    bool blocked = false;

    for (auto& job : writes) {
        if (!blocked && write_clipboard_text(m_ui_hwnd, job.text))
            continue;

        if (final || ++job.attempts >= k_clipboard_attempts)
            continue;

        blocked = true;
        writes_left.push_back(std::move(job));
    }

    for (auto& job : reads) {
        std::string text;
        const ClipboardRead status = blocked ? ClipboardRead::Busy : read_clipboard_text(m_ui_hwnd, text);

        if (status == ClipboardRead::Text) {
            done.push_back({ .callback = std::move(job.callback), .text = std::move(text) });
        } else if (status == ClipboardRead::Empty || final || ++job.attempts >= k_clipboard_attempts) {
            done.push_back({ .callback = std::move(job.callback), .text = std::nullopt });
        } else {
            reads_left.push_back(std::move(job));
        }
    }

    const bool retry = !writes_left.empty() || !reads_left.empty();
    {
        std::lock_guard lock(m_clipboard_mutex);
        m_clipboard_writes.insert(m_clipboard_writes.begin(),
            std::make_move_iterator(writes_left.begin()), std::make_move_iterator(writes_left.end()));
        m_clipboard_reads.insert(m_clipboard_reads.begin(),
            std::make_move_iterator(reads_left.begin()), std::make_move_iterator(reads_left.end()));
        for (auto& delivery : done)
            m_clipboard_done.push_back(std::move(delivery));
    }

    if (retry)
        SetTimer(m_ui_hwnd, k_timer_clipboard, k_clipboard_retry_ms, nullptr);
    else
        KillTimer(m_ui_hwnd, k_timer_clipboard);
}

void Win32Window::deliver_clipboard()
{
    std::vector<ClipboardDelivery> ready;
    {
        std::lock_guard lock(m_clipboard_mutex);
        ready.swap(m_clipboard_done);
    }

    for (auto& delivery : ready)
        delivery.callback(std::move(delivery.text));
}

// ============================================================================
// poll (graphics thread)
// ============================================================================

void Win32Window::poll()
{
    deliver_clipboard();

    if (m_keys_dirty.exchange(false, std::memory_order_acq_rel)) {
        const bool was_empty = m_held_keys.empty();
        m_held_keys = m_held_keys_ui;
        if (was_empty && !m_held_keys.empty())
            m_repeat_next_tick = GetTickCount64() + m_key_repeat_config.initial_delay_ms;
    }

    while (auto ev = m_event_queue.pop()) {
        const bool suppress_signal = ev->type == WindowEventType::WINDOW_RESIZED
            && m_in_size_move.load(std::memory_order_acquire);
        if (!suppress_signal)
            m_event_source.signal(*ev);
        if (m_event_callback)
            m_event_callback(*ev);
    }

    const ULONGLONG now = GetTickCount64();
    if (!m_held_keys.empty() && now >= m_repeat_next_tick) {
        m_repeat_next_tick = now + m_key_repeat_config.interval_ms;
        const IO::Modifiers mods = m_mods.load(std::memory_order_acquire);
        for (const auto& [k, kd] : m_held_keys) {
            WindowEvent::KeyData repeated = kd;
            repeated.mods = mods;
            WindowEvent rev;
            rev.type = WindowEventType::KEY_REPEAT;
            rev.timestamp = 0.0;
            rev.data = repeated;
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

    deliver_clipboard();
}

bool Win32Window::should_close() const
{
    return m_should_close.load();
}

void Win32Window::request_close()
{
    if (!m_should_close.exchange(true)) {
        WindowEvent ev;
        ev.type = WindowEventType::WINDOW_CLOSED;
        ev.timestamp = 0.0;
        push_event(ev);
    }
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
// Render tracking
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
