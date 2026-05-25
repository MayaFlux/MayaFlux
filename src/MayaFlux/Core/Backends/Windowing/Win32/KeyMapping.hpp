#pragma once

#include "MayaFlux/IO/Keys.hpp"

#ifdef MAYAFLUX_PLATFORM_WINDOWS

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace MayaFlux::Core {

/**
 * @brief Convert a Win32 virtual key code to IO::Keys.
 *
 * Printable letters (VK_A–VK_Z == 0x41–0x5A) and digits
 * (VK_0–VK_9 == 0x30–0x39) share the same numeric values as the Keys
 * enum and are cast directly. All other keys are dispatched through a
 * lookup table.
 *
 * @param vk Win32 virtual key code (WPARAM from WM_KEYDOWN/WM_KEYUP).
 * @return Corresponding IO::Keys value, or Keys::Unknown if unmapped.
 */
MAYAFLUX_API IO::Keys from_win32_key(WPARAM vk) noexcept;

/**
 * @brief Convert IO::Keys to the corresponding Win32 virtual key code.
 *
 * Returns 0 for Keys::Unknown or any key that has no Win32 equivalent.
 *
 * @param key IO::Keys value to convert.
 * @return Win32 virtual key code, or 0 if unmapped.
 */
MAYAFLUX_API int to_win32_key(IO::Keys key) noexcept;

/**
 * @brief Convert a Win32 scancode to IO::Keys for numpad keys whose VK codes
 *        invert with NumLock state.
 *
 * Only handles scancodes 0x47–0x53 without the extended-key bit. Returns
 * Keys::Unknown for all other inputs; caller falls back to from_win32_key().
 *
 * @param scancode Raw scancode from HIWORD(lParam) & 0x1FF (bit 8 = extended).
 * @return Corresponding IO::Keys value, or Keys::Unknown if not handled.
 */
MAYAFLUX_API IO::Keys from_win32_scancode(uint32_t scancode) noexcept;

/**
 * @brief Check whether a Win32 virtual key code maps to a known IO::Keys value.
 *
 * @param vk Win32 virtual key code.
 * @return True if the key is recognised, false otherwise.
 */
MAYAFLUX_API bool is_valid_win32_key(WPARAM vk) noexcept;

} // namespace MayaFlux::Core

#endif // MAYAFLUX_PLATFORM_WINDOWS
