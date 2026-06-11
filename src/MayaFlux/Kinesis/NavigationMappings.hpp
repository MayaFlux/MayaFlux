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

} // namespace MayaFlux::Kinesis
