#pragma once

#include "MayaFlux/IO/Keys.hpp"

namespace MayaFlux::Core {

/**
 * @brief Convert a GLFW key code to MayaFlux Keys enum.
 *
 * GLFW key codes directly map to our Keys enum values (by design),
 * so this is a straightforward type-safe cast.
 *
 * @param glfw_key GLFW key code (from GLFW_KEY_* constants)
 * @return The corresponding IO::Keys value
 *
 * @note Unknown/unmapped GLFW keys are converted to Keys::Unknown
 */
IO::Keys from_glfw_key(int glfw_key) noexcept;

/**
 * @brief Convert a MayaFlux Keys enum to GLFW key code.
 *
 * Reverse mapping from our Keys enum back to GLFW key codes.
 * Used when you need to interface with GLFW or serialize key bindings.
 *
 * @param key The IO::Keys value to convert
 * @return The corresponding GLFW key code
 */
int to_glfw_key(IO::Keys key) noexcept;

/**
 * @brief Check if a GLFW key code is valid/supported.
 *
 * @param glfw_key GLFW key code to validate
 * @return True if the key is a recognized MayaFlux key, false otherwise
 */
bool is_valid_glfw_key(int glfw_key) noexcept;

} // namespace MayaFlux::Core
