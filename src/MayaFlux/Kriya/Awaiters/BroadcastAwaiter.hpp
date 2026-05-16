#pragma once

#include "EventAwaiter.hpp"
#include "MayaFlux/Vruta/BroadcastSource.hpp"

namespace MayaFlux::Kriya {

/**
 * @class BroadcastAwaiter
 * @brief Awaiter for suspending a coroutine until a BroadcastSource<T> fires.
 *
 * Works with any coroutine type: SoundRoutine, GraphicsRoutine, Event,
 * ComplexRoutine. The awaiter does not own the source; the source must
 * outlive any suspended awaiter.
 *
 * Edge-triggered: await_ready() returns true only when a value arrived
 * before this co_await expression (pending slot occupied). Otherwise the
 * coroutine suspends and is resumed by the next signal() call.
 *
 * One BroadcastAwaiter may be registered against a BroadcastSource at a
 * time. For fan-out to multiple consumers, use one BroadcastSource per
 * consumer.
 *
 * The captured shared_ptr to the BroadcastSource must be copied into a
 * coroutine-local variable before first use, not referenced only through
 * the lambda closure. The lambda closure lives on the caller's stack;
 * the coroutine frame is heap-allocated and outlives the caller.
 *
 * @code
 * auto src = make_persistent_shared<Vruta::BroadcastSource<MyType>>();
 *
 * auto routine = [src]() -> Vruta::Event {
 *     auto local_src = src;   // pull into coroutine frame
 *     while (true) {
 *         auto val = co_await local_src->next();
 *     }
 * };
 * @endcode
 *
 * @tparam T Payload type matching the bound BroadcastSource<T>.
 * @see BroadcastSource
 */
template <typename T>
class BroadcastAwaiter : public EventAwaiter {
public:
    explicit BroadcastAwaiter(Vruta::BroadcastSource<T>& source)
        : m_source(source)
    {
    }

    ~BroadcastAwaiter() override
    {
        if (m_is_suspended)
            m_source.unregister_waiter(this);
    }

    BroadcastAwaiter(const BroadcastAwaiter&) = delete;
    BroadcastAwaiter& operator=(const BroadcastAwaiter&) = delete;
    BroadcastAwaiter(BroadcastAwaiter&&) noexcept = default;
    BroadcastAwaiter& operator=(BroadcastAwaiter&&) noexcept = delete;

    /**
     * @brief Returns true if a pending value is already available.
     *
     * Consumes the pending value on success. The coroutine does not suspend.
     */
    bool await_ready()
    {
        auto val = m_source.pop_pending();
        if (val) {
            m_result = std::move(*val);
            return true;
        }
        return false;
    }

    /**
     * @brief Register with the source and suspend, or consume a pending value
     *        that arrived in the race window and return false (no suspend).
     *
     * @param handle Coroutine handle.
     * @return true if the coroutine should suspend, false if it should continue.
     */
    bool await_suspend(std::coroutine_handle<> handle)
    {
        m_handle = handle;
        m_is_suspended = true;
        m_source.register_waiter(this);

        if (auto val = m_source.pop_pending()) {
            m_source.unregister_waiter(this);
            m_result = std::move(*val);
            m_is_suspended = false;
            return false;
        }

        return true;
    }

    /**
     * @brief Return the delivered value on resume.
     */
    T await_resume()
    {
        m_is_suspended = false;
        return std::move(m_result);
    }

    /**
     * @brief Called by BroadcastSource::signal() from any thread.
     * @param value Value committed by the signaller.
     */
    void deliver(const T& value)
    {
        m_result = value;
        m_is_suspended = false;
        m_handle.resume();
    }

    /**
     * @brief Satisfies EventAwaiter interface. BroadcastAwaiter bypasses
     * the EventSource dispatch() path; deliver() is called directly.
     */
    void try_resume(const void*) override { }

    /**
     * @brief BroadcastAwaiter has no filter semantics. Always returns true.
     */
    [[nodiscard]] bool filter_matches(const void*) const override { return true; }

private:
    Vruta::BroadcastSource<T>& m_source;
    T m_result {};
};

} // namespace MayaFlux::Kriya
