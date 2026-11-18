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
 * auto task = Kriya::message_received(src, [](const Core::NetworkMessage& msg) { });
 * @endcode
 */
MAYAFLUX_API Vruta::Event message_received(
    std::shared_ptr<Vruta::NetworkSource> source,
    std::function<void(const Core::NetworkMessage&)> callback);

/**
 * @brief Creates an Event coroutine that fires only for messages from a specific sender
 * @param source         Shared ownership of the NetworkSource
 * @param sender_address Matched against NetworkMessage::sender_address
 * @param callback       Invoked with each matching message
 * @return Event coroutine suitable for EventManager::add_event()
 *
 * Over a Unix socket a listener reports the same address for every connection it
 * accepts. Use the connection overload to tell those clients apart.
 *
 * @code
 * auto task = Kriya::message_received(src, "192.168.1.10",
 *     [](const Core::NetworkMessage& msg) { });
 * @endcode
 */
MAYAFLUX_API Vruta::Event message_received(
    std::shared_ptr<Vruta::NetworkSource> source,
    std::string sender_address,
    std::function<void(const Core::NetworkMessage&)> callback);

/**
 * @brief Creates an Event coroutine that fires only for messages from one connection
 * @param source        Shared ownership of the NetworkSource
 * @param connection_id NetworkMessage::endpoint_id to match. For a listener this is the id of
 *                      the accepted connection, which the first message from that client reports.
 * @param callback      Invoked with each matching message
 * @return Event coroutine suitable for EventManager::add_event()
 *
 * @code
 * auto task = Kriya::message_received(src, connection_id,
 *     [](const Core::NetworkMessage& msg) { });
 * @endcode
 */
MAYAFLUX_API Vruta::Event message_received(
    std::shared_ptr<Vruta::NetworkSource> source,
    uint64_t connection_id,
    std::function<void(const Core::NetworkMessage&)> callback);

/**
 * @brief Creates an Event coroutine that fires only when a predicate matches
 * @param source    Shared ownership of the NetworkSource
 * @param predicate Returns true for messages that should trigger the callback
 * @param callback  Invoked with each matching message
 * @return Event coroutine suitable for EventManager::add_event()
 *
 * @code
 * auto task = Kriya::message_received(src,
 *     [](const Core::NetworkMessage& m) { return m.data.size() == 4; },
 *     [](const Core::NetworkMessage& msg) { });
 * @endcode
 */
MAYAFLUX_API Vruta::Event message_received(
    std::shared_ptr<Vruta::NetworkSource> source,
    std::function<bool(const Core::NetworkMessage&)> predicate,
    std::function<void(const Core::NetworkMessage&)> callback);

/** @brief Replaced by message_received(source, callback). */
[[deprecated("use message_received")]] MAYAFLUX_API Vruta::Event on_message(
    std::shared_ptr<Vruta::NetworkSource> source,
    std::function<void(const Core::NetworkMessage&)> callback);

/** @brief Replaced by message_received(source, sender_address, callback). */
[[deprecated("use message_received")]] MAYAFLUX_API Vruta::Event on_message_from(
    std::shared_ptr<Vruta::NetworkSource> source,
    std::string sender_address,
    std::function<void(const Core::NetworkMessage&)> callback);

/** @brief Replaced by message_received(source, predicate, callback). */
[[deprecated("use message_received")]] MAYAFLUX_API Vruta::Event on_message_matching(
    std::shared_ptr<Vruta::NetworkSource> source,
    std::function<bool(const Core::NetworkMessage&)> predicate,
    std::function<void(const Core::NetworkMessage&)> callback);

} // namespace MayaFlux::Kriya
