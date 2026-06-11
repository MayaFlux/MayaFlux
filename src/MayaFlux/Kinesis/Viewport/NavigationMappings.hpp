#pragma once

#include "MayaFlux/IO/Keys.hpp"

namespace MayaFlux::Kinesis {

/**
 * @struct FlyKeyMap
 * @brief Key assignments for the Fly navigation preset.
 *
 * Movement keys are required; all six default to the canonical WASD/QE layout.
 * Ortho-snap slots are optional: a std::nullopt entry is silently skipped,
 * which is the correct default for environments without a numeric keypad.
 */
struct FlyKeyMap {
    IO::Keys forward { IO::Keys::W };
    IO::Keys back { IO::Keys::S };
    IO::Keys left { IO::Keys::A };
    IO::Keys right { IO::Keys::D };
    IO::Keys down { IO::Keys::Q };
    IO::Keys up { IO::Keys::E };

    std::optional<IO::Keys> ortho_front { IO::Keys::KP1 }; ///< Snap to front (+Z) view
    std::optional<IO::Keys> ortho_right { IO::Keys::KP3 }; ///< Snap to right (+X) view
    std::optional<IO::Keys> ortho_top { IO::Keys::KP7 }; ///< Snap to top (+Y) view
    std::optional<IO::Keys> ortho_flip { IO::Keys::KP9 }; ///< Flip to opposite view
};

/**
 * @struct OrbitKeyMap
 * @brief Key assignments for the Orbit navigation preset.
 *
 * Orbit has no keyboard translate axes. The only configurable key is the
 * pan modifier: when held during MMB drag, the focal point pans rather
 * than the camera rotating. Ortho snap slots follow the same optional
 * convention as FlyKeyMap.
 */
struct OrbitKeyMap {
    IO::Keys pan_modifier { IO::Keys::LShift }; ///< Held during MMB drag to pan instead of rotate

    std::optional<IO::Keys> ortho_front { IO::Keys::KP1 };
    std::optional<IO::Keys> ortho_right { IO::Keys::KP3 };
    std::optional<IO::Keys> ortho_top { IO::Keys::KP7 };
    std::optional<IO::Keys> ortho_flip { IO::Keys::KP9 };
};

/**
 * @struct PanZoom2DKeyMap
 * @brief Input assignments for the PanZoom2D navigation preset.
 *
 * Only the drag button is configurable. There are no keyboard axes or
 * ortho snaps in a 2D orthographic controller.
 */
struct PanZoom2DKeyMap {
    IO::MouseButtons drag_button { IO::MouseButtons::Middle }; ///< Button held to pan
};

} // namespace MayaFlux::Kinesis
