#pragma once

#include "MayaFlux/Nodes/Node.hpp"
#include "MayaFlux/Transitive/Facet/ControlValue.hpp"
#include "MayaFlux/Transitive/Facet/ControlValueStore.hpp"

namespace MayaFlux::Nodes::Network {

class NodeNetwork;

/**
 * @class Control
 * @brief One named control of a network or operator: empty, a value, or a mapped source.
 *
 * A Control converts implicitly from a number, a bool, an enum, a glm::vec2,
 * glm::vec3 or glm::vec4, from a box (a struct of exactly a glm::vec3 `min` and
 * a glm::vec3 `max`, such as Kinesis::SamplerBounds or Kinesis::AABB3D), from a node (BROADCAST) and from a network
 * (ONE_TO_ONE), so a controls struct reads as designated initializers:
 * `net->set({ .decay = 0.6, .amplitude = bell })` or
 * `op->set({ .gravity = glm::vec3(0, -9.8, 0) })`. An empty Control means
 * "leave this alone". Setting a value replaces any source held before it.
 * The owner reads it back as the type it declared: `pending()` and `value()`
 * give a double, and `pending<glm::vec3>()`, `pending<BoundsMode>()` and so on
 * give that type. A read of another type than was set finds nothing.
 *
 * A value may be set from any thread while a block renders. Numbers are atomic,
 * and a vector is written and read under a sequence lock, so it is never seen
 * half written. A value is never lost to a race, only possibly applied twice
 * with the same content. A Control holds one pending value, so a later value
 * replaces one that no block has applied yet. Integers are carried exactly up to 2^53. Attaching, replacing
 * or removing a node or network source belongs outside the block, as with the
 * operator chain. A consumer calls settle() before it reads the value.
 */
class Control : public ControlValue {
public:
    Control() = default;

    template <typename T>
        requires is_value<T>
    Control(T value) noexcept
        : ControlValue(value)
    {
    }

    template <typename Source>
        requires std::derived_from<Source, Node>
    Control(std::shared_ptr<Source> source)
        : m_node(std::move(source))
    {
        set_blank();
    }

    template <typename Source>
        requires std::derived_from<Source, NodeNetwork>
    Control(std::shared_ptr<Source> source)
        : m_network(std::move(source))
    {
        set_blank();
    }

    Control(const Control&) = default;
    Control(Control&&) noexcept = default;
    Control& operator=(const Control&) = default;
    Control& operator=(Control&&) noexcept = default;
    ~Control() = default;

    /** @brief True when a node or a network feeds this control. */
    [[nodiscard]] bool sourced() const noexcept { return m_node || m_network; }

    /** @brief True when a network feeds one value per element. */
    [[nodiscard]] bool per_element() const noexcept { return static_cast<bool>(m_network); }

    /** @brief Node count of the source network, or 0 without one. */
    [[nodiscard]] size_t element_count() const;

    /** @brief Value for one element: the network's output there, else the scalar. */
    [[nodiscard]] std::optional<double> at(size_t index) const;

    /** @brief True when a consumer should apply this control now. */
    [[nodiscard]] bool due() const noexcept
    {
        return has_value() && (sourced() || changed());
    }

    /**
     * @brief The value to apply now as type T, or nullopt when nothing is due.
     *
     * A value is returned once per change, a node source every call. A network
     * source is per element and is never returned here; read it with at(). A
     * control set with another type than T returns nothing.
     */
    template <typename T = double>
        requires is_value<T>
    [[nodiscard]] std::optional<T> pending() noexcept
    {
        if (m_network) {
            return std::nullopt;
        }
        if (m_node) {
            if (!has_value() || width() != width_of<T>()) {
                return std::nullopt;
            }
            return value<T>();
        }
        return ControlValue::pending<T>();
    }

    /** @brief Reads the node source into value(). Networks call this once per block. */
    void refresh()
    {
        if (m_node) {
            set_scalar(m_node->get_last_output());
        }
    }

    /**
     * @brief Takes everything @p incoming holds, so the next due() is true.
     *
     * Between two values this is safe from any thread. When either side holds
     * a source it replaces the source pointers, which belongs outside the block.
     */
    void assign(const Control& incoming)
    {
        if (sourced() || incoming.sourced()) {
            m_node = incoming.m_node;
            m_network = incoming.m_network;
        }
        ControlValue::assign(incoming);
    }

    /** @brief Shows the value the owner really has, unless a source is feeding it. */
    template <typename T>
        requires is_value<T>
    void observe(T actual) noexcept
    {
        if (!sourced()) {
            ControlValue::observe(actual);
        }
    }

private:
    std::shared_ptr<Node> m_node;
    std::shared_ptr<NodeNetwork> m_network;
};

/**
 * @brief The store of Control members for a NodeNetwork or a NetworkOperator.
 *
 * See ControlValueStore. Every member of the declared struct must be a Control.
 */
using ControlStore = ControlValueStore<Control>;

}
