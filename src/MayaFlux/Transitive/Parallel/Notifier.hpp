#pragma once

#include <semaphore>

namespace MayaFlux::Parallel {

/**
 * @class Notifier
 * @brief Lets one thread sleep until any other thread says there is something to look at.
 *
 * notify() is lock-free and safe from any thread, including a real-time one.
 * Notifications that arrive before the waiter returns collapse into one, so a
 * wake means "check now", never "this many things happened". A wait may return
 * with nothing to find; the caller re-checks its own state.
 *
 * One thread waits. Stopping a waiter is the caller's job: register
 * notify() as a std::stop_callback on its stop token.
 *
 * @code{.cpp}
 * Parallel::Notifier wake;
 * std::jthread worker([&](const std::stop_token& stop) {
 *     const std::stop_callback on_stop(stop, [&] { wake.notify(); });
 *     while (!stop.stop_requested()) {
 *         wake.wait();
 *         drain_work();
 *     }
 * });
 *
 * push_work();
 * wake.notify();
 * @endcode
 */
class Notifier {
public:
    Notifier() = default;

    Notifier(const Notifier&) = delete;
    Notifier& operator=(const Notifier&) = delete;
    Notifier(Notifier&&) = delete;
    Notifier& operator=(Notifier&&) = delete;

    /** @brief Wake the waiter, or make its next wait return at once. */
    void notify() noexcept
    {
        if (!m_pending.exchange(true, std::memory_order_acq_rel)) {
            m_gate.release();
        }
    }

    /** @brief Block until notified. */
    void wait() noexcept
    {
        m_gate.acquire();
        m_pending.exchange(false, std::memory_order_acq_rel);
    }

    /** @brief Block until notified or @p timeout passes; true when notified. */
    template <typename Rep, typename Period>
    [[nodiscard]] bool wait_for(const std::chrono::duration<Rep, Period>& timeout) noexcept
    {
        if (!m_gate.try_acquire_for(timeout)) {
            return false;
        }
        m_pending.exchange(false, std::memory_order_acq_rel);
        return true;
    }

    /** @brief Block until notified or @p deadline passes; true when notified. */
    template <typename Clock, typename Duration>
    [[nodiscard]] bool wait_until(const std::chrono::time_point<Clock, Duration>& deadline) noexcept
    {
        if (!m_gate.try_acquire_until(deadline)) {
            return false;
        }
        m_pending.exchange(false, std::memory_order_acq_rel);
        return true;
    }

private:
    std::binary_semaphore m_gate { 0 };
    std::atomic<bool> m_pending { false };
};

} // namespace MayaFlux::Parallel
