#pragma once

#include "NetworkUtils.hpp"

namespace MayaFlux::Registry::Service {
struct NetworkService;
}

namespace MayaFlux::Portal::Network {

/**
 * @class NetworkSink
 * @brief Owned handle for sending data through a network stream.
 *
 * NetworkSink is the send-side counterpart to Vruta::NetworkSource.
 * It owns one endpoint opened via NetworkService and exposes send
 * and send_to as the only public interface.
 *
 * Lifetime matches the stream: the endpoint is opened on construction
 * and closed on destruction. Move-only. A sink made with to_connection
 * borrows an existing connection instead and never closes it.
 *
 * Consumers hold a shared_ptr<NetworkSink>. A metro coroutine,
 * a node output callback, or a future Portal::Network::PacketFlow
 * all hold the same handle. The underlying transport is hidden behind
 * NetworkFoundry — callers never touch NetworkService directly.
 *
 * Today NetworkFoundry is a thin wrapper over NetworkService. When
 * WebRTC or QUIC arrives, only NetworkFoundry changes.
 *
 * @code
 * auto sink = std::make_shared<Portal::Network::NetworkSink>(
 *     StreamConfig{
 *         .name      = "osc_out",
 *         .endpoint  = { .address = "127.0.0.1", .port = 9000 },
 *         .profile   = StreamProfile::CONTROL,
 *         .transport = NetworkTransportHint::UDP,
 *     });
 *
 * sink->send(payload);
 * sink->send_to(payload, "192.168.1.5", 9001);
 * @endcode
 */
class MAYAFLUX_API NetworkSink {
public:
    /**
     * @brief Open a send endpoint from a StreamConfig.
     * @param config Stream configuration.
     *
     * Resolves the transport hint, opens the endpoint via NetworkService,
     * and logs a warning if the service is unavailable.
     */
    explicit NetworkSink(const StreamConfig& config);

    /**
     * @brief A sink over a connection that already exists, which it does not own.
     * @param connection_id Endpoint id of the connection, usually NetworkMessage::endpoint_id
     *                      of a message a listener received.
     * @param name          Optional name for the sink.
     *
     * For replying to a client of a listener, or sending to it later. The sink only
     * sends: destroying it leaves the connection open, and the connection ending leaves
     * the sink valid but send returns false. The framing is the connection's own.
     * is_open reports that a connection id and the service are held, not that the
     * peer is still there. Send is the test of that.
     *
     * @code
     * on_message_received(source, [](const Core::NetworkMessage& msg) {
     *     auto reply = Portal::Network::NetworkSink::to_connection(msg.endpoint_id);
     *     (void)reply.send(answer);
     * });
     * @endcode
     */
    [[nodiscard]] static NetworkSink to_connection(uint64_t connection_id, std::string name = {});

    /**
     * @brief Close the endpoint and release resources, unless the sink does not own it.
     */
    ~NetworkSink();

    NetworkSink(const NetworkSink&) = delete;
    NetworkSink& operator=(const NetworkSink&) = delete;
    NetworkSink(NetworkSink&&) noexcept;
    NetworkSink& operator=(NetworkSink&&) noexcept;

    /**
     * @brief Send bytes through this sink's endpoint.
     * @param data Read-only byte view of the payload.
     * @return true if the send was accepted by the backend.
     *
     * For UDP: non-blocking sendto() to the address in StreamConfig::endpoint.
     * For TCP and UDS: writes a framed message once the connection is open,
     * and returns false while it is still connecting or after it failed.
     */
    [[nodiscard]] bool send(ByteView data) const;

    /**
     * @brief Send bytes to an explicit address, overriding the config target.
     * @param data    Read-only byte view of the payload.
     * @param address Target IP address string.
     * @param port    Target port.
     * @return true if the send was accepted.
     *
     * Primary use: UDP fan-out to a different peer than the default target.
     * TCP and UDS ignore address/port and use the connected peer.
     */
    [[nodiscard]] bool send_to(ByteView data, const std::string& address, uint16_t port) const;

    /**
     * @brief Bytes accepted by send and not yet written to the peer.
     * @return 0 when the sink is not open or the transport does not queue.
     *
     * A control rate writer can skip a write while this is high.
     */
    [[nodiscard]] size_t write_backlog() const;

    /**
     * @brief Set the DTR and RTS lines of a serial stream.
     * @param dtr New DTR level, or nothing to leave it.
     * @param rts New RTS level, or nothing to leave it.
     * @return false unless this is an open serial stream.
     */
    bool set_control_lines(std::optional<bool> dtr, std::optional<bool> rts) const;

    /**
     * @brief Hold the transmit line of a serial stream in break.
     * @return false unless this is an open serial stream.
     */
    bool send_break() const;

    /**
     * @brief Return true if the endpoint was opened successfully.
     */
    [[nodiscard]] bool is_open() const { return m_endpoint_id != 0; }

    /**
     * @brief Return the stream name from the config.
     */
    [[nodiscard]] const std::string& name() const { return m_name; }

private:
    struct Borrowed { };

    NetworkSink(Borrowed, uint64_t connection_id, std::string name);

    std::string m_name;
    uint64_t m_endpoint_id { 0 };
    bool m_owns { true };

    Registry::Service::NetworkService* m_service { nullptr };
};

} // namespace MayaFlux::Portal::Network
