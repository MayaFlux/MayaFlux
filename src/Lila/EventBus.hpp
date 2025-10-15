#pragma once

namespace Lila {

/**
 * @enum EventType
 * @brief Enumerates all event types supported by the Lila event system.
 *
 * These events represent key lifecycle and runtime actions in the live coding environment,
 * such as client connections, code evaluation, symbol definition, and server state changes.
 */
enum class EventType : uint8_t {
    ClientConnected, ///< A client has connected to the server
    ClientDisconnected, ///< A client has disconnected from the server
    EvalStart, ///< Code evaluation has started
    EvalSuccess, ///< Code evaluation succeeded
    EvalError, ///< Code evaluation failed
    SymbolDefined, ///< A new symbol was defined in the interpreter
    ServerStart, ///< The server has started
    ServerStop ///< The server has stopped
};

/**
 * @brief Converts an EventType to its string representation.
 * @param type EventType value
 * @return String view of the event name
 */
constexpr std::string_view to_string(EventType type)
{
    switch (type) {
    case EventType::ClientConnected:
        return "client_connected";
    case EventType::ClientDisconnected:
        return "client_disconnected";
    case EventType::EvalStart:
        return "eval_start";
    case EventType::EvalSuccess:
        return "eval_success";
    case EventType::EvalError:
        return "eval_error";
    case EventType::SymbolDefined:
        return "symbol_defined";
    case EventType::ServerStart:
        return "server_start";
    case EventType::ServerStop:
        return "server_stop";
    default:
        return "unknown";
    }
}

/**
 * @struct ClientInfo
 * @brief Information about a connected client.
 *
 * Used for connection/disconnection events.
 */
struct ClientInfo {
    int fd; ///< Client socket file descriptor
    std::string session_id; ///< Session identifier (if set)
    std::chrono::system_clock::time_point connected_at; ///< Connection timestamp

    auto operator<=>(const ClientInfo&) const = default;
};

/**
 * @struct EvalEvent
 * @brief Data for code evaluation events.
 *
 * Used for EvalStart events.
 */
struct EvalEvent {
    std::string code_snippet; ///< The code being evaluated
    std::optional<std::string> session_id; ///< Optional session ID
    std::chrono::system_clock::time_point timestamp; ///< Event timestamp
};

/**
 * @struct ErrorEvent
 * @brief Data for error events.
 *
 * Used for EvalError events.
 */
struct ErrorEvent {
    std::string message; ///< Error message
    std::optional<std::string> details; ///< Optional error details
    std::chrono::system_clock::time_point timestamp; ///< Event timestamp
};

/**
 * @struct SymbolEvent
 * @brief Data for symbol definition events.
 *
 * Used for SymbolDefined events.
 */
struct SymbolEvent {
    std::string name; ///< Symbol name
    std::optional<std::string> type; ///< Optional symbol type
    std::optional<uintptr_t> address; ///< Optional symbol address
};

/**
 * @brief Variant type for event data.
 *
 * Associates each EventType with its corresponding data structure.
 */
using EventData = std::variant<
    ClientInfo, ///< For ClientConnected, ClientDisconnected
    EvalEvent, ///< For EvalStart
    std::monostate, ///< For EvalSuccess, ServerStart, ServerStop
    ErrorEvent, ///< For EvalError
    SymbolEvent ///< For SymbolDefined
    >;

/**
 * @struct StreamEvent
 * @brief Represents a published event in the system.
 *
 * Contains the event type, associated data, and timestamp.
 */
struct StreamEvent {
    EventType type; ///< Type of event
    EventData data; ///< Event data
    std::chrono::system_clock::time_point timestamp; ///< Event timestamp

    StreamEvent(EventType t, EventData d = std::monostate {});
};

/**
 * @class Subscription
 * @brief Abstract base class for event subscriptions.
 *
 * Implement this interface to receive events from the EventBus.
 */
class Subscription {
public:
    virtual ~Subscription() = default;
    /**
     * @brief Called when an event is published.
     * @param event The published event
     */
    virtual void on_event(const StreamEvent& event) = 0;
};

/**
 * @concept EventHandler
 * @brief Concept for event handler functions.
 *
 * Any callable that takes a const StreamEvent& and returns void.
 */
template <typename T>
concept EventHandler = requires(T handler, const StreamEvent& event) {
    { handler(event) } -> std::same_as<void>;
};

/**
 * @class EventBus
 * @brief Thread-safe event subscription and publishing system for Lila.
 *
 * The EventBus allows components to subscribe to specific event types and receive notifications
 * when those events are published. Supports both Subscription objects and simple handler functions.
 *
 * ## Usage Example
 * ```cpp
 * // Subscribe with a handler function
 * bus.subscribe(EventType::EvalError, [](const StreamEvent& event) {
 *     // Handle error event
 * });
 *
 * // Subscribe with a Subscription object
 * class MySub : public Lila::Subscription { ... };
 * bus.subscribe(EventType::ClientConnected, std::make_shared<MySub>());
 *
 * // Publish an event
 * bus.publish(StreamEvent{EventType::EvalSuccess});
 * ```
 *
 * Thread safety: All subscription and publishing operations are protected by a mutex.
 */
class EventBus {
private:
    std::unordered_map<EventType, std::vector<std::weak_ptr<Subscription>>> m_subscribers; ///< Subscribers by event type
    std::mutex m_mutex; ///< Mutex for thread safety

public:
    /**
     * @brief Subscribe to an event type with a Subscription object.
     * @param type EventType to subscribe to
     * @param subscriber Shared pointer to Subscription
     */
    void subscribe(EventType type, const std::shared_ptr<Subscription>& subscriber);

    /**
     * @brief Subscribe to an event type with a handler function.
     * @tparam Handler Callable matching EventHandler concept
     * @param type EventType to subscribe to
     * @param handler Handler function
     */
    template <EventHandler Handler>
    void subscribe(EventType type, Handler&& handler)
    {
        struct HandlerSubscription : Subscription {
            Handler m_handler;
            HandlerSubscription(Handler&& h)
                : m_handler(std::forward<Handler>(h))
            {
            }
            void on_event(const StreamEvent& event) override { m_handler(event); }
        };

        subscribe(type, std::make_shared<HandlerSubscription>(std::forward<Handler>(handler)));
    }

    /**
     * @brief Publish an event to all subscribers of its type.
     * @param event The event to publish
     */
    void publish(const StreamEvent& event);
};

} // namespace Lila
