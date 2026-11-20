#pragma once

#include "MayaFlux/Core/Input/SerialSpec.hpp"

namespace MayaFlux::Vruta {
class NetworkSource;
}

namespace MayaFlux::Portal::Network {

/**
 * @brief Read a serial device as network messages, framed the way @p framing says.
 * @param device_id Id of a serial device from open_serial_port().
 * @param framing Only its framing, terminator, gap and message limit are used. Every message is
 *                delivered as bytes whatever its parser says.
 * @return A source fed with one NetworkMessage per framed message, or nothing when serial
 *         input is not enabled or the device is unknown.
 *
 * The source reads the same bytes as the device and as every other reader, cut by its own
 * framing, so the device's own reading and the nodes bound to it are unaffected. The message
 * carries the device id as endpoint_id and no sender address. Hand it to on_message_received()
 * or await it like any other NetworkSource. Dropping the last reference stops the reading.
 *
 * @code
 * auto board = open_serial_port(port);
 * auto frames = Portal::Network::serial_source(board->id, Core::SerialSpec::cobs());
 * on_message_received(frames, [](const Core::NetworkMessage& message) { });
 * @endcode
 */
MAYAFLUX_API std::shared_ptr<Vruta::NetworkSource> serial_source(uint32_t device_id, const Core::SerialSpec& framing);

} // namespace MayaFlux::Portal::Network
