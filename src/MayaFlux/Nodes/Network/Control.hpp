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
 *
 * A number may be set from any thread while a block renders: the value and its
 * flags are atomic, and a number is never lost, only possibly applied twice at
 * the same value. Attaching, replacing or removing a node or network source
 * belongs outside the block, as with the operator chain. A consumer calls
 * settle() before it reads the value.
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
        : m_set(true)
        , m_node(std::move(source))
    {
    }

    template <typename Source>
        requires std::derived_from<Source, NodeNetwork>
    Control(std::shared_ptr<Source> source)
        : m_set(true)
        , m_network(std::move(source))
    {
    }

    Control(const Control& other)
        : m_value(other.value())
        , m_set(other.has_value())
        , m_dirty(other.m_dirty.load(std::memory_order_acquire))
        , m_node(other.m_node)
        , m_network(other.m_network)
    {
    }

    Control& operator=(const Control& other)
    {
        if (this != &other) {
            m_node = other.m_node;
            m_network = other.m_network;
            store(other.value(), other.has_value(), other.m_dirty.load(std::memory_order_acquire));
        }
        return *this;
    }

    Control(Control&& other) noexcept
        : m_value(other.value())
        , m_set(other.has_value())
        , m_dirty(other.m_dirty.load(std::memory_order_acquire))
        , m_node(std::move(other.m_node))
        , m_network(std::move(other.m_network))
    {
    }

    Control& operator=(Control&& other) noexcept
    {
        if (this != &other) {
            m_node = std::move(other.m_node);
            m_network = std::move(other.m_network);
            store(other.value(), other.has_value(), other.m_dirty.load(std::memory_order_acquire));
        }
        return *this;
    }

    ~Control() = default;

    /** @brief True once a number or a source was given. */
    [[nodiscard]] bool has_value() const noexcept { return m_set.load(std::memory_order_acquire); }

    /** @brief True when a node or a network feeds this control. */
    [[nodiscard]] bool sourced() const noexcept { return m_node || m_network; }

    /** @brief True when a network feeds one value per element. */
    [[nodiscard]] bool per_element() const noexcept { return static_cast<bool>(m_network); }

    /** @brief Node count of the source network, or 0 without one. */
    [[nodiscard]] size_t element_count() const;

    /** @brief The number, or the node's last output as of the latest refresh. */
    [[nodiscard]] double value() const noexcept { return m_value.load(std::memory_order_relaxed); }

    operator double() const noexcept { return value(); }

    /** @brief Value for one element: the network's output there, else the scalar. */
    [[nodiscard]] std::optional<double> at(size_t index) const;

    /** @brief True when a consumer should apply this control now. */
    [[nodiscard]] bool due() const noexcept
    {
        return has_value() && (sourced() || m_dirty.load(std::memory_order_acquire));
    }

    /** @brief Marks the current value as applied. Call before reading value(). */
    void settle() noexcept { m_dirty.exchange(false, std::memory_order_acq_rel); }

    /**
     * @brief The scalar to apply now, or nullopt when nothing is due.
     *
     * A number is returned once per change, a node source every call. A network
     * source is per element and is never returned here; read it with at().
     */
    [[nodiscard]] std::optional<double> pending() noexcept
    {
        if (!has_value() || m_network) {
            return std::nullopt;
        }
        if (m_node) {
            return value();
        }
        if (!m_dirty.exchange(false, std::memory_order_acq_rel)) {
            return std::nullopt;
        }
        return value();
    }

    /** @brief Reads the node source into value(). Networks call this once per block. */
    void refresh()
    {
        if (m_node) {
            m_value.store(m_node->get_last_output(), std::memory_order_relaxed);
        }
    }

    /**
     * @brief Takes everything @p incoming holds, so the next due() is true.
     *
     * Between two numbers this is safe from any thread. When either side holds
     * a source it replaces the source pointers, which belongs outside the block.
     */
    void assign(const Control& incoming)
    {
        if (sourced() || incoming.sourced()) {
            m_node = incoming.m_node;
            m_network = incoming.m_network;
        }
        store(incoming.value(), true, true);
    }

    /** @brief Shows the value the network really has, unless a source is feeding it. */
    void observe(double actual) noexcept
    {
        if (!sourced()) {
            m_value.store(actual, std::memory_order_relaxed);
            m_set.store(true, std::memory_order_release);
        }
    }

private:
    void store(double value, bool set, bool dirty) noexcept
    {
        m_value.store(value, std::memory_order_relaxed);
        m_set.store(set, std::memory_order_release);
        m_dirty.store(dirty, std::memory_order_release);
    }

    std::atomic<double> m_value { 0.0 };
    std::atomic<bool> m_set { false };
    std::atomic<bool> m_dirty { false };
    std::shared_ptr<Node> m_node;
    std::shared_ptr<NodeNetwork> m_network;
};

}
