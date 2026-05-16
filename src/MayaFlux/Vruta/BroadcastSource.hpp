#pragma once

#include "MayaFlux/Vruta/EventSource.hpp"

namespace MayaFlux::Kriya {
template <typename T>
class BroadcastAwaiter;
}

namespace MayaFlux::Vruta {

/**
 * @class BroadcastSource
 * @brief Awaitable single-value broadcast channel for cross-thread signal delivery.
 *
 * Bridges a callback-based producer (e.g. audio output observer) to a coroutine
 * consumer via co_await. Each signal() call delivers to the suspended awaiter,
 * or stores the value for the next await_ready() poll if no awaiter is registered.
 *
 * Thread safety contract:
 * - signal() may be called from any thread at any time.
 * - register_waiter() / unregister_waiter() / pop_pending() are called only
 *   from the coroutine's execution context (single-threaded per coroutine).
 *
 * Race window between await_ready() returning false and await_suspend()
 * completing registration is closed by signal() itself: it uses a generation
 * counter so a missed signal leaves the pending slot non-null, which
 * await_suspend() checks after registration via a second pop_pending() call.
 * The self-resume in await_suspend is intentionally absent; instead the CAS
 * leaves the awaiter registered and the value in m_result, and returns to the
 * coroutine machinery which will call await_resume() when the handle is resumed
 * by the scheduler or by a subsequent signal() deliver() call.
 *
 * One coroutine per BroadcastSource instance. For fan-out, use one
 * BroadcastSource per consumer.
 *
 * @tparam T Payload type. Must be copyable.
 *
 * @see BroadcastAwaiter
 */
template <typename T>
class BroadcastSource : public EventSource {
public:
    BroadcastSource() = default;
    ~BroadcastSource() override = default;

    BroadcastSource(const BroadcastSource&) = delete;
    BroadcastSource& operator=(const BroadcastSource&) = delete;
    BroadcastSource(BroadcastSource&&) = delete;
    BroadcastSource& operator=(BroadcastSource&&) = delete;

    /**
     * @brief Deliver a value to the waiting coroutine, or store it as pending.
     *
     * Atomically takes the waiter slot. If a waiter is present, delivers
     * directly and resumes. Otherwise stores the value for the next
     * await_ready() or post-registration check.
     *
     * Safe to call from any thread.
     *
     * @param value Value to deliver.
     */
    void signal(const T& value)
    {
        auto* w = m_waiter.exchange(nullptr, std::memory_order_acq_rel);
        if (w) {
            w->deliver(value);
        } else {
            m_pending_value = value;
            m_pending_flag.store(true, std::memory_order_release);
        }
    }

    /**
     * @brief Create an awaiter for the next signal.
     */
    Kriya::BroadcastAwaiter<T> next()
    {
        return Kriya::BroadcastAwaiter<T> { *this };
    }

    /**
     * @brief Returns true if a value arrived before any awaiter registered.
     */
    [[nodiscard]] bool has_pending() const override
    {
        return m_pending_flag.load(std::memory_order_acquire);
    }

    /**
     * @brief Discard any stored pending value.
     */
    void clear() override
    {
        m_pending_flag.store(false, std::memory_order_release);
    }

private:
    std::atomic<Kriya::BroadcastAwaiter<T>*> m_waiter { nullptr };

    /// @brief Pending value written by signal() when no waiter is registered.
    /// Written only by the signal() caller; read only by the coroutine thread.
    T m_pending_value {};
    std::atomic<bool> m_pending_flag { false };

    /**
     * @brief Consume and return the pending value if one is available.
     *
     * Called only from the coroutine's execution context.
     *
     * @return The pending value, or std::nullopt.
     */
    std::optional<T> pop_pending()
    {
        if (!m_pending_flag.load(std::memory_order_acquire))
            return std::nullopt;
        T val = m_pending_value;
        m_pending_flag.store(false, std::memory_order_release);
        return val;
    }

    void register_waiter(Kriya::BroadcastAwaiter<T>* awaiter)
    {
        m_waiter.store(awaiter, std::memory_order_release);
    }

    void unregister_waiter(Kriya::BroadcastAwaiter<T>* awaiter)
    {
        Kriya::BroadcastAwaiter<T>* expected = awaiter;
        m_waiter.compare_exchange_strong(
            expected, nullptr,
            std::memory_order_acq_rel, std::memory_order_relaxed);
    }

    friend class Kriya::BroadcastAwaiter<T>;
};

} // namespace MayaFlux::Vruta
