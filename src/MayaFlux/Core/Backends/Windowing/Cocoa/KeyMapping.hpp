#pragma once

#ifdef MAYAFLUX_PLATFORM_MACOS

#include "MayaFlux/IO/Keys.hpp"

#include <cstdint>

namespace MayaFlux::Core {

/**
 * @brief Convert a macOS virtual key code to a layout-independent key.
 * @param key_code Key code from NSEvent.keyCode.
 * @return The matching key, or IO::Keys::Unknown when unmapped.
 */
MAYAFLUX_API IO::Keys from_cocoa_key(uint16_t key_code) noexcept;

/**
 * @brief Convert a key to a macOS virtual key code.
 * @param key Key to convert.
 * @return The matching key code, or -1 when unmapped.
 */
MAYAFLUX_API int to_cocoa_key(IO::Keys key) noexcept;

/**
 * @brief Check whether a macOS virtual key code has a matching key.
 * @param key_code Key code to check.
 * @return True when the key code is mapped.
 */
MAYAFLUX_API bool is_valid_cocoa_key(uint16_t key_code) noexcept;

}

#endif
