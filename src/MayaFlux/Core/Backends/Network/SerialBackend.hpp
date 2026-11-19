#pragma once

#include "StreamBackend.hpp"

#include <asio/serial_port.hpp>

namespace MayaFlux::Core {

/**
 * @struct SerialTransport
 * @brief StreamBackend policy for serial ports
 *
 * An endpoint opens the device named by remote_path with the line setup in
 * EndpointInfo::serial. The port opens synchronously, so open_endpoint returns 0
 * for a port that cannot be opened, unless the backend reconnects, in which case
 * the endpoint waits in RECONNECTING until the device appears. AUTO framing is
 * NONE: each read is delivered as one message and a send writes its payload as
 * it is.
 *
 * Baud rates outside asio's table go through the platform: termios2 on Linux and
 * IOSSIOSPEED on macOS, and Windows accepts any value. DTR and RTS are driven
 * after the open, which asserts both, so a board that restarts on DTR sees the
 * open itself. The endpoint then stays OPENING for settle_ms before any read, and
 * input received before it opens is discarded.
 */
struct SerialTransport {
    using Stream = asio::serial_port;
    using Config = SerialLinkInfo;

    static constexpr bool listens = false;
    static constexpr FramingPolicy automatic_framing = FramingPolicy::NONE;
    static constexpr NetworkTransport id = NetworkTransport::SERIAL;
    static constexpr std::string_view label = "Serial";
    static constexpr std::string_view name = "Serial (Asio)";

    static asio::error_code open(Stream& port, const EndpointInfo& info);
    static std::chrono::milliseconds settle_time(const EndpointInfo& info);
    static void ready(Stream& port, const EndpointInfo& info);
    static bool set_control_lines(Stream& port, std::optional<bool> dtr, std::optional<bool> rts);
    static bool send_break(Stream& port);
    static std::string describe(const EndpointInfo& info);
    static std::string peer_label(const EndpointInfo& info);
};

/**
 * @class SerialBackend
 * @brief Byte stream transport over serial ports via standalone Asio
 *
 * One serial device path is one endpoint, with the connection model, outbox,
 * reconnection and state callbacks of StreamBackend, plus DTR, RTS and break
 * through INetworkBackend. See SerialTransport for how a port is opened.
 *
 * @code
 * EndpointInfo port;
 * port.transport = NetworkTransport::SERIAL;
 * port.remote_path = "/dev/ttyACM0";
 * port.serial.baud_rate = 31250;
 * port.serial.dtr_on_open = false;
 * auto id = serial.open_endpoint(port);
 * @endcode
 */
using SerialBackend = StreamBackend<SerialTransport>;

#ifdef MAYAFLUX_EXPORTS
extern template class StreamBackend<SerialTransport>;
#else
extern template class MAYAFLUX_API StreamBackend<SerialTransport>;
#endif

} // namespace MayaFlux::Core
