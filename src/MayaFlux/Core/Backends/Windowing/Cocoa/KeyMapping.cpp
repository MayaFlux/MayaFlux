#include "KeyMapping.hpp"

#ifdef MAYAFLUX_PLATFORM_MACOS

#include <HIToolbox/Events.h>

#include <array>

namespace MayaFlux::Core {

namespace {

constexpr auto key_map = [] {
    std::array<IO::Keys, 128> keys;
    keys.fill(IO::Keys::Unknown);

    keys[kVK_ANSI_0] = IO::Keys::N0;
    keys[kVK_ANSI_1] = IO::Keys::N1;
    keys[kVK_ANSI_2] = IO::Keys::N2;
    keys[kVK_ANSI_3] = IO::Keys::N3;
    keys[kVK_ANSI_4] = IO::Keys::N4;
    keys[kVK_ANSI_5] = IO::Keys::N5;
    keys[kVK_ANSI_6] = IO::Keys::N6;
    keys[kVK_ANSI_7] = IO::Keys::N7;
    keys[kVK_ANSI_8] = IO::Keys::N8;
    keys[kVK_ANSI_9] = IO::Keys::N9;

    keys[kVK_ANSI_A] = IO::Keys::A;
    keys[kVK_ANSI_B] = IO::Keys::B;
    keys[kVK_ANSI_C] = IO::Keys::C;
    keys[kVK_ANSI_D] = IO::Keys::D;
    keys[kVK_ANSI_E] = IO::Keys::E;
    keys[kVK_ANSI_F] = IO::Keys::F;
    keys[kVK_ANSI_G] = IO::Keys::G;
    keys[kVK_ANSI_H] = IO::Keys::H;
    keys[kVK_ANSI_I] = IO::Keys::I;
    keys[kVK_ANSI_J] = IO::Keys::J;
    keys[kVK_ANSI_K] = IO::Keys::K;
    keys[kVK_ANSI_L] = IO::Keys::L;
    keys[kVK_ANSI_M] = IO::Keys::M;
    keys[kVK_ANSI_N] = IO::Keys::N;
    keys[kVK_ANSI_O] = IO::Keys::O;
    keys[kVK_ANSI_P] = IO::Keys::P;
    keys[kVK_ANSI_Q] = IO::Keys::Q;
    keys[kVK_ANSI_R] = IO::Keys::R;
    keys[kVK_ANSI_S] = IO::Keys::S;
    keys[kVK_ANSI_T] = IO::Keys::T;
    keys[kVK_ANSI_U] = IO::Keys::U;
    keys[kVK_ANSI_V] = IO::Keys::V;
    keys[kVK_ANSI_W] = IO::Keys::W;
    keys[kVK_ANSI_X] = IO::Keys::X;
    keys[kVK_ANSI_Y] = IO::Keys::Y;
    keys[kVK_ANSI_Z] = IO::Keys::Z;

    keys[kVK_ANSI_Quote] = IO::Keys::Apostrophe;
    keys[kVK_ANSI_Comma] = IO::Keys::Comma;
    keys[kVK_ANSI_Minus] = IO::Keys::Minus;
    keys[kVK_ANSI_Period] = IO::Keys::Period;
    keys[kVK_ANSI_Slash] = IO::Keys::Slash;
    keys[kVK_ANSI_Semicolon] = IO::Keys::Semicolon;
    keys[kVK_ANSI_Equal] = IO::Keys::Equal;
    keys[kVK_ANSI_LeftBracket] = IO::Keys::LeftBracket;
    keys[kVK_ANSI_Backslash] = IO::Keys::Backslash;
    keys[kVK_ANSI_RightBracket] = IO::Keys::RightBracket;
    keys[kVK_ANSI_Grave] = IO::Keys::GraveAccent;
    keys[kVK_Space] = IO::Keys::Space;

    keys[kVK_Escape] = IO::Keys::Escape;
    keys[kVK_Return] = IO::Keys::Enter;
    keys[kVK_Tab] = IO::Keys::Tab;
    keys[kVK_Delete] = IO::Keys::Backspace;
    keys[kVK_Help] = IO::Keys::Insert;
    keys[kVK_ForwardDelete] = IO::Keys::Delete;
    keys[kVK_RightArrow] = IO::Keys::Right;
    keys[kVK_LeftArrow] = IO::Keys::Left;
    keys[kVK_DownArrow] = IO::Keys::Down;
    keys[kVK_UpArrow] = IO::Keys::Up;
    keys[kVK_PageUp] = IO::Keys::PageUp;
    keys[kVK_PageDown] = IO::Keys::PageDown;
    keys[kVK_Home] = IO::Keys::Home;
    keys[kVK_End] = IO::Keys::End;
    keys[kVK_CapsLock] = IO::Keys::CapsLock;
    keys[kVK_ANSI_KeypadClear] = IO::Keys::NumLock;

    keys[kVK_F1] = IO::Keys::F1;
    keys[kVK_F2] = IO::Keys::F2;
    keys[kVK_F3] = IO::Keys::F3;
    keys[kVK_F4] = IO::Keys::F4;
    keys[kVK_F5] = IO::Keys::F5;
    keys[kVK_F6] = IO::Keys::F6;
    keys[kVK_F7] = IO::Keys::F7;
    keys[kVK_F8] = IO::Keys::F8;
    keys[kVK_F9] = IO::Keys::F9;
    keys[kVK_F10] = IO::Keys::F10;
    keys[kVK_F11] = IO::Keys::F11;
    keys[kVK_F12] = IO::Keys::F12;
    keys[kVK_F13] = IO::Keys::F13;
    keys[kVK_F14] = IO::Keys::F14;
    keys[kVK_F15] = IO::Keys::F15;
    keys[kVK_F16] = IO::Keys::F16;
    keys[kVK_F17] = IO::Keys::F17;
    keys[kVK_F18] = IO::Keys::F18;
    keys[kVK_F19] = IO::Keys::F19;
    keys[kVK_F20] = IO::Keys::F20;

    keys[kVK_ANSI_Keypad0] = IO::Keys::KP0;
    keys[kVK_ANSI_Keypad1] = IO::Keys::KP1;
    keys[kVK_ANSI_Keypad2] = IO::Keys::KP2;
    keys[kVK_ANSI_Keypad3] = IO::Keys::KP3;
    keys[kVK_ANSI_Keypad4] = IO::Keys::KP4;
    keys[kVK_ANSI_Keypad5] = IO::Keys::KP5;
    keys[kVK_ANSI_Keypad6] = IO::Keys::KP6;
    keys[kVK_ANSI_Keypad7] = IO::Keys::KP7;
    keys[kVK_ANSI_Keypad8] = IO::Keys::KP8;
    keys[kVK_ANSI_Keypad9] = IO::Keys::KP9;
    keys[kVK_ANSI_KeypadDecimal] = IO::Keys::KPDecimal;
    keys[kVK_ANSI_KeypadDivide] = IO::Keys::KPDivide;
    keys[kVK_ANSI_KeypadMultiply] = IO::Keys::KPMultiply;
    keys[kVK_ANSI_KeypadMinus] = IO::Keys::KPSubtract;
    keys[kVK_ANSI_KeypadPlus] = IO::Keys::KPAdd;
    keys[kVK_ANSI_KeypadEnter] = IO::Keys::KPEnter;
    keys[kVK_ANSI_KeypadEquals] = IO::Keys::KPEqual;

    keys[kVK_Shift] = IO::Keys::LShift;
    keys[kVK_Control] = IO::Keys::LCtrl;
    keys[kVK_Option] = IO::Keys::LAlt;
    keys[kVK_Command] = IO::Keys::LSuper;
    keys[kVK_RightShift] = IO::Keys::RShift;
    keys[kVK_RightControl] = IO::Keys::RCtrl;
    keys[kVK_RightOption] = IO::Keys::RAlt;
    keys[0x36] = IO::Keys::RSuper;

    return keys;
}();

}

IO::Keys from_cocoa_key(uint16_t key_code) noexcept
{
    return key_code < key_map.size() ? key_map[key_code] : IO::Keys::Unknown;
}

int to_cocoa_key(IO::Keys key) noexcept
{
    if (key == IO::Keys::Unknown)
        return -1;

    for (std::size_t key_code = 0; key_code < key_map.size(); ++key_code) {
        if (key_map[key_code] == key)
            return static_cast<int>(key_code);
    }

    return -1;
}

bool is_valid_cocoa_key(uint16_t key_code) noexcept
{
    return from_cocoa_key(key_code) != IO::Keys::Unknown;
}

}

#endif
