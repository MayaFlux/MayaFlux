#pragma once

#include "Control.hpp"

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

namespace MayaFlux::Nodes::Network {

/**
 * @class ControlStore
 * @brief Owns one described struct of Control members for a NodeNetwork or a NetworkOperator.
 *
 * A host holds one as a member and forwards to it. It is not a base class.
 * declare() creates the struct and keeps it alive for the host's lifetime,
 * merge() applies the members an incoming struct holds, snapshot() copies the
 * struct for get(), and refresh() reads node sources once per block. A member
 * named `scale` is mirrored to a value the host passes in, so a network can map
 * it to its output scale and an operator, which has none, passes nothing.
 * Every member of the struct must be a Control.
 */
class MAYAFLUX_API ControlStore {
public:
    ControlStore() = default;
    ~ControlStore() = default;

    ControlStore(const ControlStore&) = delete;
    ControlStore& operator=(const ControlStore&) = delete;
    ControlStore(ControlStore&&) = delete;
    ControlStore& operator=(ControlStore&&) = delete;

    /** @brief Creates the struct and returns it, for the host to hold by reference. */
    template <typename C>
    [[nodiscard]] C& declare()
    {
        auto storage = std::make_shared<C>();
        C& controls = *storage;

        m_storage = std::move(storage);
        m_tag = tag<C>();
        m_slots.clear();
        m_scale = nullptr;

        std::apply(
            [&](const auto&... member) {
                static_assert((std::same_as<typename std::remove_cvref_t<decltype(member)>::value_type, Control> && ...),
                    "every member of a controls struct must be a Control");
                (add(member.name, member.get(controls)), ...);
            },
            Reflect::Generated::Description<C>::members());

        return controls;
    }

    /** @brief Applies every member of @p incoming that holds a value. */
    template <typename C>
    void merge(const C& incoming)
    {
        if (m_tag != tag<C>()) {
            return;
        }

        auto& live = *static_cast<C*>(m_storage.get());
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

    /**
     * @brief A copy of the struct.
     * @param scale The value a member named `scale` really holds, or null.
     */
    template <typename C>
    [[nodiscard]] C snapshot(const double* scale = nullptr) const
    {
        C current = m_tag == tag<C>()
            ? *static_cast<const C*>(m_storage.get())
            : C {};
        if (scale) {
            std::apply(
                [&](const auto&... member) {
                    ((member.name == "scale" ? member.get(current).observe(*scale) : void()), ...);
                },
                Reflect::Generated::Description<C>::members());
        }
        return current;
    }

    /**
     * @brief Reads node sources into their controls.
     * @param scale Where a member named `scale` is written when it is due, or null.
     */
    void refresh(double* scale = nullptr)
    {
        for (Control* control : m_slots) {
            control->refresh();
        }

        if (scale && m_scale) {
            if (const auto due = m_scale->pending()) {
                *scale = std::max(0.0, *due);
            }
        }
    }

private:
    template <typename C>
    [[nodiscard]] static const void* tag() noexcept
    {
        static constexpr char value = 0;
        return &value;
    }

    void add(std::string_view name, Control& control)
    {
        m_slots.push_back(&control);
        if (name == "scale") {
            m_scale = &control;
        }
    }

    std::shared_ptr<void> m_storage;
    const void* m_tag = nullptr;
    std::vector<Control*> m_slots;
    Control* m_scale = nullptr;
};

}
