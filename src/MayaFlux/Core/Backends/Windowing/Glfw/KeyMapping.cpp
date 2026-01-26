#include "KeyMapping.hpp"

namespace MayaFlux::Core {

IO::Keys from_glfw_key(int glfw_key) noexcept
{
    if (glfw_key < -1 || glfw_key > 348) {
        return IO::Keys::Unknown;
    }

    auto result = static_cast<IO::Keys>(glfw_key);

    if (!is_valid_glfw_key(glfw_key)) {
        return IO::Keys::Unknown;
    }

    return result;
}

int to_glfw_key(IO::Keys key) noexcept
{
    return static_cast<int>(key);
}

bool is_valid_glfw_key(int glfw_key) noexcept
{
    // GLFW only generates specific key codes;
    // Supported ranges based on Keys enum:
    // - 32-126: Printable ASCII
    // - 256-269: Navigation/editing
    // - 280-284: Lock keys
    // - 290-314: Function keys (F1-F25)
    // - 320-336: Keypad
    // - 340-348: Modifiers and menu

    if (glfw_key == -1) {
        return false;
    }

    if (glfw_key >= 32 && glfw_key <= 126) {
        return true; // Printable ASCII range
    }

    if (glfw_key >= 256 && glfw_key <= 269) {
        return true; // Escape through End
    }

    if (glfw_key >= 280 && glfw_key <= 284) {
        return true; // Lock keys
    }

    if (glfw_key >= 290 && glfw_key <= 314) {
        return true; // F1-F25
    }

    if (glfw_key >= 320 && glfw_key <= 336) {
        return true; // Keypad
    }

    if (glfw_key >= 340 && glfw_key <= 348) {
        return true; // Modifiers and Menu
    }

    return false;
}

} // namespace MayaFlux::Core
