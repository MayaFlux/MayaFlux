#pragma once

namespace Lila {

enum class EventType : uint8_t {
    ClientConnected,
    ClientDisconnected,
    EvalStart,
    EvalSuccess,
    EvalError,
    SymbolDefined,
    ServerStart,
    ServerStop
};

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

struct ClientInfo {
    int fd;
    std::string session_id;
    std::chrono::system_clock::time_point connected_at;

    auto operator<=>(const ClientInfo&) const = default;
};

struct EvalEvent {
    std::string code_snippet;
    std::optional<std::string> session_id;
    std::chrono::system_clock::time_point timestamp;
};

struct ErrorEvent {
    std::string message;
    std::optional<std::string> details;
    std::chrono::system_clock::time_point timestamp;
};

struct SymbolEvent {
    std::string name;
    std::optional<std::string> type;
    std::optional<uintptr_t> address;
};

using EventData = std::variant<
    ClientInfo, ///< ClientConnected, ClientDisconnected
    EvalEvent, ///< EvalStart
    std::monostate, ///< EvalSuccess, ServerStart, ServerStop
    ErrorEvent, ///< EvalError
    SymbolEvent ///< SymbolDefined
    >;

struct Event {
    EventType type;
    EventData data;
    std::chrono::system_clock::time_point timestamp;

    Event(EventType t, EventData d = std::monostate {});
};

class Subscription {
public:
    virtual ~Subscription() = default;
    virtual void on_event(const Event& event) = 0;
};

template <typename T>
concept EventHandler = requires(T handler, const Event& event) {
    { handler(event) } -> std::same_as<void>;
};

class EventBus {
private:
    std::unordered_map<EventType, std::vector<std::weak_ptr<Subscription>>> m_subscribers;
    std::mutex m_mutex;

public:
    void subscribe(EventType type, const std::shared_ptr<Subscription>& subscriber);

    template <EventHandler Handler>
    void subscribe(EventType type, Handler&& handler)
    {
        struct HandlerSubscription : Subscription {
            Handler m_handler;
            HandlerSubscription(Handler&& h)
                : m_handler(std::forward<Handler>(h))
            {
            }
            void on_event(const Event& event) override { m_handler(event); }
        };

        subscribe(type, std::make_shared<HandlerSubscription>(std::forward<Handler>(handler)));
    }

    void publish(const Event& event);
};

} // namespace Lila
