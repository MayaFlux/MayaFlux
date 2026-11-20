#include "NetworkEvents.hpp"

#include "Awaiters/GetPromise.hpp"
#include "Awaiters/NetworkAwaiter.hpp"

#include "MayaFlux/Vruta/Event.hpp"

namespace MayaFlux::Kriya {

Vruta::Event message_received(
    std::shared_ptr<Vruta::NetworkSource> source,
    std::function<void(const Core::NetworkMessage&)> callback)
{
    auto& promise = co_await GetEventPromise { source };

    while (true) {
        if (promise.should_terminate) {
            break;
        }

        auto msg = co_await source->next_message();
        if (promise.should_terminate)
            break;
        callback(msg);
    }
}

Vruta::Event message_received(
    std::shared_ptr<Vruta::NetworkSource> source,
    std::function<bool(const Core::NetworkMessage&)> predicate,
    std::function<void(const Core::NetworkMessage&)> callback)
{
    auto& promise = co_await GetEventPromise { source };

    while (true) {
        if (promise.should_terminate) {
            break;
        }

        auto msg = co_await source->next_message();
        if (promise.should_terminate)
            break;

        if (predicate(msg)) {
            callback(msg);
        }
    }
}

Vruta::Event message_received(
    std::shared_ptr<Vruta::NetworkSource> source,
    std::string sender_address,
    std::function<void(const Core::NetworkMessage&)> callback)
{
    return message_received(
        std::move(source),
        std::function<bool(const Core::NetworkMessage&)>(
            [sender = std::move(sender_address)](const Core::NetworkMessage& msg) {
                return msg.sender_address == sender;
            }),
        std::move(callback));
}

Vruta::Event message_received(
    std::shared_ptr<Vruta::NetworkSource> source,
    uint64_t connection_id,
    std::function<void(const Core::NetworkMessage&)> callback)
{
    return message_received(
        std::move(source),
        std::function<bool(const Core::NetworkMessage&)>(
            [connection_id](const Core::NetworkMessage& msg) {
                return msg.endpoint_id == connection_id;
            }),
        std::move(callback));
}

} // namespace MayaFlux::Kriya
