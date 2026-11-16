#pragma once

#include "MayaFlux/Nodes/Node.hpp"
#include "MayaFlux/Transitive/Memory/SeqLock.hpp"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace MayaFlux::Nodes::Network {

class NodeNetwork;

/**
 * @class Control
 * @brief One named control of a network or operator: empty, a value, or a mapped source.
 *
 * A Control converts implicitly from a number, a bool, an enum, a glm::vec2,
 * glm::vec3 or glm::vec4, from a node (BROADCAST) and from a network
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
class Control {
public:
    template <typename T>
    static constexpr bool is_vector = std::same_as<T, glm::vec2> || std::same_as<T, glm::vec3> || std::same_as<T, glm::vec4>;

    template <typename T>
    static constexpr bool is_value = std::is_arithmetic_v<T> || std::is_enum_v<T> || is_vector<T>;

    Control() = default;

    template <typename T>
        requires is_value<T>
    Control(T value) noexcept
    {
        store(to_lanes(value), width_of<T>(), true);
        m_dirty.store(true, std::memory_order_release);
    }

    template <typename Source>
        requires std::derived_from<Source, Node>
    Control(std::shared_ptr<Source> source)
        : m_node(std::move(source))
    {
        store({}, 1, true);
    }

    template <typename Source>
        requires std::derived_from<Source, NodeNetwork>
    Control(std::shared_ptr<Source> source)
        : m_network(std::move(source))
    {
        store({}, 1, true);
    }

    Control(const Control& other)
        : m_node(other.m_node)
        , m_network(other.m_network)
    {
        copy_state(other);
    }

    Control(Control&& other) noexcept
        : m_node(std::move(other.m_node))
        , m_network(std::move(other.m_network))
    {
        copy_state(other);
    }

    Control& operator=(const Control& other)
    {
        if (this != &other) {
            m_node = other.m_node;
            m_network = other.m_network;
            copy_state(other);
        }
        return *this;
    }

    Control& operator=(Control&& other) noexcept
    {
        if (this != &other) {
            m_node = std::move(other.m_node);
            m_network = std::move(other.m_network);
            copy_state(other);
        }
        return *this;
    }

    ~Control() = default;

    /** @brief True once a value or a source was given. */
    [[nodiscard]] bool has_value() const noexcept { return m_set.load(std::memory_order_acquire); }

    /** @brief Components held: 0 when empty, 1 for a number, flag or enum, 2 to 4 for a vector. */
    [[nodiscard]] size_t width() const noexcept { return m_width.load(std::memory_order_acquire); }

    /** @brief True when a node or a network feeds this control. */
    [[nodiscard]] bool sourced() const noexcept { return m_node || m_network; }

    /** @brief True when a network feeds one value per element. */
    [[nodiscard]] bool per_element() const noexcept { return static_cast<bool>(m_network); }

    /** @brief Node count of the source network, or 0 without one. */
    [[nodiscard]] size_t element_count() const;

    /** @brief The number, or the node's last output as of the latest refresh. */
    [[nodiscard]] double value() const noexcept { return m_lane[0].load(std::memory_order_relaxed); }

    /** @brief The value as type T, which must be the type it was set with. */
    template <typename T>
        requires is_value<T>
    [[nodiscard]] T value() const noexcept
    {
        return from_lanes<T>(lanes_for<T>());
    }

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
        if (!has_value() || m_network || m_width.load(std::memory_order_acquire) != width_of<T>()) {
            return std::nullopt;
        }
        if (m_node) {
            return from_lanes<T>(lanes_for<T>());
        }
        if (!m_dirty.exchange(false, std::memory_order_acq_rel)) {
            return std::nullopt;
        }
        return from_lanes<T>(lanes_for<T>());
    }

    /** @brief Reads the node source into value(). Networks call this once per block. */
    void refresh()
    {
        if (m_node) {
            m_lane[0].store(m_node->get_last_output(), std::memory_order_relaxed);
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
        const State state = incoming.read_state();
        store(state.lanes, state.width, true);
        m_dirty.store(true, std::memory_order_release);
    }

    /** @brief Shows the value the owner really has, unless a source is feeding it. */
    template <typename T>
        requires is_value<T>
    void observe(T actual) noexcept
    {
        if (!sourced()) {
            store(to_lanes(actual), width_of<T>(), true);
        }
    }

private:
    static constexpr size_t k_lanes = 4;

    struct State {
        std::array<double, k_lanes> lanes {};
        uint8_t width {};
        bool set {};
    };

    template <typename T>
    static constexpr uint8_t width_of() noexcept
    {
        if constexpr (std::same_as<T, glm::vec2>) {
            return 2;
        } else if constexpr (std::same_as<T, glm::vec3>) {
            return 3;
        } else if constexpr (std::same_as<T, glm::vec4>) {
            return 4;
        } else {
            return 1;
        }
    }

    template <typename T>
    static std::array<double, k_lanes> to_lanes(T value) noexcept
    {
        std::array<double, k_lanes> lanes {};
        if constexpr (std::same_as<T, glm::vec2>) {
            lanes = { value.x, value.y, 0.0, 0.0 };
        } else if constexpr (std::same_as<T, glm::vec3>) {
            lanes = { value.x, value.y, value.z, 0.0 };
        } else if constexpr (std::same_as<T, glm::vec4>) {
            lanes = { value.x, value.y, value.z, value.w };
        } else if constexpr (std::is_enum_v<T>) {
            lanes[0] = static_cast<double>(static_cast<std::underlying_type_t<T>>(value));
        } else {
            lanes[0] = static_cast<double>(value);
        }
        return lanes;
    }

    template <typename T>
    static T from_lanes(const std::array<double, k_lanes>& lanes) noexcept
    {
        if constexpr (std::same_as<T, glm::vec2>) {
            return { static_cast<float>(lanes[0]), static_cast<float>(lanes[1]) };
        } else if constexpr (std::same_as<T, glm::vec3>) {
            return { static_cast<float>(lanes[0]), static_cast<float>(lanes[1]), static_cast<float>(lanes[2]) };
        } else if constexpr (std::same_as<T, glm::vec4>) {
            return { static_cast<float>(lanes[0]), static_cast<float>(lanes[1]), static_cast<float>(lanes[2]), static_cast<float>(lanes[3]) };
        } else if constexpr (std::is_enum_v<T>) {
            return static_cast<T>(static_cast<std::underlying_type_t<T>>(lanes[0]));
        } else if constexpr (std::same_as<T, bool>) {
            return lanes[0] != 0.0;
        } else {
            return static_cast<T>(lanes[0]);
        }
    }

    template <typename T>
    [[nodiscard]] std::array<double, k_lanes> lanes_for() const noexcept
    {
        if constexpr (width_of<T>() == 1) {
            return { m_lane[0].load(std::memory_order_relaxed), 0.0, 0.0, 0.0 };
        } else {
            return read_state().lanes;
        }
    }

    [[nodiscard]] State read_state() const noexcept
    {
        return Memory::seqlock_read(m_lock, 0, [this] {
            State state;
            for (size_t index = 0; index < k_lanes; ++index) {
                state.lanes[index] = m_lane[index].load(std::memory_order_relaxed);
            }
            state.width = m_width.load(std::memory_order_relaxed);
            state.set = m_set.load(std::memory_order_relaxed);
            return state;
        }).value();
    }

    void store(const std::array<double, k_lanes>& lanes, uint8_t width, bool set) noexcept
    {
        Memory::SerializedSeqlockWriteGuard guard(m_lock);
        for (size_t index = 0; index < k_lanes; ++index) {
            m_lane[index].store(lanes[index], std::memory_order_relaxed);
        }
        m_width.store(width, std::memory_order_release);
        m_set.store(set, std::memory_order_release);
    }

    void copy_state(const Control& other) noexcept
    {
        const State state = other.read_state();
        store(state.lanes, state.width, state.set);
        m_dirty.store(other.m_dirty.load(std::memory_order_acquire), std::memory_order_release);
    }

    mutable Memory::Seqlock m_lock;
    std::array<std::atomic<double>, k_lanes> m_lane {};
    std::atomic<uint8_t> m_width { 0 };
    std::atomic<bool> m_set { false };
    std::atomic<bool> m_dirty { false };
    std::shared_ptr<Node> m_node;
    std::shared_ptr<NodeNetwork> m_network;
};

}
