#pragma once

#include "MayaFlux/Vruta/Promise.hpp"

namespace MayaFlux::Kriya {

/**
 * @class ConditionAwaiter
 * @brief Suspends a FreeRoutine until a caller-supplied predicate returns true.
 *
 * await_ready() evaluates the condition immediately; if already true the
 * coroutine does not suspend. Otherwise await_suspend() arms the promise so
 * the scheduler's CONDITIONAL thread will evaluate the condition and resume the
 * handle when it returns true.
 *
 * By default the thread polls the condition once per poll interval of the
 * scheduler. A condition that some other thread changes can instead ask to be
 * looked at only after TaskScheduler::wake_conditional(), so the thread sleeps
 * in between.
 *
 * The condition is moved into the promise on suspension and cleared on
 * resumption. It must be safe to call from the scheduler thread.
 *
 * @code
 * auto routine = [&]() -> Vruta::FreeRoutine {
 *     while (true) {
 *         co_await ConditionAwaiter{ [&]{ return flag.load(); } };
 *         do_work();
 *     }
 * };
 * @endcode
 */
class ConditionAwaiter {
public:
    /**
     * @brief Construct with a condition predicate.
     * @param condition Callable returning bool. Evaluated on the scheduler thread.
     * @param wake_only Look at the condition only after TaskScheduler::wake_conditional(), not every poll interval.
     */
    explicit ConditionAwaiter(std::function<bool()> condition, bool wake_only = false)
        : m_condition(std::move(condition))
        , m_wake_only(wake_only)
    {
    }

    /**
     * @brief Evaluate the condition before suspending.
     * @return true if the condition is already met; the coroutine will not suspend.
     */
    [[nodiscard]] bool await_ready() const
    {
        return m_condition && m_condition();
    }

    /**
     * @brief Arm the promise with the condition and the coroutine handle.
     * @param handle Handle to the suspended FreeRoutine coroutine frame.
     *
     * Requires that the coroutine type is FreeRoutine; the promise cast
     * is asserted at compile time via the typed handle overload.
     */
    void await_suspend(std::coroutine_handle<Vruta::conditional_promise> handle)
    {
        auto& p = handle.promise();
        p.condition = std::move(m_condition);
        p.deadline.reset();
        p.wake_only = m_wake_only;
        p.armed.store(true, std::memory_order_release);
    }

    /**
     * @brief No value to return on resumption.
     */
    void await_resume() const { }

private:
    std::function<bool()> m_condition;
    bool m_wake_only;
};

/**
 * @class SteadyDelay
 * @brief Suspends a FreeRoutine for a span of wall-clock time.
 *
 * The scheduler's CONDITIONAL thread sleeps until the earliest such deadline,
 * so a delay does not cost polling. It always suspends once, even for a zero
 * wait, which lets other conditional tasks run.
 *
 * @code
 * auto routine = []() -> Vruta::FreeRoutine {
 *     co_await SteadyDelay { 0.25 };
 *     do_work();
 * };
 * @endcode
 */
class SteadyDelay {
public:
    /**
     * @param seconds Time to wait; zero, negative and NaN wait one pass, and anything past 1e9 waits 1e9.
     */
    explicit SteadyDelay(double seconds)
        : m_deadline(std::chrono::steady_clock::now()
              + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                  std::chrono::duration<double>(seconds > 0.0 ? std::min(seconds, 1.0e9) : 0.0)))
    {
    }

    [[nodiscard]] static bool await_ready() noexcept { return false; }

    void await_suspend(std::coroutine_handle<Vruta::conditional_promise> handle) const
    {
        auto& p = handle.promise();
        p.condition = nullptr;
        p.deadline = m_deadline;
        p.wake_only = false;
        p.armed.store(true, std::memory_order_release);
    }

    void await_resume() const { }

private:
    std::chrono::steady_clock::time_point m_deadline;
};

} // namespace MayaFlux::Kriya
