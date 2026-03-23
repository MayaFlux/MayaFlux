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
 * and closed on destruction. Move-only.
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
     * @brief Close the endpoint and release resources.
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
     * For TCP: writes a framed message; may block briefly on backpressure.
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
     * TCP ignores address/port and uses the connected peer.
     */
    [[nodiscard]] bool send_to(ByteView data, const std::string& address, uint16_t port) const;

    /**
     * @brief Return true if the endpoint was opened successfully.
     */
    [[nodiscard]] bool is_open() const { return m_endpoint_id != 0; }

    /**
     * @brief Return the stream name from the config.
     */
    [[nodiscard]] const std::string& name() const { return m_name; }

private:
    std::string m_name;
    uint64_t m_endpoint_id { 0 };

    Registry::Service::NetworkService* m_service { nullptr };
};

} // namespace MayaFlux::Portal::Network
