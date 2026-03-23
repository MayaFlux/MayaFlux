#pragma once

#include "MayaFlux/Core/GlobalNetworkConfig.hpp"
#include "MayaFlux/Transitive/Memory/RingBuffer.hpp"

namespace MayaFlux::Kriya {
class NetworkAwaiter;
}

namespace MayaFlux::Vruta {

/**
 * @class NetworkSource
 * @brief Awaitable broadcast message stream for a network endpoint
 *
 * Opens and owns a network endpoint on construction. Any number of
 * coroutines may co_await the same source simultaneously via next_message().
 * signal() broadcasts to all registered waiters on every incoming message.
 *
 * Fully lock-free on all paths. Waiter registration uses an intrusive
 * singly-linked list with an atomic head. signal() atomically detaches
 * the entire list and resumes each awaiter. The message queue is a
 * lock-free ring buffer shared by all waiters — each awaiter pops its
 * own copy on resumption.
 *
 * Endpoint lifecycle is internal. BackendRegistry is consulted in the
 * constructor and destructor; the caller never touches NetworkService.
 *
 * @code
 * Vruta::NetworkSource source({ .transport = NetworkTransport::UDP, .local_port = 8000 });
 *
 * auto handler = [&source]() -> Vruta::Event {
 *     while (true) {
 *         auto msg = co_await source.next_message();
 *         std::cout << msg.data.size() << " bytes from "
 *                   << msg.sender_address << "\n";
 *     }
 * };
 * @endcode
 */
class MAYAFLUX_API NetworkSource {
public:
    /**
     * @brief Open a network endpoint and begin receiving
     * @param info Endpoint configuration.
     *
     * Opens the endpoint via NetworkService and registers a receive
     * callback that calls signal(). Logs a warning and remains inert
     * if NetworkService is unavailable.
     */
    explicit NetworkSource(const Core::EndpointInfo& info);

    /**
     * @brief Close the endpoint and release all resources
     */
    ~NetworkSource();

    NetworkSource(const NetworkSource&) = delete;
    NetworkSource& operator=(const NetworkSource&) = delete;
    NetworkSource(NetworkSource&&) noexcept = default;
    NetworkSource& operator=(NetworkSource&&) noexcept = default;

    /**
     * @brief Signal that a message arrived (called from Asio IO thread)
     * @param message The received message.
     *
     * Lock-free. Pushes into ring buffer, atomically detaches the full
     * waiter list, and calls try_resume() on every registered awaiter.
     * Safe to call from any thread.
     */
    void signal(const Core::NetworkMessage& message);

    /**
     * @brief Create an awaiter for the next message
     * @return Awaiter that suspends until a message arrives.
     */
    Kriya::NetworkAwaiter next_message();

    /**
     * @brief Check if messages are pending
     */
    [[nodiscard]] bool has_pending() const;

    /**
     * @brief Get number of pending messages
     */
    [[nodiscard]] size_t pending_count() const;

    /**
     * @brief Clear all pending messages
     */
    void clear();

private:
    static constexpr size_t QUEUE_CAPACITY = 256;

    uint64_t m_endpoint_id {};
    Memory::LockFreeQueue<Core::NetworkMessage, QUEUE_CAPACITY> m_queue;

    /**
     * @brief Head of the intrusive lock-free waiter list.
     *
     * Each NetworkAwaiter carries an atomic next pointer. register_waiter
     * pushes onto the head with a CAS loop. signal() atomically swaps the
     * head to null, detaching the full list, then walks and resumes each
     * awaiter. No mutex, no fixed cap, no missed wakeups.
     */
    std::atomic<Kriya::NetworkAwaiter*> m_waiters_head { nullptr };

    std::optional<Core::NetworkMessage> pop_message();

    void register_waiter(Kriya::NetworkAwaiter* awaiter);
    void unregister_waiter(Kriya::NetworkAwaiter* awaiter);

    friend class Kriya::NetworkAwaiter;
};

} // namespace MayaFlux::Vruta
