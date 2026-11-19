#pragma once

#include "MayaFlux/Transitive/Memory/SeqLock.hpp"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace MayaFlux {

/**
 * @class ControlValue
 * @brief One pending value, written from any thread and applied once per change by one consumer.
 *
 * Holds a number, a bool, an enum, a float glm vector or a box (a struct of
 * exactly a glm::vec3 `min` and a glm::vec3 `max`) as six double lanes, a width
 * and flags. Numbers are atomic, and a vector is written and read under a
 * sequence lock, so it is never seen half written. A value is never lost to a
 * race, only possibly applied twice with the same content. It holds one pending
 * value, so a later value replaces one that no block has applied yet. Integers
 * are carried exactly up to 2^53. A read of another type than was set finds nothing.
 */
class ControlValue {
public:
    template <typename T>
    static constexpr bool is_vector = requires {
        requires GlmVectorType<T>;
        requires std::same_as<glm_component_type<T>, float>;
    };

    template <typename T>
    static constexpr bool is_box = requires(T box) {
        requires std::same_as<decltype(box.min), glm::vec3>;
        requires std::same_as<decltype(box.max), glm::vec3>;
    } && sizeof(T) == 2 * sizeof(glm::vec3);

    template <typename T>
    static constexpr bool is_value = std::is_arithmetic_v<T> || std::is_enum_v<T> || is_vector<T> || is_box<T>;

    ControlValue() = default;

    template <typename T>
        requires is_value<T>
    explicit ControlValue(T value) noexcept
    {
        store(to_lanes(value), width_of<T>(), true);
        m_dirty.store(true, std::memory_order_release);
    }

    ControlValue(const ControlValue& other) noexcept { copy_state(other); }

    ControlValue(ControlValue&& other) noexcept { copy_state(other); }

    ControlValue& operator=(const ControlValue& other) noexcept
    {
        if (this != &other) {
            copy_state(other);
        }
        return *this;
    }

    ControlValue& operator=(ControlValue&& other) noexcept
    {
        if (this != &other) {
            copy_state(other);
        }
        return *this;
    }

    ~ControlValue() = default;

    /** @brief True once a value was given. */
    [[nodiscard]] bool has_value() const noexcept { return m_set.load(std::memory_order_acquire); }

    /** @brief Components held: 0 when empty, 1 for a number, flag or enum, 2 to 4 for a vector, 6 for a box. */
    [[nodiscard]] size_t width() const noexcept { return m_width.load(std::memory_order_acquire); }

    /** @brief The value as a double. */
    [[nodiscard]] double value() const noexcept { return m_lane[0].load(std::memory_order_relaxed); }

    /** @brief The value as type T, which must be the type it was set with. */
    template <typename T>
        requires is_value<T>
    [[nodiscard]] T value() const noexcept
    {
        return from_lanes<T>(lanes_for<T>());
    }

    operator double() const noexcept { return value(); }

    /** @brief True when a consumer should apply this value now. */
    [[nodiscard]] bool due() const noexcept { return has_value() && changed(); }

    /** @brief Marks the current value as applied. Call before reading value(). */
    void settle() noexcept { m_dirty.exchange(false, std::memory_order_acq_rel); }

    /**
     * @brief The value to apply now as type T, or nullopt when nothing is due.
     *
     * A value is returned once per change. A value set with another type than T
     * returns nothing.
     */
    template <typename T = double>
        requires is_value<T>
    [[nodiscard]] std::optional<T> pending() noexcept
    {
        if (!has_value() || m_width.load(std::memory_order_acquire) != width_of<T>()) {
            return std::nullopt;
        }
        if (!m_dirty.exchange(false, std::memory_order_acq_rel)) {
            return std::nullopt;
        }
        return from_lanes<T>(lanes_for<T>());
    }

    /** @brief Nothing to read for a plain value. A holder with a source hides this. */
    void refresh() noexcept { }

    /** @brief Takes everything @p incoming holds, so the next due() is true. */
    void assign(const ControlValue& incoming) noexcept
    {
        const State state = incoming.read_state();
        store(state.lanes, state.width, true);
        m_dirty.store(true, std::memory_order_release);
    }

    /** @brief Shows the value the owner really has. */
    template <typename T>
        requires is_value<T>
    void observe(T actual) noexcept
    {
        store(to_lanes(actual), width_of<T>(), true);
    }

protected:
    static constexpr size_t k_lanes = 6;

    template <typename T>
    static constexpr uint8_t width_of() noexcept
    {
        if constexpr (is_vector<T>) {
            return static_cast<uint8_t>(glm_component_count<T>());
        } else if constexpr (is_box<T>) {
            return 6;
        } else {
            return 1;
        }
    }

    [[nodiscard]] bool changed() const noexcept { return m_dirty.load(std::memory_order_acquire); }

    /** @brief Marks the slot set with one zero lane and leaves it clean. */
    void set_blank() noexcept { store({}, 1, true); }

    void set_scalar(double value) noexcept { m_lane[0].store(value, std::memory_order_relaxed); }

private:
    struct State {
        std::array<double, k_lanes> lanes {};
        uint8_t width {};
        bool set {};
    };

    template <typename T>
    static std::array<double, k_lanes> to_lanes(T value) noexcept
    {
        std::array<double, k_lanes> lanes {};
        if constexpr (is_vector<T>) {
            for (size_t index = 0; index < glm_component_count<T>(); ++index) {
                lanes[index] = value[static_cast<glm::length_t>(index)];
            }
        } else if constexpr (is_box<T>) {
            lanes = { value.min.x, value.min.y, value.min.z, value.max.x, value.max.y, value.max.z };
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
        if constexpr (is_vector<T>) {
            T vector {};
            for (size_t index = 0; index < glm_component_count<T>(); ++index) {
                vector[static_cast<glm::length_t>(index)] = static_cast<float>(lanes[index]);
            }
            return vector;
        } else if constexpr (is_box<T>) {
            T box {};
            box.min = { static_cast<float>(lanes[0]), static_cast<float>(lanes[1]), static_cast<float>(lanes[2]) };
            box.max = { static_cast<float>(lanes[3]), static_cast<float>(lanes[4]), static_cast<float>(lanes[5]) };
            return box;
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

    void copy_state(const ControlValue& other) noexcept
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
};

}
