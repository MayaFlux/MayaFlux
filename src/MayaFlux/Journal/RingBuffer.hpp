#pragma once

namespace MayaFlux::Journal {

/**
 * @brief Lock-free SPSC (Single Producer Single Consumer) ring buffer
 *
 * Designed for wait-free writes from realtime threads.
 * Uses atomic operations and careful memory ordering.
 *
 * @tparam T Element type (must be trivially copyable)
 * @tparam Capacity Buffer size (must be power of 2 for fast modulo)
 */
template <typename T, size_t Capacity>
class RingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");

public:
    RingBuffer()
        : m_write_index(0)
        , m_read_index(0)
    {
    }

    /**
     * @brief Attempt to write an element (wait-free)
     * @return true if write succeeded, false if buffer full
     */
    bool try_push(const T& item) noexcept
    {
        const auto current_write = m_write_index.load(std::memory_order_relaxed);
        const auto next_write = increment(current_write);

        if (next_write == m_read_index.load(std::memory_order_acquire)) {
            return false;
        }

        m_buffer[current_write] = item;

        m_write_index.store(next_write, std::memory_order_release);

        return true;
    }

    /**
     * @brief Attempt to read an element
     * @return Element if available, nullopt if buffer empty
     */
    std::optional<T> try_pop() noexcept
    {
        const auto current_read = m_read_index.load(std::memory_order_relaxed);

        if (current_read == m_write_index.load(std::memory_order_acquire)) {
            return std::nullopt;
        }

        T item = m_buffer[current_read];

        // (release semantics for writer)
        m_read_index.store(increment(current_read), std::memory_order_release);

        return item;
    }

    /**
     * @brief Check if buffer is empty
     */
    [[nodiscard]] bool empty() const noexcept
    {
        return m_read_index.load(std::memory_order_acquire) == m_write_index.load(std::memory_order_acquire);
    }

    /**
     * @brief Get approximate size (may be stale)
     */
    [[nodiscard]] size_t size() const noexcept
    {
        const auto write = m_write_index.load(std::memory_order_acquire);
        const auto read = m_read_index.load(std::memory_order_acquire);
        return (write >= read) ? (write - read) : (Capacity - read + write);
    }

    /**
     * @brief Get buffer capacity
     */
    static constexpr size_t capacity() noexcept
    {
        return Capacity;
    }

private:
    static constexpr size_t increment(size_t index) noexcept
    {
        return (index + 1) & (Capacity - 1);
    }

    alignas(64) std::atomic<size_t> m_write_index;
    alignas(64) std::atomic<size_t> m_read_index;
    std::array<T, Capacity> m_buffer;
};

} // namespace MayaFlux::Journal
