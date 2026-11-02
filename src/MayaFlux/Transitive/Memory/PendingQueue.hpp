#pragma once

namespace MayaFlux::Memory {

/**
 * @class PendingQueue
 * @brief Bounded multi-producer, multi-consumer queue of deferred operations.
 *
 * Producers on any thread push an operation; any thread may drain the queue at
 * a point where mutation is safe, and each operation is applied exactly once,
 * in the order it was claimed. Each cell carries its own sequence
 * number, so a producer only claims a cell that is free and a consumer only
 * reads a cell whose payload has been published. A full queue rejects the push
 * without disturbing any other claim.
 *
 * Capacity must be a power of two. T must be default constructible and move
 * assignable; the drained cell is reset so the queue never retains a payload
 * (for example a shared_ptr) past its application.
 *
 * Overlapping drains never hand out the same operation twice, but they do not
 * serialise the apply step; the caller guards whatever @p apply mutates.
 *
 * @code
 * PendingQueue<Op, 1024> queue;
 *
 * if (!queue.push(Op { node, true })) {
 *     // full: retry after the next drain
 * }
 *
 * queue.drain([&](Op&& op) { apply(std::move(op)); });
 * @endcode
 */
template <typename T, size_t Capacity>
class PendingQueue {
    static_assert(Capacity >= 2 && (Capacity & (Capacity - 1)) == 0,
        "PendingQueue capacity must be a power of two");
    static_assert(std::is_default_constructible_v<T> && std::is_move_assignable_v<T>,
        "PendingQueue payload must be default constructible and move assignable");

public:
    PendingQueue() noexcept
    {
        for (size_t i = 0; i < Capacity; ++i) {
            m_cells[i].sequence.store(i, std::memory_order_relaxed);
        }
    }

    PendingQueue(const PendingQueue&) = delete;
    PendingQueue& operator=(const PendingQueue&) = delete;
    PendingQueue(PendingQueue&&) = delete;
    PendingQueue& operator=(PendingQueue&&) = delete;
    ~PendingQueue() = default;

    /**
     * @brief Enqueue an operation from any thread.
     * @return false if the queue is full; the operation is left untouched.
     */
    [[nodiscard]] bool push(T&& value) noexcept(std::is_nothrow_move_assignable_v<T>)
    {
        size_t pos = m_enqueue.load(std::memory_order_relaxed);
        Cell* cell = nullptr;

        for (;;) {
            cell = &m_cells[pos & MASK];
            const size_t seq = cell->sequence.load(std::memory_order_acquire);
            const auto diff = static_cast<std::intptr_t>(seq) - static_cast<std::intptr_t>(pos);

            if (diff == 0) {
                if (m_enqueue.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    break;
                }
            } else if (diff < 0) {
                return false;
            } else {
                pos = m_enqueue.load(std::memory_order_relaxed);
            }
        }

        cell->value = std::move(value);
        cell->sequence.store(pos + 1, std::memory_order_release);
        return true;
    }

    /**
     * @brief Apply and remove the published operations claimed before the call.
     * @param apply Callable taking T&&.
     * @return Number of operations this call applied.
     *
     * Safe from any thread, including overlapping drains: each operation is
     * handed to exactly one caller. Operations pushed while the drain runs,
     * including by @p apply itself, wait for the next drain. Stops at the first
     * cell whose producer has claimed but not yet published; that operation and
     * everything behind it is applied by a later drain.
     */
    template <typename Apply>
    size_t drain(Apply&& apply)
    {
        const size_t limit = m_enqueue.load(std::memory_order_acquire);
        size_t count = 0;

        for (;;) {
            size_t pos = m_dequeue.load(std::memory_order_relaxed);
            if (static_cast<std::intptr_t>(limit - pos) <= 0) {
                break;
            }

            Cell& cell = m_cells[pos & MASK];
            const auto diff = static_cast<std::intptr_t>(cell.sequence.load(std::memory_order_acquire))
                - static_cast<std::intptr_t>(pos + 1);

            if (diff == 0) {
                if (!m_dequeue.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    continue;
                }

                T value = std::move(cell.value);
                cell.value = T {};
                cell.sequence.store(pos + Capacity, std::memory_order_release);
                ++count;

                std::forward<Apply>(apply)(std::move(value));
            } else if (diff < 0) {
                break;
            }
        }

        return count;
    }

    /**
     * @brief Cheap check for claimed operations awaiting a drain.
     *
     * May report true for an operation still being published; drain() then
     * applies nothing and the operation is picked up by the next one.
     */
    [[nodiscard]] bool has_pending() const noexcept
    {
        return m_enqueue.load(std::memory_order_relaxed)
            != m_dequeue.load(std::memory_order_relaxed);
    }

private:
    static constexpr size_t MASK = Capacity - 1;

    struct Cell {
        std::atomic<size_t> sequence { 0 };
        T value {};
    };

    alignas(64) std::atomic<size_t> m_enqueue { 0 };
    alignas(64) std::atomic<size_t> m_dequeue { 0 };
    std::array<Cell, Capacity> m_cells;
};

}
