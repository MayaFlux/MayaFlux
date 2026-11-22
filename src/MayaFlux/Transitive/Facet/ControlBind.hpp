#pragma once

namespace MayaFlux {

namespace detail {
    template <auto Member>
    struct ControlMemberOf;

    template <typename Controls, typename Slot, Slot Controls::* Member>
    struct ControlMemberOf<Member> {
        using controls_type = Controls;
        using slot_type = Slot;
    };
}

/**
 * @brief A function that sets one member of a host's controls struct from a value.
 * @tparam Member Pointer to the member, such as `&PhysicsControls::drag`.
 * @tparam T The type of the value the function takes, which the member's slot must be made from.
 * @param host The concrete type that declares the controls, retained by the function.
 *
 * Each call builds an empty controls struct, sets that one member and hands it to
 * `host->set()`, so every other member is left alone and the value is applied at
 * the host's next block. The function fits any callable that takes `const T&`, such
 * as the target of an influence.
 */
template <auto Member, typename T, typename Host>
[[nodiscard]] std::function<void(const T&)> control_writer(std::shared_ptr<Host> host)
{
    using Controls = typename detail::ControlMemberOf<Member>::controls_type;
    using Slot = typename detail::ControlMemberOf<Member>::slot_type;

    return [host = std::move(host)](const T& value) {
        Controls incoming;
        incoming.*Member = Slot(value);
        host->set(incoming);
    };
}

/**
 * @brief A function that reads one member of a host's controls struct as a type.
 * @tparam Member Pointer to the member, such as `&PhysicsControls::drag`.
 * @tparam T The type to read it as. For a numeric slot it must be the type it was set with, and
 *         for a ControlSnapshot member, such as `&CourseControls::range`, the type it holds.
 * @param host The concrete type that declares the controls, retained by the function.
 *
 * Each call copies the whole struct through `host->get()`, so the value is what the
 * host holds now, and answers nothing while the member is empty.
 */
template <auto Member, typename T, typename Host>
[[nodiscard]] std::function<std::optional<T>()> control_reader(std::shared_ptr<Host> host)
{
    return [host = std::move(host)]() -> std::optional<T> {
        const auto controls = host->get();
        const auto& slot = controls.*Member;
        if (!slot.has_value()) {
            return std::nullopt;
        }
        if constexpr (requires { slot.template value<T>(); }) {
            return slot.template value<T>();
        } else {
            const auto held = slot.value();
            if (!held) {
                return std::nullopt;
            }
            return T(*held);
        }
    };
}

}
