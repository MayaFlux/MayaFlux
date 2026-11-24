#pragma once

#include "MayaFlux/Transitive/Facet/ControlValue.hpp"

namespace MayaFlux::Nexus {

/**
 * @class Quantity
 * @brief One member of a Nexus controls struct: empty, a value, or an unset value.
 *
 * A Quantity converts implicitly from whatever ControlValue holds: a number, a bool,
 * an enum, a float vector, a quaternion or a box. An empty one means "leave this
 * alone" when a controls struct is merged. Quantity::unset() means "make this empty",
 * so a member that is optional, such as a position, can be taken away as well as set.
 * It has no node or network source.
 *
 * A value may be set from any thread. The owner reads it with has_value() and
 * value<T>() as often as it likes, and with pending<T>() when it wants a change once.
 */
class Quantity : public ControlValue {
public:
    Quantity() = default;

    template <typename T>
        requires is_value<T>
    Quantity(T value) noexcept
        : ControlValue(value)
    {
    }

    Quantity(const Quantity&) = default;
    Quantity(Quantity&&) noexcept = default;
    Quantity& operator=(const Quantity&) = default;
    Quantity& operator=(Quantity&&) noexcept = default;
    ~Quantity() = default;

    /** @brief A Quantity that empties the member it is merged into. */
    [[nodiscard]] static Quantity unset() noexcept
    {
        Quantity quantity;
        quantity.mark_cleared();
        return quantity;
    }
};

}
