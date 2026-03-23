#pragma once

#include "MayaFlux/Core/GlobalNetworkConfig.hpp"

namespace MayaFlux::Core {
struct NetworkMessage;
}

namespace MayaFlux::Vruta {
class NetworkSource;
class Event;
}

namespace MayaFlux::Kriya {

/**
 * @brief Creates an Event coroutine that fires on every message received by a source
 * @param source   Shared ownership of the NetworkSource; lifetime is tied to the coroutine frame
 * @param callback Invoked with each received message
 * @return Event coroutine suitable for EventManager::add_event()
 *
 * @code
 * auto src = std::make_shared<Vruta::NetworkSource>(
 *     Core::EndpointInfo{ .transport = Core::NetworkTransport::UDP, .local_port = 8000 });
 * auto task = Kriya::on_message(src, [](const Core::NetworkMessage& msg) { });
 * @endcode
 */
MAYAFLUX_API Vruta::Event on_message(
    std::shared_ptr<Vruta::NetworkSource> source,
    std::function<void(const Core::NetworkMessage&)> callback);

/**
 * @brief Creates an Event coroutine that fires only for messages from a specific sender
 * @param source         Shared ownership of the NetworkSource
 * @param sender_address IP address string matched against NetworkMessage::sender_address
 * @param callback       Invoked with each matching message
 * @return Event coroutine suitable for EventManager::add_event()
 *
 * @code
 * auto task = Kriya::on_message_from(src, "192.168.1.10",
 *     [](const Core::NetworkMessage& msg) { });
 * @endcode
 */
MAYAFLUX_API Vruta::Event on_message_from(
    std::shared_ptr<Vruta::NetworkSource> source,
    std::string sender_address,
    std::function<void(const Core::NetworkMessage&)> callback);

/**
 * @brief Creates an Event coroutine that fires only when a predicate matches
 * @param source    Shared ownership of the NetworkSource
 * @param predicate Returns true for messages that should trigger the callback
 * @param callback  Invoked with each matching message
 * @return Event coroutine suitable for EventManager::add_event()
 *
 * @code
 * auto task = Kriya::on_message_matching(src,
 *     [](const Core::NetworkMessage& m) { return m.data.size() == 4; },
 *     [](const Core::NetworkMessage& msg) { });
 * @endcode
 */
MAYAFLUX_API Vruta::Event on_message_matching(
    std::shared_ptr<Vruta::NetworkSource> source,
    std::function<bool(const Core::NetworkMessage&)> predicate,
    std::function<void(const Core::NetworkMessage&)> callback);

} // namespace MayaFlux::Kriya
