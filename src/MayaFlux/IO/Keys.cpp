#include "Keys.hpp"
#include "MayaFlux/EnumUtils.hpp"

namespace MayaFlux::IO {

namespace {
    constexpr std::array<std::pair<char, Keys>, 10> char_to_keys = { {
        { ' ', Keys::Space },
        { '\'', Keys::Apostrophe },
        { ',', Keys::Comma },
        { '-', Keys::Minus },
        { '.', Keys::Period },
        { '/', Keys::Slash },
        { ';', Keys::Semicolon },
        { '=', Keys::Equal },
        { '[', Keys::LeftBracket },
        { ']', Keys::RightBracket },
    } };
}

std::optional<Keys> from_char(char c) noexcept
{
    for (const auto& [ch, key] : char_to_keys) {
        if (ch == c)
            return key;
    }

    if (c >= '0' && c <= '9') {
        return static_cast<Keys>(static_cast<int>(c));
    }
    if (c >= 'A' && c <= 'Z') {
        return static_cast<Keys>(static_cast<int>(c));
    }
    if (c >= 'a' && c <= 'z') {
        return static_cast<Keys>(static_cast<int>(c) - 32);
    }

    return std::nullopt;
}

std::optional<Keys> from_string(std::string_view str) noexcept
{
    if (str.empty()) {
        return std::nullopt;
    }

    if (str.length() == 1) {
        return from_char(str[0]);
    }

    return Utils::string_to_enum_case_insensitive<Keys>(str);
}

std::string_view to_string(Keys key) noexcept
{
    return Utils::enum_to_string(key);
}

std::string to_lowercase_string(Keys key) noexcept
{
    return Utils::enum_to_lowercase_string(key);
}

bool is_printable(Keys key) noexcept
{
    int k = static_cast<int>(key);
    return k >= 32 && k <= 126; // Standard printable ASCII range
}

bool is_modifier(Keys key) noexcept
{
    return key == Keys::LShift || key == Keys::RShift || key == Keys::LCtrl || key == Keys::RCtrl || key == Keys::LAlt || key == Keys::RAlt || key == Keys::LSuper || key == Keys::RSuper;
}

bool is_function_key(Keys key) noexcept
{
    int k = static_cast<int>(key);
    return k >= 290 && k <= 314; // F1-F25
}

bool is_keypad_key(Keys key) noexcept
{
    int k = static_cast<int>(key);
    return k >= 320 && k <= 336; // KP0-KPEqual
}

std::vector<std::string> all_key_names_lowercase() noexcept
{
    return Utils::get_enum_names_lowercase<Keys>();
}

auto all_key_names() noexcept
{
    return Utils::get_enum_names<Keys>();
}

auto all_keys() noexcept
{
    return Utils::get_enum_values<Keys>();
}

size_t key_count() noexcept
{
    return Utils::enum_count<Keys>();
}

} // namespace MayaFlux::IO
