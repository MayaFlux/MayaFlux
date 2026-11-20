#pragma once

#include "SerialLink.hpp"

namespace MayaFlux::Registry::Service {
struct NetworkService;
}

namespace MayaFlux::Core {

/**
 * @class NetworkSerialLink
 * @brief An ISerialLink over a SERIAL endpoint of the NetworkService.
 *
 * The endpoint is opened with framing NONE, so every chunk the port yields reaches the
 * chunk callback as it is. Reconnection, settle time and line control are the endpoint's,
 * driven by the SerialLineSettings given to open(). The state callback also reports the
 * endpoint's state at the moment open() returns, so a transition that happened before
 * the callbacks were registered is not lost.
 *
 * Without a service, open() fails.
 */
class MAYAFLUX_API NetworkSerialLink : public ISerialLink {
public:
    /**
     * @param service The registered NetworkService. Not owned, and it must outlive the link.
     */
    explicit NetworkSerialLink(Registry::Service::NetworkService* service);
    ~NetworkSerialLink() override;

    NetworkSerialLink(const NetworkSerialLink&) = delete;
    NetworkSerialLink& operator=(const NetworkSerialLink&) = delete;
    NetworkSerialLink(NetworkSerialLink&&) = delete;
    NetworkSerialLink& operator=(NetworkSerialLink&&) = delete;

    bool open(const std::string& path, const SerialLineSettings& line) override;
    void close() override;
    [[nodiscard]] EndpointState state() const override;
    bool write(std::span<const uint8_t> bytes) override;
    [[nodiscard]] size_t backlog() const override;
    bool set_control_lines(std::optional<bool> dtr, std::optional<bool> rts) override;
    bool send_break() override;
    TimerToken post_after(std::chrono::milliseconds delay, std::function<void()> callback) override;
    void cancel_post(TimerToken token) override;
    void set_chunk_callback(ChunkCallback callback) override;
    void set_state_callback(StateCallback callback) override;

private:
    struct Shared;

    Registry::Service::NetworkService* m_service;
    uint64_t m_endpoint { 0 };
    ChunkCallback m_chunk;
    StateCallback m_state;
    std::shared_ptr<Shared> m_shared;
};

} // namespace MayaFlux::Core
