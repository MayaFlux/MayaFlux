#pragma once

#include "MayaFlux/Vruta/NetworkSource.hpp"

#include <coroutine>

namespace MayaFlux::Kriya {

/**
 * @class NetworkAwaiter
 * @brief Awaiter for suspending a coroutine until a network message arrives
 *
 * Follows the same structural pattern as EventAwaiter. Works with any
 * coroutine type: SoundRoutine, GraphicsRoutine, Event, ComplexRoutine.
 *
 * Multiple awaiters may register against the same NetworkSource
 * simultaneously. signal() delivers a copy of the message directly to
 * each awaiter — no shared queue contention between waiters.
 *
 * The queue on NetworkSource is used only by await_ready() for the
 * no-suspend fast path. Once suspended, the awaiter receives its
 * message via deliver() called directly from NetworkSource::signal().
 *
 * The awaiter does not own the NetworkSource. The source must outlive
 * any suspended awaiter.
 *
 * @code
 * Vruta::NetworkSource source({ .transport = NetworkTransport::UDP, .local_port = 8000 });
 *
 * auto routine = [&source]() -> Vruta::Event {
 *     while (true) {
 *         auto msg = co_await source.next_message();
 *         process(msg.data);
 *     }
 * };
 * @endcode
 */
class MAYAFLUX_API NetworkAwaiter {
public:
    explicit NetworkAwaiter(Vruta::NetworkSource& source)
        : m_source(source)
    {
    }

    ~NetworkAwaiter();

    NetworkAwaiter(const NetworkAwaiter&) = delete;
    NetworkAwaiter& operator=(const NetworkAwaiter&) = delete;
    NetworkAwaiter(NetworkAwaiter&&) noexcept = default;
    NetworkAwaiter& operator=(NetworkAwaiter&&) noexcept = delete;

    /**
     * @brief Check if a message is already queued
     * @return true if a message is available (no suspend needed)
     */
    bool await_ready();

    /**
     * @brief Suspend the coroutine and register with the source
     * @param handle Type-erased coroutine handle
     */
    void await_suspend(std::coroutine_handle<> handle);

    /**
     * @brief Return the received message on resume
     * @return The NetworkMessage that caused resumption
     */
    Core::NetworkMessage await_resume();

    /**
     * @brief Called by NetworkSource::signal() with the incoming message
     *
     * Stores the message directly, unregisters from the waiter list,
     * and resumes the suspended coroutine. Each awaiter gets its own
     * copy — no queue pop, no contention with other waiters.
     */
    void deliver(const Core::NetworkMessage& message);

private:
    Vruta::NetworkSource& m_source;
    Core::NetworkMessage m_result;
    std::coroutine_handle<> m_handle;
    bool m_is_suspended {};

    /**
     * @brief Intrusive list link for lock-free waiter broadcast.
     */
    std::atomic<NetworkAwaiter*> m_next { nullptr };

    friend class Vruta::NetworkSource;
};

} // namespace MayaFlux::Kriya
