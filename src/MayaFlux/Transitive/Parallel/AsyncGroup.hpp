#pragma once

#include <future>

#include "MayaFlux/Transitive/Memory/SweepList.hpp"

namespace MayaFlux::Parallel {

/**
 * @class AsyncGroup
 * @brief Owns a set of in-flight std::async futures on behalf of a subsystem.
 *
 * Retains each future past the initiating call, prunes completed work on later
 * submissions, and blocks on whatever remains at teardown. It is not a thread
 * pool: callers launch the work themselves, with std::async or anything
 * returning a std::future<T>, and hand the future to submit(). Submission, pruning
 * and drain are lock-free.
 *
 * An exception from a submitted task goes to the handler given at construction.
 * Without one it is kept and returned by take_errors().
 *
 * @tparam T Result type of the tracked futures. AsyncGroup<void> tracks
 *           completion only.
 *
 * @code{.cpp}
 * Parallel::AsyncGroup<bool> save_tasks(
 *     [](std::exception_ptr ep) {
 *         try { std::rethrow_exception(ep); }
 *         catch (const std::exception& e) { MF_ERROR(..., "save failed: {}", e.what()); }
 *     });
 *
 * save_tasks.submit(std::async(std::launch::async, [] { return write_file(); }));
 *
 * save_tasks.drain();
 * @endcode
 */
template <typename T = void>
class AsyncGroup {
public:
    /**
     * @brief Exception policy invoked once per exception observed from a
     *        submitted future's get().
     */
    using ExceptionHandler = std::function<void(std::exception_ptr)>;

    AsyncGroup() = default;

    /**
     * @brief Construct with an explicit exception policy.
     * @param handler Called once per exception, from whichever thread
     *                observes it (a submit() caller during opportunistic
     *                pruning, or the thread calling drain()).
     */
    explicit AsyncGroup(ExceptionHandler handler)
        : m_handler(std::move(handler))
    {
    }

    AsyncGroup(const AsyncGroup&) = delete;
    AsyncGroup& operator=(const AsyncGroup&) = delete;
    AsyncGroup(AsyncGroup&&) = delete;
    AsyncGroup& operator=(AsyncGroup&&) = delete;

    /**
     * @brief Drains all in-flight work before destruction.
     *
     * Blocks until every submitted future completes. Equivalent to calling
     * drain() explicitly; provided so an owning subsystem gets this
     * guarantee for free from its own destructor.
     */
    ~AsyncGroup()
    {
        drain();
    }

    /**
     * @brief Retains @p fut and reclaims completed work.
     *
     * Safe from any thread. Each call also sweeps previously submitted futures
     * without blocking, so the retained set does not grow under steady submission.
     *
     * @param fut A future for work that is already running, for example from
     *            std::async(std::launch::async, ...). submit() does not start it.
     */
    void submit(std::future<T> fut)
    {
        m_tasks.push(std::move(fut));

        m_tasks.sweep(
            [](std::future<T>& f) {
                return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
            },
            [this](std::future<T>& f) { finish(f); });
    }

    /**
     * @brief Blocks until every currently submitted future completes.
     *
     * Safe to call from any thread. Futures submitted concurrently by
     * another thread during drain() are not guaranteed to be included; call
     * drain() again if a caller needs a stronger guarantee after quiescing
     * all submitters.
     */
    void drain()
    {
        m_tasks.drain([this](std::future<T>& f) { finish(f); });
    }

    /**
     * @brief Returns and clears exceptions accumulated with no handler set.
     *
     * Only meaningful when the group was constructed without an
     * ExceptionHandler. With a handler, exceptions are reported at the
     * point they're observed and never accumulate here.
     */
    [[nodiscard]] std::vector<std::exception_ptr> take_errors()
    {
        std::vector<std::exception_ptr> errors;
        m_errors.drain([&errors](std::exception_ptr& ep) {
            errors.push_back(std::move(ep));
        });
        return errors;
    }

    /**
     * @brief Approximate count of futures not yet reclaimed.
     * @see Memory::SweepList::approx_size
     */
    [[nodiscard]] size_t approx_pending() const noexcept
    {
        return m_tasks.approx_size();
    }

private:
    void finish(std::future<T>& f)
    {
        try {
            if constexpr (std::is_void_v<T>) {
                f.get();
            } else {
                (void)f.get();
            }
        } catch (...) {
            report(std::current_exception());
        }
    }

    void report(std::exception_ptr ep)
    {
        if (m_handler) {
            m_handler(ep);
        } else {
            m_errors.push(std::move(ep));
        }
    }

    ExceptionHandler m_handler;
    Memory::SweepList<std::future<T>> m_tasks;
    Memory::SweepList<std::exception_ptr> m_errors;
};

} // namespace MayaFlux::Parallel
