#pragma once

namespace MayaFlux::Memory {

template <typename T, size_t N>
class LockFreeRingBuffer {
public:
    bool push(const T& item)
    {
        size_t head = m_head.load(std::memory_order_relaxed);
        size_t next = (head + 1) % N;
        if (next == m_tail.load(std::memory_order_acquire)) {
            return false;
        }
        m_buffer[head] = item;
        m_head.store(next, std::memory_order_release);
        return true;
    }

    bool pop(T& item)
    {
        size_t tail = m_tail.load(std::memory_order_relaxed);
        if (tail == m_head.load(std::memory_order_acquire)) {
            return false; // empty
        }
        item = m_buffer[tail];
        m_tail.store((tail + 1) % N, std::memory_order_release);
        return true;
    }

    [[nodiscard]] std::vector<T> snapshot() const
    {
        std::vector<T> result;
        size_t tail = m_tail.load();
        size_t head = m_head.load();
        while (tail != head) {
            result.push_back(m_buffer[tail]);
            tail = (tail + 1) % N;
        }
        return result;
    }

private:
    std::array<T, N> m_buffer;
    std::atomic<size_t> m_head { 0 };
    std::atomic<size_t> m_tail { 0 };
};

} // namespace MayaFlux::Memory
