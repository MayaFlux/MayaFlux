#include "NetworkEvents.hpp"

#include "Awaiters/GetPromise.hpp"
#include "Awaiters/NetworkAwaiter.hpp"

#include "MayaFlux/Vruta/Event.hpp"

namespace MayaFlux::Kriya {

Vruta::Event on_message(
    std::shared_ptr<Vruta::NetworkSource> source,
    std::function<void(const Core::NetworkMessage&)> callback)
{
    auto& promise = co_await GetEventPromise { source };

    while (true) {
        if (promise.should_terminate) {
            break;
        }

        auto msg = co_await source->next_message();
        callback(msg);
    }
}

Vruta::Event on_message_from(
    std::shared_ptr<Vruta::NetworkSource> source,
    std::string sender_address,
    std::function<void(const Core::NetworkMessage&)> callback)
{
    auto& promise = co_await GetEventPromise { source };

    while (true) {
        if (promise.should_terminate) {
            break;
        }

        auto msg = co_await source->next_message();

        if (msg.sender_address == sender_address) {
            callback(msg);
        }
    }
}

Vruta::Event on_message_matching(
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

        if (predicate(msg)) {
            callback(msg);
        }
    }
}

} // namespace MayaFlux::Kriya
