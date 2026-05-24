#include "KeyMapping.hpp"

#ifdef MAYAFLUX_PLATFORM_WINDOWS

namespace MayaFlux::Core {

// ============================================================================
// from_win32_key
// ============================================================================

IO::Keys from_win32_key(WPARAM vk) noexcept
{
    if (vk >= 0x30 && vk <= 0x39)
        return static_cast<IO::Keys>(static_cast<int16_t>(vk));

    if (vk >= 0x41 && vk <= 0x5A)
        return static_cast<IO::Keys>(static_cast<int16_t>(vk));

    switch (vk) {
    case VK_SPACE:
        return IO::Keys::Space;
    case VK_OEM_7:
        return IO::Keys::Apostrophe;
    case VK_OEM_COMMA:
        return IO::Keys::Comma;
    case VK_OEM_MINUS:
        return IO::Keys::Minus;
    case VK_OEM_PERIOD:
        return IO::Keys::Period;
    case VK_OEM_2:
        return IO::Keys::Slash;
    case VK_OEM_1:
        return IO::Keys::Semicolon;
    case VK_OEM_PLUS:
        return IO::Keys::Equal;
    case VK_OEM_4:
        return IO::Keys::LeftBracket;
    case VK_OEM_5:
        return IO::Keys::Backslash;
    case VK_OEM_6:
        return IO::Keys::RightBracket;
    case VK_OEM_3:
        return IO::Keys::GraveAccent;

    case VK_ESCAPE:
        return IO::Keys::Escape;
    case VK_RETURN:
        return IO::Keys::Enter;
    case VK_TAB:
        return IO::Keys::Tab;
    case VK_BACK:
        return IO::Keys::Backspace;
    case VK_INSERT:
        return IO::Keys::Insert;
    case VK_DELETE:
        return IO::Keys::Delete;

    case VK_RIGHT:
        return IO::Keys::Right;
    case VK_LEFT:
        return IO::Keys::Left;
    case VK_DOWN:
        return IO::Keys::Down;
    case VK_UP:
        return IO::Keys::Up;

    case VK_PRIOR:
        return IO::Keys::PageUp;
    case VK_NEXT:
        return IO::Keys::PageDown;
    case VK_HOME:
        return IO::Keys::Home;
    case VK_END:
        return IO::Keys::End;

    case VK_CAPITAL:
        return IO::Keys::CapsLock;
    case VK_SCROLL:
        return IO::Keys::ScrollLock;
    case VK_NUMLOCK:
        return IO::Keys::NumLock;
    case VK_SNAPSHOT:
        return IO::Keys::PrintScreen;
    case VK_PAUSE:
        return IO::Keys::Pause;

    case VK_F1:
        return IO::Keys::F1;
    case VK_F2:
        return IO::Keys::F2;
    case VK_F3:
        return IO::Keys::F3;
    case VK_F4:
        return IO::Keys::F4;
    case VK_F5:
        return IO::Keys::F5;
    case VK_F6:
        return IO::Keys::F6;
    case VK_F7:
        return IO::Keys::F7;
    case VK_F8:
        return IO::Keys::F8;
    case VK_F9:
        return IO::Keys::F9;
    case VK_F10:
        return IO::Keys::F10;
    case VK_F11:
        return IO::Keys::F11;
    case VK_F12:
        return IO::Keys::F12;
    case VK_F13:
        return IO::Keys::F13;
    case VK_F14:
        return IO::Keys::F14;
    case VK_F15:
        return IO::Keys::F15;
    case VK_F16:
        return IO::Keys::F16;
    case VK_F17:
        return IO::Keys::F17;
    case VK_F18:
        return IO::Keys::F18;
    case VK_F19:
        return IO::Keys::F19;
    case VK_F20:
        return IO::Keys::F20;
    case VK_F21:
        return IO::Keys::F21;
    case VK_F22:
        return IO::Keys::F22;
    case VK_F23:
        return IO::Keys::F23;
    case VK_F24:
        return IO::Keys::F24;

    case VK_NUMPAD0:
        return IO::Keys::KP0;
    case VK_NUMPAD1:
        return IO::Keys::KP1;
    case VK_NUMPAD2:
        return IO::Keys::KP2;
    case VK_NUMPAD3:
        return IO::Keys::KP3;
    case VK_NUMPAD4:
        return IO::Keys::KP4;
    case VK_NUMPAD5:
        return IO::Keys::KP5;
    case VK_NUMPAD6:
        return IO::Keys::KP6;
    case VK_NUMPAD7:
        return IO::Keys::KP7;
    case VK_NUMPAD8:
        return IO::Keys::KP8;
    case VK_NUMPAD9:
        return IO::Keys::KP9;
    case VK_DECIMAL:
        return IO::Keys::KPDecimal;
    case VK_DIVIDE:
        return IO::Keys::KPDivide;
    case VK_MULTIPLY:
        return IO::Keys::KPMultiply;
    case VK_SUBTRACT:
        return IO::Keys::KPSubtract;
    case VK_ADD:
        return IO::Keys::KPAdd;
        // No Win32 VK for KPEnter distinct from VK_RETURN on most keyboards;
        // WM_KEYDOWN with VK_RETURN and an extended-key flag is the real signal,
        // but WndProc does not receive that distinction here. Left unmapped.

    case VK_LSHIFT:
        return IO::Keys::LShift;
    case VK_LCONTROL:
        return IO::Keys::LCtrl;
    case VK_LMENU:
        return IO::Keys::LAlt;
    case VK_LWIN:
        return IO::Keys::LSuper;
    case VK_RSHIFT:
        return IO::Keys::RShift;
    case VK_RCONTROL:
        return IO::Keys::RCtrl;
    case VK_RMENU:
        return IO::Keys::RAlt;
    case VK_RWIN:
        return IO::Keys::RSuper;
    // Non-sided VK_SHIFT / VK_CONTROL / VK_MENU arrive only when the OS
    // cannot distinguish sides; map to the left variant to match GLFW.
    case VK_SHIFT:
        return IO::Keys::LShift;
    case VK_CONTROL:
        return IO::Keys::LCtrl;
    case VK_MENU:
        return IO::Keys::LAlt;

    case VK_APPS:
        return IO::Keys::Menu;

    default:
        return IO::Keys::Unknown;
    }
}

// ============================================================================
// to_win32_key
// ============================================================================

int to_win32_key(IO::Keys key) noexcept
{
    using K = IO::Keys;

    int k = static_cast<int>(key);

    // Digits: Keys::N0=48 .. N9=57 == VK_0=0x30 .. VK_9=0x39
    if (k >= 48 && k <= 57)
        return k;

    // Letters: Keys::A=65 .. Z=90 == VK_A=0x41 .. VK_Z=0x5A
    if (k >= 65 && k <= 90)
        return k;

    switch (key) {
    case K::Space:
        return VK_SPACE;
    case K::Apostrophe:
        return VK_OEM_7;
    case K::Comma:
        return VK_OEM_COMMA;
    case K::Minus:
        return VK_OEM_MINUS;
    case K::Period:
        return VK_OEM_PERIOD;
    case K::Slash:
        return VK_OEM_2;
    case K::Semicolon:
        return VK_OEM_1;
    case K::Equal:
        return VK_OEM_PLUS;
    case K::LeftBracket:
        return VK_OEM_4;
    case K::Backslash:
        return VK_OEM_5;
    case K::RightBracket:
        return VK_OEM_6;
    case K::GraveAccent:
        return VK_OEM_3;

    case K::Escape:
        return VK_ESCAPE;
    case K::Enter:
        return VK_RETURN;
    case K::Tab:
        return VK_TAB;
    case K::Backspace:
        return VK_BACK;
    case K::Insert:
        return VK_INSERT;
    case K::Delete:
        return VK_DELETE;

    case K::Right:
        return VK_RIGHT;
    case K::Left:
        return VK_LEFT;
    case K::Down:
        return VK_DOWN;
    case K::Up:
        return VK_UP;

    case K::PageUp:
        return VK_PRIOR;
    case K::PageDown:
        return VK_NEXT;
    case K::Home:
        return VK_HOME;
    case K::End:
        return VK_END;

    case K::CapsLock:
        return VK_CAPITAL;
    case K::ScrollLock:
        return VK_SCROLL;
    case K::NumLock:
        return VK_NUMLOCK;
    case K::PrintScreen:
        return VK_SNAPSHOT;
    case K::Pause:
        return VK_PAUSE;

    case K::F1:
        return VK_F1;
    case K::F2:
        return VK_F2;
    case K::F3:
        return VK_F3;
    case K::F4:
        return VK_F4;
    case K::F5:
        return VK_F5;
    case K::F6:
        return VK_F6;
    case K::F7:
        return VK_F7;
    case K::F8:
        return VK_F8;
    case K::F9:
        return VK_F9;
    case K::F10:
        return VK_F10;
    case K::F11:
        return VK_F11;
    case K::F12:
        return VK_F12;
    case K::F13:
        return VK_F13;
    case K::F14:
        return VK_F14;
    case K::F15:
        return VK_F15;
    case K::F16:
        return VK_F16;
    case K::F17:
        return VK_F17;
    case K::F18:
        return VK_F18;
    case K::F19:
        return VK_F19;
    case K::F20:
        return VK_F20;
    case K::F21:
        return VK_F21;
    case K::F22:
        return VK_F22;
    case K::F23:
        return VK_F23;
    case K::F24:
        return VK_F24;
    case K::F25:
        return 0; // No Win32 equivalent

    case K::KP0:
        return VK_NUMPAD0;
    case K::KP1:
        return VK_NUMPAD1;
    case K::KP2:
        return VK_NUMPAD2;
    case K::KP3:
        return VK_NUMPAD3;
    case K::KP4:
        return VK_NUMPAD4;
    case K::KP5:
        return VK_NUMPAD5;
    case K::KP6:
        return VK_NUMPAD6;
    case K::KP7:
        return VK_NUMPAD7;
    case K::KP8:
        return VK_NUMPAD8;
    case K::KP9:
        return VK_NUMPAD9;
    case K::KPDecimal:
        return VK_DECIMAL;
    case K::KPDivide:
        return VK_DIVIDE;
    case K::KPMultiply:
        return VK_MULTIPLY;
    case K::KPSubtract:
        return VK_SUBTRACT;
    case K::KPAdd:
        return VK_ADD;
    case K::KPEnter:
        return 0; // No distinct VK; see from_win32_key note
    case K::KPEqual:
        return 0; // No Win32 equivalent

    case K::LShift:
        return VK_LSHIFT;
    case K::LCtrl:
        return VK_LCONTROL;
    case K::LAlt:
        return VK_LMENU;
    case K::LSuper:
        return VK_LWIN;
    case K::RShift:
        return VK_RSHIFT;
    case K::RCtrl:
        return VK_RCONTROL;
    case K::RAlt:
        return VK_RMENU;
    case K::RSuper:
        return VK_RWIN;
    case K::Menu:
        return VK_APPS;

    default:
        return 0;
    }
}

// ============================================================================
// is_valid_win32_key
// ============================================================================

bool is_valid_win32_key(WPARAM vk) noexcept
{
    return from_win32_key(vk) != IO::Keys::Unknown;
}

} // namespace MayaFlux::Core

#endif // MAYAFLUX_PLATFORM_WINDOWS
