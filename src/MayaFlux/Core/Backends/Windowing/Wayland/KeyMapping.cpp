#include "KeyMapping.hpp"

#ifdef MAYAFLUX_PLATFORM_LINUX

namespace MayaFlux::Core {

// ============================================================================
// from_xkb_keysym
// ============================================================================

IO::Keys from_xkb_keysym(xkb_keysym_t sym) noexcept
{
    if (sym >= 0x0020 && sym <= 0x007e)
        return static_cast<IO::Keys>(static_cast<int16_t>(sym));

    switch (sym) {
    case XKB_KEY_Escape:
        return IO::Keys::Escape;
    case XKB_KEY_Return:
        return IO::Keys::Enter;
    case XKB_KEY_Tab:
        return IO::Keys::Tab;
    case XKB_KEY_BackSpace:
        return IO::Keys::Backspace;
    case XKB_KEY_Insert:
        return IO::Keys::Insert;
    case XKB_KEY_Delete:
        return IO::Keys::Delete;

    case XKB_KEY_Right:
        return IO::Keys::Right;
    case XKB_KEY_Left:
        return IO::Keys::Left;
    case XKB_KEY_Down:
        return IO::Keys::Down;
    case XKB_KEY_Up:
        return IO::Keys::Up;

    case XKB_KEY_Page_Up:
        return IO::Keys::PageUp;
    case XKB_KEY_Page_Down:
        return IO::Keys::PageDown;
    case XKB_KEY_Home:
        return IO::Keys::Home;
    case XKB_KEY_End:
        return IO::Keys::End;

    case XKB_KEY_Caps_Lock:
        return IO::Keys::CapsLock;
    case XKB_KEY_Scroll_Lock:
        return IO::Keys::ScrollLock;
    case XKB_KEY_Num_Lock:
        return IO::Keys::NumLock;
    case XKB_KEY_Print:
        return IO::Keys::PrintScreen;
    case XKB_KEY_Pause:
        return IO::Keys::Pause;

    case XKB_KEY_F1:
        return IO::Keys::F1;
    case XKB_KEY_F2:
        return IO::Keys::F2;
    case XKB_KEY_F3:
        return IO::Keys::F3;
    case XKB_KEY_F4:
        return IO::Keys::F4;
    case XKB_KEY_F5:
        return IO::Keys::F5;
    case XKB_KEY_F6:
        return IO::Keys::F6;
    case XKB_KEY_F7:
        return IO::Keys::F7;
    case XKB_KEY_F8:
        return IO::Keys::F8;
    case XKB_KEY_F9:
        return IO::Keys::F9;
    case XKB_KEY_F10:
        return IO::Keys::F10;
    case XKB_KEY_F11:
        return IO::Keys::F11;
    case XKB_KEY_F12:
        return IO::Keys::F12;
    case XKB_KEY_F13:
        return IO::Keys::F13;
    case XKB_KEY_F14:
        return IO::Keys::F14;
    case XKB_KEY_F15:
        return IO::Keys::F15;
    case XKB_KEY_F16:
        return IO::Keys::F16;
    case XKB_KEY_F17:
        return IO::Keys::F17;
    case XKB_KEY_F18:
        return IO::Keys::F18;
    case XKB_KEY_F19:
        return IO::Keys::F19;
    case XKB_KEY_F20:
        return IO::Keys::F20;
    case XKB_KEY_F21:
        return IO::Keys::F21;
    case XKB_KEY_F22:
        return IO::Keys::F22;
    case XKB_KEY_F23:
        return IO::Keys::F23;
    case XKB_KEY_F24:
        return IO::Keys::F24;
    case XKB_KEY_F25:
        return IO::Keys::F25;

    case XKB_KEY_KP_0:
        return IO::Keys::KP0;
    case XKB_KEY_KP_1:
        return IO::Keys::KP1;
    case XKB_KEY_KP_2:
        return IO::Keys::KP2;
    case XKB_KEY_KP_3:
        return IO::Keys::KP3;
    case XKB_KEY_KP_4:
        return IO::Keys::KP4;
    case XKB_KEY_KP_5:
        return IO::Keys::KP5;
    case XKB_KEY_KP_6:
        return IO::Keys::KP6;
    case XKB_KEY_KP_7:
        return IO::Keys::KP7;
    case XKB_KEY_KP_8:
        return IO::Keys::KP8;
    case XKB_KEY_KP_9:
        return IO::Keys::KP9;
    case XKB_KEY_KP_Decimal:
        return IO::Keys::KPDecimal;
    case XKB_KEY_KP_Divide:
        return IO::Keys::KPDivide;
    case XKB_KEY_KP_Multiply:
        return IO::Keys::KPMultiply;
    case XKB_KEY_KP_Subtract:
        return IO::Keys::KPSubtract;
    case XKB_KEY_KP_Add:
        return IO::Keys::KPAdd;
    case XKB_KEY_KP_Enter:
        return IO::Keys::KPEnter;
    case XKB_KEY_KP_Equal:
        return IO::Keys::KPEqual;

    case XKB_KEY_Shift_L:
        return IO::Keys::LShift;
    case XKB_KEY_Control_L:
        return IO::Keys::LCtrl;
    case XKB_KEY_Alt_L:
        return IO::Keys::LAlt;
    case XKB_KEY_Super_L:
        return IO::Keys::LSuper;
    case XKB_KEY_Shift_R:
        return IO::Keys::RShift;
    case XKB_KEY_Control_R:
        return IO::Keys::RCtrl;
    case XKB_KEY_Alt_R:
        return IO::Keys::RAlt;
    case XKB_KEY_Super_R:
        return IO::Keys::RSuper;
    case XKB_KEY_Menu:
        return IO::Keys::Menu;

    default:
        return IO::Keys::Unknown;
    }
}

// ============================================================================
// to_xkb_keysym
// ============================================================================

xkb_keysym_t to_xkb_keysym(IO::Keys key) noexcept
{
    int k = static_cast<int>(key);

    if (k >= 0x0020 && k <= 0x007e)
        return static_cast<xkb_keysym_t>(k);

    using K = IO::Keys;
    switch (key) {
    case K::Escape:
        return XKB_KEY_Escape;
    case K::Enter:
        return XKB_KEY_Return;
    case K::Tab:
        return XKB_KEY_Tab;
    case K::Backspace:
        return XKB_KEY_BackSpace;
    case K::Insert:
        return XKB_KEY_Insert;
    case K::Delete:
        return XKB_KEY_Delete;

    case K::Right:
        return XKB_KEY_Right;
    case K::Left:
        return XKB_KEY_Left;
    case K::Down:
        return XKB_KEY_Down;
    case K::Up:
        return XKB_KEY_Up;

    case K::PageUp:
        return XKB_KEY_Page_Up;
    case K::PageDown:
        return XKB_KEY_Page_Down;
    case K::Home:
        return XKB_KEY_Home;
    case K::End:
        return XKB_KEY_End;

    case K::CapsLock:
        return XKB_KEY_Caps_Lock;
    case K::ScrollLock:
        return XKB_KEY_Scroll_Lock;
    case K::NumLock:
        return XKB_KEY_Num_Lock;
    case K::PrintScreen:
        return XKB_KEY_Print;
    case K::Pause:
        return XKB_KEY_Pause;

    case K::F1:
        return XKB_KEY_F1;
    case K::F2:
        return XKB_KEY_F2;
    case K::F3:
        return XKB_KEY_F3;
    case K::F4:
        return XKB_KEY_F4;
    case K::F5:
        return XKB_KEY_F5;
    case K::F6:
        return XKB_KEY_F6;
    case K::F7:
        return XKB_KEY_F7;
    case K::F8:
        return XKB_KEY_F8;
    case K::F9:
        return XKB_KEY_F9;
    case K::F10:
        return XKB_KEY_F10;
    case K::F11:
        return XKB_KEY_F11;
    case K::F12:
        return XKB_KEY_F12;
    case K::F13:
        return XKB_KEY_F13;
    case K::F14:
        return XKB_KEY_F14;
    case K::F15:
        return XKB_KEY_F15;
    case K::F16:
        return XKB_KEY_F16;
    case K::F17:
        return XKB_KEY_F17;
    case K::F18:
        return XKB_KEY_F18;
    case K::F19:
        return XKB_KEY_F19;
    case K::F20:
        return XKB_KEY_F20;
    case K::F21:
        return XKB_KEY_F21;
    case K::F22:
        return XKB_KEY_F22;
    case K::F23:
        return XKB_KEY_F23;
    case K::F24:
        return XKB_KEY_F24;
    case K::F25:
        return XKB_KEY_F25;

    case K::KP0:
        return XKB_KEY_KP_0;
    case K::KP1:
        return XKB_KEY_KP_1;
    case K::KP2:
        return XKB_KEY_KP_2;
    case K::KP3:
        return XKB_KEY_KP_3;
    case K::KP4:
        return XKB_KEY_KP_4;
    case K::KP5:
        return XKB_KEY_KP_5;
    case K::KP6:
        return XKB_KEY_KP_6;
    case K::KP7:
        return XKB_KEY_KP_7;
    case K::KP8:
        return XKB_KEY_KP_8;
    case K::KP9:
        return XKB_KEY_KP_9;
    case K::KPDecimal:
        return XKB_KEY_KP_Decimal;
    case K::KPDivide:
        return XKB_KEY_KP_Divide;
    case K::KPMultiply:
        return XKB_KEY_KP_Multiply;
    case K::KPSubtract:
        return XKB_KEY_KP_Subtract;
    case K::KPAdd:
        return XKB_KEY_KP_Add;
    case K::KPEnter:
        return XKB_KEY_KP_Enter;
    case K::KPEqual:
        return XKB_KEY_KP_Equal;

    case K::LShift:
        return XKB_KEY_Shift_L;
    case K::LCtrl:
        return XKB_KEY_Control_L;
    case K::LAlt:
        return XKB_KEY_Alt_L;
    case K::LSuper:
        return XKB_KEY_Super_L;
    case K::RShift:
        return XKB_KEY_Shift_R;
    case K::RCtrl:
        return XKB_KEY_Control_R;
    case K::RAlt:
        return XKB_KEY_Alt_R;
    case K::RSuper:
        return XKB_KEY_Super_R;
    case K::Menu:
        return XKB_KEY_Menu;

    default:
        return XKB_KEY_NoSymbol;
    }
}

// ============================================================================
// is_valid_xkb_keysym
// ============================================================================

bool is_valid_xkb_keysym(xkb_keysym_t sym) noexcept
{
    if (sym == XKB_KEY_NoSymbol)
        return false;
    return from_xkb_keysym(sym) != IO::Keys::Unknown;
}

} // namespace MayaFlux::Core

#endif // MAYAFLUX_PLATFORM_LINUX
