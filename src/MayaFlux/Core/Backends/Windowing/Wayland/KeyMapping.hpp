#pragma once

#ifdef MAYAFLUX_PLATFORM_LINUX

#include "MayaFlux/IO/Keys.hpp"

#include <xkbcommon/xkbcommon.h>

namespace MayaFlux::Core {

/**
 * @brief Convert an xkbcommon keysym to IO::Keys.
 *
 * Printable ASCII keysyms (XKB_KEY_space through XKB_KEY_asciitilde) share
 * numeric values with the Keys enum and are cast directly.  All other
 * keysyms are dispatched through a lookup table covering navigation, editing,
 * lock, function, keypad, and modifier keys.
 *
 * @param sym xkb_keysym_t from xkb_state_key_get_one_sym().
 * @return Corresponding IO::Keys value, or Keys::Unknown if unmapped.
 */
MAYAFLUX_API IO::Keys from_xkb_keysym(xkb_keysym_t sym) noexcept;

/**
 * @brief Convert IO::Keys to an xkbcommon keysym.
 *
 * Returns a layout-independent keysym for the given key.  This is only
 * meaningful for keys with stable Unicode/X11 keysym assignments (printable
 * ASCII, function keys, navigation).  Layout-sensitive printable keys will
 * return the QWERTY/US keysym regardless of the active layout.
 *
 * @param key IO::Keys value to convert.
 * @return Corresponding xkb_keysym_t, or XKB_KEY_NoSymbol if unmapped.
 */
MAYAFLUX_API xkb_keysym_t to_xkb_keysym(IO::Keys key) noexcept;

/**
 * @brief Check whether an xkbcommon keysym maps to a known IO::Keys value.
 *
 * @param sym xkb_keysym_t to validate.
 * @return True if sym is not XKB_KEY_NoSymbol and has a known mapping.
 */
MAYAFLUX_API bool is_valid_xkb_keysym(xkb_keysym_t sym) noexcept;

} // namespace MayaFlux::Core

#endif // MAYAFLUX_PLATFORM_LINUX
