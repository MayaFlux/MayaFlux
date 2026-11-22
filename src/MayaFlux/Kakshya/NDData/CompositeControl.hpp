#pragma once

#include "CompositeDescribe.hpp"

#include "MayaFlux/Transitive/Facet/ControlSnapshot.hpp"
#include "MayaFlux/Transitive/Facet/ControlValueText.hpp"

namespace MayaFlux::Kakshya {

namespace detail {

    template <typename Slot>
    [[nodiscard]] bool requests_control_clear(const Slot& slot) noexcept
    {
        if constexpr (requires { { slot.clear_requested() } -> std::convertible_to<bool>; }) {
            return slot.clear_requested();
        } else {
            return false;
        }
    }

}

/**
 * @brief A numeric control slot as one dynamic Composite field.
 *
 * Gives every controls struct that is described, whatever host it belongs to, a
 * Composite row, so it can be written to any Composite writer and read back as a
 * patch. The three states of a slot are kept apart:
 * - an empty slot is an absent field, which an overlay leaves alone;
 * - an unset slot, for a slot type that offers `T::unset()`, is a present NULL,
 *   which an overlay turns into an unset slot;
 * - a value is a REAL for a number, a bool or an enum, and TEXT of whitespace
 *   separated numbers for a vector, a quaternion or a box.
 *
 * Reading also accepts an INTEGER, and a NULL for a slot type that has no
 * `T::unset()` leaves the member as it is. A BLOB or text that is not numbers is
 * refused.
 */
template <typename T>
    requires NumericControlSlot<T>
struct CompositeFieldCodec<T> {
    static bool add(CompositeLayout& layout, std::string name)
    {
        return layout.add_dynamic_field(std::move(name));
    }

    static bool write(CompositeInsertion& insertion, size_t row,
        std::string_view name, const T& slot, std::string& error)
    {
        bool written {};
        if (slot.has_value()) {
            written = slot.width() == 1
                ? insertion.set(row, name, slot.value())
                : insertion.set_text(row, name, Reflect::control_text(slot));
        } else if (detail::requests_control_clear(slot)) {
            written = insertion.set_null(row, name);
        } else {
            written = insertion.clear(row, name);
        }
        return written || detail::described_field_error(name, error);
    }

    static bool read(const Composite& source, std::string_view name,
        T& value, std::string& error)
    {
        const auto cell = source.value(name);
        if (!cell) {
            return detail::described_field_error(name, error);
        }

        if (std::holds_alternative<std::monostate>(*cell)) {
            if constexpr (requires { { T::unset() } -> std::same_as<T>; }) {
                value.assign(T::unset());
            }
            return true;
        }
        if (const auto* real = std::get_if<double>(&*cell)) {
            value.assign(T(*real));
            return true;
        }
        if (const auto* integer = std::get_if<int64_t>(&*cell)) {
            value.assign(T(static_cast<double>(*integer)));
            return true;
        }
        if (const auto* text = std::get_if<std::string_view>(&*cell)) {
            Reflect::TextError reason = Reflect::TextError::Invalid;
            return Reflect::TextCodec<T>::decode(*text, value, reason)
                || detail::described_field_error(name, error);
        }
        return detail::described_field_error(name, error);
    }
};

/** @brief A text control slot as a std::string field: empty is absent, a value is its text. */
template <>
struct CompositeFieldCodec<ControlSnapshot<std::string>> {
    static bool add(CompositeLayout& layout, std::string name)
    {
        return layout.add_field<std::string>(std::move(name));
    }

    static bool write(CompositeInsertion& insertion, size_t row,
        std::string_view name, const ControlSnapshot<std::string>& slot, std::string& error)
    {
        bool written {};
        const auto held = slot.value();
        if (slot.has_value() && held) {
            written = insertion.set_text(row, name, *held);
        } else {
            written = insertion.clear(row, name);
        }
        return written || detail::described_field_error(name, error);
    }

    static bool read(const Composite& source, std::string_view name,
        ControlSnapshot<std::string>& value, std::string& error)
    {
        const auto text = source.text(name);
        if (!text) {
            return detail::described_field_error(name, error);
        }
        value.set(std::string(*text));
        return true;
    }
};

}
