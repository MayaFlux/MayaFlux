#pragma once

#include "Control.hpp"

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

namespace MayaFlux::Nodes::Network {

/**
 * @class ControlSurface
 * @brief A described struct of Control members, owned and reachable by set and get.
 *
 * NodeNetwork and NetworkOperator derive from it. A derived class opts in with
 * `using Controls = X;`, where X is a struct of Control members declared with
 * MF_DESCRIBE, and holds the reference declare_controls() returns, taken in its
 * constructor's initializer list. It calls refresh_controls() once per block
 * and then reads its own struct.
 *
 * `set` merges only the members that hold a value, so `x->set({ .decay = lfo })`
 * changes only decay. A node source maps in BROADCAST and a network source in
 * ONE_TO_ONE. `get` returns a copy with the values the object really holds. A
 * number may be set from any thread while a block renders. Attaching or removing
 * a node or network source belongs outside the block.
 *
 * A derived class may offer a public `read_controls(Controls&) const` so get()
 * can report members whose value lives outside the struct. A member named
 * `scale` is mirrored to the target a network binds with bind_scale().
 * Designators follow the struct's declaration order.
 */
class ControlSurface {
public:
    ControlSurface(const ControlSurface&) = delete;
    ControlSurface& operator=(const ControlSurface&) = delete;

    /** @brief Applies every member of @p incoming that holds a value. */
    template <typename Self>
    void set(this Self& self, const typename Self::Controls& incoming)
    {
        static_cast<ControlSurface&>(self).merge_controls(incoming);
    }

    /** @brief The controls with the values this object holds now. */
    template <typename Self>
    [[nodiscard]] typename Self::Controls get(this const Self& self)
    {
        auto current = static_cast<const ControlSurface&>(self)
                           .template snapshot_controls<typename Self::Controls>();
        if constexpr (requires { self.read_controls(current); }) {
            self.read_controls(current);
        }
        return current;
    }

protected:
    ControlSurface() = default;
    ~ControlSurface() = default;

    /**
     * @brief Create the controls struct and keep it alive.
     * @return The struct, owned by this object, for the derived class to read.
     *
     * Call once from the constructor's initializer list and hold the result in
     * a reference member. Every member of the struct must be a Control.
     */
    template <typename C>
    [[nodiscard]] C& declare_controls()
    {
        auto storage = std::make_shared<C>();
        C& controls = *storage;

        m_controls_storage = std::move(storage);
        m_controls_tag = controls_tag<C>();
        m_control_slots.clear();
        m_scale_control = nullptr;

        std::apply(
            [&](const auto&... member) {
                static_assert((std::same_as<typename std::remove_cvref_t<decltype(member)>::value_type, Control> && ...),
                    "every member of a controls struct must be a Control");
                (register_control(member.name, member.get(controls)), ...);
            },
            Reflect::Generated::Description<C>::members());

        return controls;
    }

    /**
     * @brief Read node sources and mirror a member named `scale` to the bound target.
     *
     * Call once per block before reading the controls struct.
     */
    void refresh_controls()
    {
        for (Control* control : m_control_slots) {
            control->refresh();
        }

        if (m_scale_control && m_scale_target) {
            if (const auto scale = m_scale_control->pending()) {
                *m_scale_target = std::max(0.0, *scale);
            }
        }
    }

    /** @brief Names the value a member called `scale` is written to and read back from. */
    void bind_scale(double* target) noexcept { m_scale_target = target; }

private:
    template <typename C>
    [[nodiscard]] static const void* controls_tag() noexcept
    {
        static constexpr char tag = 0;
        return &tag;
    }

    void register_control(std::string_view name, Control& control)
    {
        m_control_slots.push_back(&control);
        if (name == "scale") {
            m_scale_control = &control;
        }
    }

    template <typename C>
    void merge_controls(const C& incoming)
    {
        if (m_controls_tag != controls_tag<C>()) {
            return;
        }

        auto& live = *static_cast<C*>(m_controls_storage.get());
        const auto merge_one = [&](const auto& member) {
            const Control& given = member.get(incoming);
            if (given.has_value()) {
                member.get(live).assign(given);
            }
        };
        std::apply(
            [&](const auto&... member) { (merge_one(member), ...); },
            Reflect::Generated::Description<C>::members());
    }

    template <typename C>
    [[nodiscard]] C snapshot_controls() const
    {
        C current = m_controls_tag == controls_tag<C>()
            ? *static_cast<const C*>(m_controls_storage.get())
            : C {};
        if (m_scale_target) {
            std::apply(
                [&](const auto&... member) {
                    ((member.name == "scale" ? member.get(current).observe(*m_scale_target) : void()), ...);
                },
                Reflect::Generated::Description<C>::members());
        }
        return current;
    }

    std::shared_ptr<void> m_controls_storage;
    const void* m_controls_tag = nullptr;
    std::vector<Control*> m_control_slots;
    Control* m_scale_control = nullptr;
    double* m_scale_target = nullptr;
};

}
