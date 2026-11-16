#pragma once

#include "MayaFlux/Nodes/Node.hpp"

namespace MayaFlux::Nodes::Network {

class NodeNetwork;

/**
 * @class Control
 * @brief One named control of a network: empty, a number, or a mapped source.
 *
 * A Control converts implicitly from a double, from a node (BROADCAST) and from
 * a network (ONE_TO_ONE), so a controls struct reads as designated initializers:
 * `net->set({ .decay = 0.6, .amplitude = bell })`. An empty Control means "leave
 * this alone". Setting a number replaces any source held before it.
 */
class Control {
public:
    Control() = default;

    Control(double value) noexcept
        : m_value(value)
        , m_set(true)
        , m_dirty(true)
    {
    }

    template <typename Source>
        requires std::derived_from<Source, Node>
    Control(std::shared_ptr<Source> source)
        : m_node(std::move(source))
        , m_set(true)
    {
    }

    template <typename Source>
        requires std::derived_from<Source, NodeNetwork>
    Control(std::shared_ptr<Source> source)
        : m_network(std::move(source))
        , m_set(true)
    {
    }

    /** @brief True once a number or a source was given. */
    [[nodiscard]] bool has_value() const noexcept { return m_set; }

    /** @brief True when a node or a network feeds this control. */
    [[nodiscard]] bool sourced() const noexcept { return m_node || m_network; }

    /** @brief True when a network feeds one value per element. */
    [[nodiscard]] bool per_element() const noexcept { return static_cast<bool>(m_network); }

    /** @brief Node count of the source network, or 0 without one. */
    [[nodiscard]] size_t element_count() const;

    /** @brief The number, or the node's last output as of the latest refresh. */
    [[nodiscard]] double value() const noexcept { return m_value; }

    operator double() const noexcept { return m_value; }

    /** @brief Value for one element: the network's output there, else the scalar. */
    [[nodiscard]] std::optional<double> at(size_t index) const;

    /** @brief True when a consumer should apply this control now. */
    [[nodiscard]] bool due() const noexcept { return m_set && (sourced() || m_dirty); }

    /** @brief Marks the current value as applied. */
    void settle() noexcept { m_dirty = false; }

    /**
     * @brief The scalar to apply now, or nullopt when nothing is due.
     *
     * A number is returned once per change, a node source every call. A network
     * source is per element and is never returned here; read it with at().
     */
    [[nodiscard]] std::optional<double> pending() noexcept
    {
        if (!due() || m_network) {
            return std::nullopt;
        }
        m_dirty = false;
        return m_value;
    }

    /** @brief Reads the node source into value(). Networks call this once per block. */
    void refresh()
    {
        if (m_node) {
            m_value = m_node->get_last_output();
        }
    }

    /** @brief Takes everything @p incoming holds, so the next due() is true. */
    void assign(const Control& incoming)
    {
        *this = incoming;
        m_dirty = true;
    }

    /** @brief Shows the value the network really has, unless a source is feeding it. */
    void observe(double actual) noexcept
    {
        if (!sourced()) {
            m_value = actual;
            m_set = true;
        }
    }

private:
    double m_value {};
    bool m_set {};
    bool m_dirty {};
    std::shared_ptr<Node> m_node;
    std::shared_ptr<NodeNetwork> m_network;
};

}
