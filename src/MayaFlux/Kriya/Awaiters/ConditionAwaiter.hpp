#pragma once

#include "MayaFlux/Vruta/Promise.hpp"

namespace MayaFlux::Kriya {

/**
 * @class ConditionAwaiter
 * @brief Suspends a FreeRoutine until a caller-supplied predicate returns true.
 *
 * await_ready() evaluates the condition immediately; if already true the
 * coroutine does not suspend. Otherwise await_suspend() arms the promise so
 * the scheduler's CONDITIONAL thread will evaluate the condition on each pass
 * and resume the handle when it returns true.
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
     */
    explicit ConditionAwaiter(std::function<bool()> condition)
        : m_condition(std::move(condition))
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
        p.armed.store(true, std::memory_order_release);
    }

    /**
     * @brief No value to return on resumption.
     */
    void await_resume() const { }

private:
    std::function<bool()> m_condition;
};

} // namespace MayaFlux::Kriya
