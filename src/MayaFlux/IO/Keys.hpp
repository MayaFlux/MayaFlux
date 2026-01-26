#pragma once

namespace MayaFlux::IO {

enum class Keys : int16_t {
    // Printable ASCII keys (single char, so we use the actual character names)
    Space = 32,
    Apostrophe = 39, // '
    Comma = 44, // ,
    Minus = 45, // -
    Period = 46, // .
    Slash = 47, // /

    N0 = 48,
    N1,
    N2,
    N3,
    N4,
    N5,
    N6,
    N7,
    N8,
    N9,

    Semicolon = 59, // ;
    Equal = 61, // =

    A = 65,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
    I,
    J,
    K,
    L,
    M,
    N,
    O,
    P,
    Q,
    R,
    S,
    T,
    U,
    V,
    W,
    X,
    Y,
    Z,

    LeftBracket = 91, // [
    Backslash = 92, // \

    RightBracket = 93, // ]
    GraveAccent = 96, // `

    // Function keys
    Escape = 256,
    Enter = 257,
    Tab = 258,
    Backspace = 259,
    Insert = 260,
    Delete = 261,

    Right = 262,
    Left = 263,
    Down = 264,
    Up = 265,

    PageUp = 266,
    PageDown = 267,
    Home = 268,
    End = 269,

    CapsLock = 280,
    ScrollLock = 281,
    NumLock = 282,
    PrintScreen = 283,
    Pause = 284,

    F1 = 290,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,
    F13,
    F14,
    F15,
    F16,
    F17,
    F18,
    F19,
    F20,
    F21,
    F22,
    F23,
    F24,
    F25,

    KP0 = 320,
    KP1,
    KP2,
    KP3,
    KP4,
    KP5,
    KP6,
    KP7,
    KP8,
    KP9,

    KPDecimal = 330,
    KPDivide = 331,
    KPMultiply = 332,
    KPSubtract = 333,
    KPAdd = 334,
    KPEnter = 335,
    KPEqual = 336,

    LShift = 340,
    LCtrl = 341,
    LAlt = 342,
    LSuper = 343,
    RShift = 344,
    RCtrl = 345,
    RAlt = 346,
    RSuper = 347,

    Menu = 348,

    Unknown = -1
};

/**
 * @brief Converts a character to the corresponding Keys enum value.
 * @param c The character to convert.
 * @return An optional Keys value if the character matches a key, std::nullopt otherwise.
 */
std::optional<Keys> from_char(char c) noexcept;

/**
 * @brief Converts a string to the corresponding Keys enum value.
 * @param str The string to convert.
 * @return An optional Keys value if the string matches a key, std::nullopt otherwise.
 */
std::optional<Keys> from_string(std::string_view str) noexcept;

/**
 * @brief Converts a Keys enum value to its string representation.
 * @param key The key to convert.
 * @return The string view representing the key.
 */
std::string_view to_string(Keys key) noexcept;

/**
 * @brief Converts a Keys enum value to its lowercase string representation.
 * @param key The key to convert.
 * @return The lowercase string representing the key.
 */
std::string to_lowercase_string(Keys key) noexcept;

/**
 * @brief Checks if a key is a printable character.
 * @param key The key to check.
 * @return True if the key is printable, false otherwise.
 */
bool is_printable(Keys key) noexcept;

/**
 * @brief Checks if a key is a modifier key (e.g., Shift, Ctrl, Alt).
 * @param key The key to check.
 * @return True if the key is a modifier, false otherwise.
 */
bool is_modifier(Keys key) noexcept;

/**
 * @brief Checks if a key is a function key (e.g., F1-F25).
 * @param key The key to check.
 * @return True if the key is a function key, false otherwise.
 */
bool is_function_key(Keys key) noexcept;

/**
 * @brief Checks if a key is a keypad key.
 * @param key The key to check.
 * @return True if the key is a keypad key, false otherwise.
 */
bool is_keypad_key(Keys key) noexcept;

/**
 * @brief Returns a vector of all key names in lowercase.
 * @return A vector of lowercase key names.
 */
std::vector<std::string> all_key_names_lowercase() noexcept;

/**
 * @brief Returns a container of all key names.
 * @return A container of all key names.
 */
auto all_key_names() noexcept;

/**
 * @brief Returns a container of all Keys enum values.
 * @return A container of all Keys values.
 */
auto all_keys() noexcept;

/**
 * @brief Returns the total number of keys.
 * @return The number of keys.
 */
size_t key_count() noexcept;

} // namespace MayaFlux::IO
