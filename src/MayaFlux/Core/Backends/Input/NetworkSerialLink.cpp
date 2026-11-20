#include "NetworkSerialLink.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Registry/Service/NetworkService.hpp"

namespace MayaFlux::Core {

struct NetworkSerialLink::Shared {
    ChunkCallback chunk;
    StateCallback state;
    std::atomic<EndpointState> reported { EndpointState::CLOSED };

    void report(EndpointState current)
    {
        const EndpointState previous = reported.exchange(current);
        if (previous != current && state) {
            state(previous, current);
        }
    }
};

NetworkSerialLink::NetworkSerialLink(Registry::Service::NetworkService* service)
    : m_service(service)
{
}

NetworkSerialLink::~NetworkSerialLink()
{
    close();
}

bool NetworkSerialLink::open(const std::string& path, const SerialLineSettings& line)
{
    if (!m_service || !m_service->open_endpoint || !m_service->set_endpoint_receive_callback) {
        MF_WARN(Journal::Component::Core, Journal::Context::InputBackend,
            "Serial link {} needs a NetworkService", path);
        return false;
    }

    close();

    auto shared = std::make_shared<Shared>();
    shared->chunk = m_chunk;
    shared->state = m_state;

    EndpointInfo info;
    info.transport = NetworkTransport::SERIAL;
    info.role = EndpointRole::BIDIRECTIONAL;
    info.framing = FramingPolicy::NONE;
    info.remote_path = path;
    info.label = path;
    info.serial = line;

    const uint64_t endpoint = m_service->open_endpoint(info);
    if (endpoint == 0) {
        MF_ERROR(Journal::Component::Core, Journal::Context::InputBackend,
            "Serial link could not open {}: the port must exist, or the serial transport must be enabled in GlobalNetworkConfig and set to reconnect",
            path);
        return false;
    }

    m_endpoint = endpoint;
    m_shared = shared;
    const std::weak_ptr<Shared> weak = shared;

    m_service->set_endpoint_receive_callback(endpoint,
        [weak](uint64_t, const uint8_t* data, size_t size, std::string_view) {
            if (const auto held = weak.lock(); held && held->chunk) {
                held->chunk(std::span<const uint8_t> { data, size });
            }
        });

    if (m_service->set_endpoint_state_callback) {
        m_service->set_endpoint_state_callback(endpoint,
            [weak](uint64_t, EndpointState, EndpointState current) {
                if (const auto held = weak.lock()) {
                    held->report(current);
                }
            });
    }

    if (m_service->get_endpoint_state) {
        shared->report(m_service->get_endpoint_state(endpoint));
    }

    return true;
}

void NetworkSerialLink::close()
{
    if (m_endpoint != 0 && m_service && m_service->close_endpoint) {
        m_service->close_endpoint(m_endpoint);
    }
    m_endpoint = 0;
    m_shared.reset();
}

EndpointState NetworkSerialLink::state() const
{
    if (m_endpoint == 0 || !m_service || !m_service->get_endpoint_state) {
        return EndpointState::CLOSED;
    }
    return m_service->get_endpoint_state(m_endpoint);
}

bool NetworkSerialLink::write(std::span<const uint8_t> bytes)
{
    if (m_endpoint == 0 || !m_service || !m_service->send) {
        return false;
    }
    return m_service->send(m_endpoint, bytes.data(), bytes.size());
}

size_t NetworkSerialLink::backlog() const
{
    if (m_endpoint == 0 || !m_service || !m_service->get_write_backlog) {
        return 0;
    }
    return m_service->get_write_backlog(m_endpoint);
}

bool NetworkSerialLink::set_control_lines(std::optional<bool> dtr, std::optional<bool> rts)
{
    if (m_endpoint == 0 || !m_service || !m_service->set_control_lines) {
        return false;
    }
    return m_service->set_control_lines(m_endpoint, dtr, rts);
}

bool NetworkSerialLink::send_break()
{
    if (m_endpoint == 0 || !m_service || !m_service->send_break) {
        return false;
    }
    return m_service->send_break(m_endpoint);
}

ISerialLink::TimerToken NetworkSerialLink::post_after(std::chrono::milliseconds delay, std::function<void()> callback)
{
    if (!m_service || !m_service->post_after) {
        return 0;
    }
    return m_service->post_after(delay, std::move(callback));
}

void NetworkSerialLink::cancel_post(TimerToken token)
{
    if (token != 0 && m_service && m_service->cancel_post) {
        m_service->cancel_post(token);
    }
}

void NetworkSerialLink::set_chunk_callback(ChunkCallback callback)
{
    m_chunk = std::move(callback);
}

void NetworkSerialLink::set_state_callback(StateCallback callback)
{
    m_state = std::move(callback);
}

} // namespace MayaFlux::Core
