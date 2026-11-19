#pragma once

#include "MayaFlux/Transitive/Memory/SnapshotCell.hpp"

namespace MayaFlux {

/**
 * @class ControlSnapshot
 * @brief One pending value of any copyable type, such as a string or a list, set from any thread.
 *
 * The counterpart of ControlValue for what does not fit six double lanes. A
 * value is published as one immutable snapshot, so a reader never sees it half
 * written, and a later value replaces one that nothing has applied yet. The
 * consumer takes it once per change with pending(), or reads the current one
 * with value(). It satisfies ControlSlot, so a ControlValueStore can hold it,
 * but not NumericControlSlot, so it has no named number access.
 *
 * @tparam T A copyable value type.
 */
template <typename T>
class ControlSnapshot {
public:
    ControlSnapshot() = default;

    explicit ControlSnapshot(T initial) { set(std::move(initial)); }

    ControlSnapshot(const ControlSnapshot& other) { copy_state(other); }

    ControlSnapshot& operator=(const ControlSnapshot& other)
    {
        if (this != &other) {
            copy_state(other);
        }
        return *this;
    }

    ControlSnapshot(ControlSnapshot&& other) { copy_state(other); }

    ControlSnapshot& operator=(ControlSnapshot&& other)
    {
        if (this != &other) {
            copy_state(other);
        }
        return *this;
    }

    ~ControlSnapshot() = default;

    /** @brief True once a value was given. */
    [[nodiscard]] bool has_value() const noexcept { return m_set.load(std::memory_order_acquire); }

    /** @brief The current value, or null when none was given. */
    [[nodiscard]] std::shared_ptr<const T> value() const noexcept
    {
        const auto view = m_cell.read();
        return view ? *view : nullptr;
    }

    /** @brief True when a consumer should apply this value now. */
    [[nodiscard]] bool due() const noexcept
    {
        return has_value() && m_dirty.load(std::memory_order_acquire);
    }

    /** @brief Marks the current value as applied. */
    void settle() noexcept { m_dirty.exchange(false, std::memory_order_acq_rel); }

    /** @brief The value to apply now, or null when nothing is due. A value is returned once per change. */
    [[nodiscard]] std::shared_ptr<const T> pending() noexcept
    {
        if (!has_value() || !m_dirty.exchange(false, std::memory_order_acq_rel)) {
            return nullptr;
        }
        return value();
    }

    /** @brief Replaces the value, so the next due() is true. */
    void set(T value)
    {
        m_cell.publish(std::make_shared<const T>(std::move(value)));
        m_set.store(true, std::memory_order_release);
        m_dirty.store(true, std::memory_order_release);
    }

    /** @brief Takes the value @p incoming holds, so the next due() is true. */
    void assign(const ControlSnapshot& incoming)
    {
        const auto view = incoming.m_cell.read();
        if (view) {
            m_cell.publish(*view);
            m_set.store(true, std::memory_order_release);
        }
        m_dirty.store(true, std::memory_order_release);
    }

    /** @brief Nothing to read from a source. */
    void refresh() noexcept { }

private:
    void copy_state(const ControlSnapshot& other)
    {
        const auto view = other.m_cell.read();
        if (view) {
            m_cell.publish(*view);
        }
        m_set.store(other.m_set.load(std::memory_order_acquire), std::memory_order_release);
        m_dirty.store(other.m_dirty.load(std::memory_order_acquire), std::memory_order_release);
    }

    Memory::SnapshotCell<std::shared_ptr<const T>> m_cell;
    std::atomic<bool> m_set { false };
    std::atomic<bool> m_dirty { false };
};

}
