#pragma once

#include "MayaFlux/IO/Keys.hpp"

namespace MayaFlux::Core {

/**
 * @class ModifierKeyState
 * @brief Tracks which of the eight left and right modifier keys are down.
 *
 * For backends whose native events do not carry a modifier mask. held() folds
 * the two sides of Shift, Ctrl, Alt and Super into IO::Modifiers bits. Lock
 * state is not tracked here.
 */
class ModifierKeyState {
public:
    /**
     * @brief Record a key going down or up. Keys that are not modifiers are ignored.
     */
    void set(IO::Keys key, bool down) noexcept
    {
        const int index = static_cast<int>(key) - static_cast<int>(IO::Keys::LShift);
        if (index < 0 || index >= 8)
            return;

        const auto bit = static_cast<uint8_t>(1U << index);
        m_down = down ? static_cast<uint8_t>(m_down | bit) : static_cast<uint8_t>(m_down & ~bit);
    }

    void clear() noexcept { m_down = 0; }

    /**
     * @brief Shift, Ctrl, Alt and Super bits for the keys currently down.
     */
    [[nodiscard]] IO::Modifiers held() const noexcept
    {
        return static_cast<IO::Modifiers>((m_down | (m_down >> 4)) & 0x0F);
    }

private:
    static_assert(static_cast<int>(IO::Keys::LCtrl) == static_cast<int>(IO::Keys::LShift) + 1
            && static_cast<int>(IO::Keys::LAlt) == static_cast<int>(IO::Keys::LShift) + 2
            && static_cast<int>(IO::Keys::LSuper) == static_cast<int>(IO::Keys::LShift) + 3
            && static_cast<int>(IO::Keys::RShift) == static_cast<int>(IO::Keys::LShift) + 4
            && static_cast<int>(IO::Keys::RSuper) == static_cast<int>(IO::Keys::LShift) + 7,
        "held() relies on the modifier key order in IO::Keys");

    static_assert(static_cast<int>(IO::Modifiers::Shift) == 1
            && static_cast<int>(IO::Modifiers::Ctrl) == 2
            && static_cast<int>(IO::Modifiers::Alt) == 4
            && static_cast<int>(IO::Modifiers::Super) == 8,
        "held() relies on the IO::Modifiers bit order");

    uint8_t m_down { 0 };
};

} // namespace MayaFlux::Core
