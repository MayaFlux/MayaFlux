#include "NetworkSink.hpp"

#include "MayaFlux/Core/GlobalNetworkConfig.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/NetworkService.hpp"

namespace MayaFlux::Portal::Network {

namespace {

    Core::NetworkTransport resolve_transport(NetworkTransportHint hint, StreamProfile profile)
    {
        switch (hint) {
        case NetworkTransportHint::UDP:
            return Core::NetworkTransport::UDP;
        case NetworkTransportHint::TCP:
            return Core::NetworkTransport::TCP;
        case NetworkTransportHint::SHARED_MEMORY:
            return Core::NetworkTransport::SHARED_MEMORY;
        case NetworkTransportHint::UDS:
            return Core::NetworkTransport::UDS;
        case NetworkTransportHint::SERIAL:
            return Core::NetworkTransport::SERIAL;
        case NetworkTransportHint::AUTO:
            switch (profile) {
            case StreamProfile::ORDERED_BULK:
                return Core::NetworkTransport::TCP;
            default:
                return Core::NetworkTransport::UDP;
            }
        }
        return Core::NetworkTransport::UDP;
    }

} // namespace

NetworkSink::NetworkSink(const StreamConfig& config)
    : m_name(config.name)
{
    auto* svc = Registry::BackendRegistry::instance()
                    .get_service<Registry::Service::NetworkService>();

    if (!svc) {
        MF_WARN(Journal::Component::Portal, Journal::Context::Networking,
            "NetworkSink '{}': NetworkService not available", m_name);
        return;
    }

    m_service = svc;

    Core::EndpointInfo info;
    info.transport = resolve_transport(config.transport, config.profile);
    info.role = Core::EndpointRole::SEND;
    info.label = config.name;
    info.framing = config.framing;
    info.max_message_size = config.max_message_size;
    info.serial = config.serial;

    if (info.transport == Core::NetworkTransport::UDS || info.transport == Core::NetworkTransport::SERIAL) {
        if (config.endpoint.path.empty()) {
            MF_WARN(Journal::Component::Portal, Journal::Context::Networking,
                "NetworkSink '{}': a path is needed for this transport", m_name);
            return;
        }
        info.remote_path = config.endpoint.path;
    } else {
        info.remote_address = config.endpoint.address;
        info.remote_port = config.endpoint.port;
        info.local_port = config.endpoint.local_port;
    }

    m_endpoint_id = svc->open_endpoint(info);

    if (m_endpoint_id == 0) {
        MF_WARN(Journal::Component::Portal, Journal::Context::Networking,
            "NetworkSink '{}': endpoint open failed", m_name);
        return;
    }

    MF_DEBUG(Journal::Component::Portal, Journal::Context::Networking,
        "NetworkSink '{}' opened endpoint {}", m_name, m_endpoint_id);
}

NetworkSink::NetworkSink(Borrowed, uint64_t connection_id, std::string name)
    : m_name(std::move(name))
    , m_owns(false)
{
    m_service = Registry::BackendRegistry::instance()
                    .get_service<Registry::Service::NetworkService>();

    if (!m_service) {
        MF_WARN(Journal::Component::Portal, Journal::Context::Networking,
            "NetworkSink '{}': NetworkService not available", m_name);
        return;
    }

    m_endpoint_id = connection_id;
}

NetworkSink NetworkSink::to_connection(uint64_t connection_id, std::string name)
{
    return NetworkSink(Borrowed {}, connection_id, std::move(name));
}

NetworkSink::~NetworkSink()
{
    if (!m_owns || m_endpoint_id == 0 || !m_service) {
        return;
    }
    m_service->close_endpoint(m_endpoint_id);
}

NetworkSink::NetworkSink(NetworkSink&& other) noexcept
    : m_name(std::move(other.m_name))
    , m_endpoint_id(other.m_endpoint_id)
    , m_owns(other.m_owns)
    , m_service(other.m_service)
{
    other.m_endpoint_id = 0;
    other.m_service = nullptr;
}

NetworkSink& NetworkSink::operator=(NetworkSink&& other) noexcept
{
    if (this != &other) {
        if (m_owns && m_endpoint_id != 0 && m_service) {
            m_service->close_endpoint(m_endpoint_id);
        }
        m_name = std::move(other.m_name);
        m_endpoint_id = other.m_endpoint_id;
        m_owns = other.m_owns;
        m_service = other.m_service;
        other.m_endpoint_id = 0;
        other.m_service = nullptr;
    }
    return *this;
}

bool NetworkSink::send(ByteView data) const
{
    if (m_endpoint_id == 0 || !m_service) {
        return false;
    }
    return m_service->send(m_endpoint_id, data.data(), data.size());
}

bool NetworkSink::send_to(ByteView data, const std::string& address, uint16_t port) const
{
    if (m_endpoint_id == 0 || !m_service) {
        return false;
    }
    return m_service->send_to(m_endpoint_id, data.data(), data.size(), address, port);
}

size_t NetworkSink::write_backlog() const
{
    if (m_endpoint_id == 0 || !m_service || !m_service->get_write_backlog) {
        return 0;
    }
    return m_service->get_write_backlog(m_endpoint_id);
}

bool NetworkSink::set_control_lines(std::optional<bool> dtr, std::optional<bool> rts) const
{
    if (m_endpoint_id == 0 || !m_service || !m_service->set_control_lines) {
        return false;
    }
    return m_service->set_control_lines(m_endpoint_id, dtr, rts);
}

bool NetworkSink::send_break() const
{
    if (m_endpoint_id == 0 || !m_service || !m_service->send_break) {
        return false;
    }
    return m_service->send_break(m_endpoint_id);
}

} // namespace MayaFlux::Portal::Network
