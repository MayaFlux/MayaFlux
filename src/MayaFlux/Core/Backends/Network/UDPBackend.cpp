#include "UDPBackend.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Core {

UDPBackend::UDPBackend(const UDPBackendInfo& config, asio::io_context& context)
    : m_config(config)
    , m_context(context)
{
}

UDPBackend::~UDPBackend()
{
    shutdown();
}

// ─────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

bool UDPBackend::initialize()
{
    if (m_initialized.load()) {
        return true;
    }

    m_initialized.store(true);

    MF_INFO(Journal::Component::Core, Journal::Context::Init,
        "UDP backend initialized");

    return true;
}

void UDPBackend::start()
{
    if (m_running.load()) {
        return;
    }

    m_running.store(true);

    MF_DEBUG(Journal::Component::Core, Journal::Context::Init,
        "UDP backend started");
}

void UDPBackend::stop()
{
    if (!m_running.load()) {
        return;
    }

    m_running.store(false);

    MF_DEBUG(Journal::Component::Core, Journal::Context::Shutdown,
        "UDP backend stopped");
}

void UDPBackend::shutdown()
{
    stop();

    {
        std::unique_lock lock(m_endpoints_mutex);
        m_endpoints.clear();
    }

    {
        std::unique_lock lock(m_sockets_mutex);
        for (auto& [port, state] : m_sockets) {
            asio::error_code ec;
            if (state->socket.close(ec)) {
                MF_WARN(Journal::Component::Core, Journal::Context::Networking,
                    "Error closing UDP socket on port {}: {}", port, ec.message());
            }
        }
        m_sockets.clear();
    }

    m_initialized.store(false);

    MF_INFO(Journal::Component::Core, Journal::Context::Shutdown,
        "UDP backend shutdown");
}

// ─────────────────────────────────────────────────────────────────────────────
// Endpoint management
// ─────────────────────────────────────────────────────────────────────────────

uint64_t UDPBackend::open_endpoint(const EndpointInfo& info)
{
    uint16_t local_port = info.local_port;

    if (local_port == 0 && info.role == EndpointRole::SEND) {
        local_port = 0;
    } else if (local_port == 0) {
        local_port = m_config.default_receive_port;
    }

    auto* sock = acquire_socket(local_port);
    if (!sock) {
        return 0;
    }

    EndpointRecord record;
    record.info = info;
    record.socket_state = sock;

    if (!info.remote_address.empty() && info.remote_port > 0) {
        asio::error_code ec;
        auto addr = asio::ip::make_address(info.remote_address, ec);
        if (ec) {
            MF_ERROR(Journal::Component::Core, Journal::Context::Networking,
                "Invalid remote address '{}': {}", info.remote_address, ec.message());
            release_socket(local_port);
            return 0;
        }
        record.default_remote = asio::ip::udp::endpoint(addr, info.remote_port);
        record.has_default_remote = true;
    }

    record.info.state = EndpointState::OPEN;

    {
        std::unique_lock lock(m_endpoints_mutex);
        m_endpoints[info.id] = std::move(record);
    }

    transition_state(m_endpoints[info.id], EndpointState::OPEN);

    MF_INFO(Journal::Component::Core, Journal::Context::Networking,
        "UDP endpoint {} opened (local:{}, remote:{}:{})",
        info.id, local_port, info.remote_address, info.remote_port);

    return info.id;
}

void UDPBackend::close_endpoint(uint64_t endpoint_id)
{
    std::unique_lock lock(m_endpoints_mutex);
    auto it = m_endpoints.find(endpoint_id);
    if (it == m_endpoints.end()) {
        return;
    }

    uint16_t local_port = it->second.socket_state->local_port;

    EndpointState prev = it->second.info.state;
    it->second.info.state = EndpointState::CLOSED;

    m_endpoints.erase(it);
    lock.unlock();

    release_socket(local_port);

    MF_DEBUG(Journal::Component::Core, Journal::Context::Networking,
        "UDP endpoint {} closed", endpoint_id);
}

EndpointState UDPBackend::get_endpoint_state(uint64_t endpoint_id) const
{
    std::shared_lock lock(m_endpoints_mutex);
    auto it = m_endpoints.find(endpoint_id);
    if (it == m_endpoints.end()) {
        return EndpointState::CLOSED;
    }
    return it->second.info.state;
}

std::vector<EndpointInfo> UDPBackend::get_endpoints() const
{
    std::shared_lock lock(m_endpoints_mutex);
    std::vector<EndpointInfo> result;
    result.reserve(m_endpoints.size());
    for (const auto& [id, record] : m_endpoints) {
        result.push_back(record.info);
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Data transfer
// ─────────────────────────────────────────────────────────────────────────────

bool UDPBackend::send(uint64_t endpoint_id, const uint8_t* data, size_t size)
{
    std::shared_lock lock(m_endpoints_mutex);
    auto it = m_endpoints.find(endpoint_id);
    if (it == m_endpoints.end()) {
        return false;
    }

    auto& record = it->second;
    if (!record.has_default_remote) {
        MF_WARN(Journal::Component::Core, Journal::Context::Networking,
            "UDP endpoint {} has no default remote target", endpoint_id);
        return false;
    }

    auto buf = std::make_shared<std::vector<uint8_t>>(data, data + size);
    auto remote = record.default_remote;
    auto& socket = record.socket_state->socket;

    socket.async_send_to(
        asio::buffer(*buf), remote,
        [buf, endpoint_id](const asio::error_code& ec, size_t /*bytes_sent*/) {
            if (ec) {
                MF_WARN(Journal::Component::Core, Journal::Context::Networking,
                    "UDP send failed on endpoint {}: {}", endpoint_id, ec.message());
            }
        });

    return true;
}

bool UDPBackend::send_to(uint64_t endpoint_id, const uint8_t* data, size_t size,
    const std::string& address, uint16_t port)
{
    std::shared_lock lock(m_endpoints_mutex);
    auto it = m_endpoints.find(endpoint_id);
    if (it == m_endpoints.end()) {
        return false;
    }

    asio::error_code ec;
    auto addr = asio::ip::make_address(address, ec);
    if (ec) {
        MF_WARN(Journal::Component::Core, Journal::Context::Networking,
            "Invalid send_to address '{}': {}", address, ec.message());
        return false;
    }

    asio::ip::udp::endpoint target(addr, port);
    auto buf = std::make_shared<std::vector<uint8_t>>(data, data + size);
    auto& socket = it->second.socket_state->socket;

    socket.async_send_to(
        asio::buffer(*buf), target,
        [buf, endpoint_id](const asio::error_code& send_ec, size_t) {
            if (send_ec) {
                MF_WARN(Journal::Component::Core, Journal::Context::Networking,
                    "UDP send_to failed on endpoint {}: {}", endpoint_id, send_ec.message());
            }
        });

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Callbacks
// ─────────────────────────────────────────────────────────────────────────────

void UDPBackend::set_receive_callback(NetworkReceiveCallback callback)
{
    m_receive_callback = std::move(callback);
}

void UDPBackend::set_state_callback(EndpointStateCallback callback)
{
    m_state_callback = std::move(callback);
}

// ─────────────────────────────────────────────────────────────────────────────
// Private: socket management
// ─────────────────────────────────────────────────────────────────────────────

UDPBackend::SocketState* UDPBackend::acquire_socket(uint16_t local_port)
{
    std::unique_lock lock(m_sockets_mutex);

    auto it = m_sockets.find(local_port);
    if (it != m_sockets.end()) {
        it->second->ref_count++;
        return it->second.get();
    }

    auto state = std::make_unique<SocketState>(m_context);
    state->local_port = local_port;
    state->ref_count = 1;

    asio::error_code ec;
    if (state->socket.open(asio::ip::udp::v4(), ec)) {
        MF_ERROR(Journal::Component::Core, Journal::Context::Networking,
            "Failed to open UDP socket: {}", ec.message());
        return nullptr;
    }

    if (state->socket.set_option(asio::socket_base::reuse_address(true), ec)) {
        MF_ERROR(Journal::Component::Core, Journal::Context::Networking,
            "Failed to set SO_REUSEADDR on UDP socket: {}", ec.message());
        return nullptr;
    }

    if (local_port > 0) {
        if (
            state->socket.bind(
                asio::ip::udp::endpoint(asio::ip::udp::v4(), local_port), ec)) {
            MF_ERROR(Journal::Component::Core, Journal::Context::Networking,
                "Failed to bind UDP socket to port {}: {}", local_port, ec.message());
            return nullptr;
        }
    }

    if (m_config.receive_buffer_size > 0) {
        if (state->socket.set_option(
                asio::socket_base::receive_buffer_size(
                    static_cast<int>(m_config.receive_buffer_size)),
                ec)) {
            MF_WARN(Journal::Component::Core, Journal::Context::Networking,
                "Failed to set receive buffer size on UDP socket: {}", ec.message());
        }
    }

    auto* raw = state.get();
    m_sockets[local_port] = std::move(state);

    start_receive_loop(*raw);

    MF_DEBUG(Journal::Component::Core, Journal::Context::Networking,
        "UDP socket bound to port {}", local_port);

    return raw;
}

void UDPBackend::release_socket(uint16_t local_port)
{
    std::unique_lock lock(m_sockets_mutex);
    auto it = m_sockets.find(local_port);
    if (it == m_sockets.end()) {
        return;
    }

    it->second->ref_count--;
    if (it->second->ref_count == 0) {
        asio::error_code ec;

        if (it->second->socket.close(ec)) {
            MF_WARN(Journal::Component::Core, Journal::Context::Networking,
                "Error closing UDP socket on port {}: {}", local_port, ec.message());
        }
        m_sockets.erase(it);

        MF_DEBUG(Journal::Component::Core, Journal::Context::Networking,
            "UDP socket on port {} released", local_port);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Private: async receive loop
// ─────────────────────────────────────────────────────────────────────────────

void UDPBackend::start_receive_loop(SocketState& state)
{
    state.socket.async_receive_from(
        asio::buffer(state.recv_buffer),
        state.sender_endpoint,
        [this, &state](const asio::error_code& ec, size_t bytes_received) {
            on_receive(state, ec, bytes_received);
        });
}

void UDPBackend::on_receive(SocketState& state, const asio::error_code& ec, size_t bytes_received)
{
    if (ec) {
        if (ec == asio::error::operation_aborted) {
            return;
        }
        MF_WARN(Journal::Component::Core, Journal::Context::Networking,
            "UDP receive error on port {}: {}", state.local_port, ec.message());
        start_receive_loop(state);
        return;
    }

    if (m_receive_callback && bytes_received > 0) {
        uint64_t ep_id = resolve_endpoint_for_sender(
            state.local_port, state.sender_endpoint);

        std::string sender_str = state.sender_endpoint.address().to_string()
            + ":" + std::to_string(state.sender_endpoint.port());

        m_receive_callback(ep_id, state.recv_buffer.data(), bytes_received, sender_str);
    }

    start_receive_loop(state);
}

uint64_t UDPBackend::resolve_endpoint_for_sender(
    uint16_t local_port, const asio::ip::udp::endpoint& sender) const
{
    std::shared_lock lock(m_endpoints_mutex);

    uint64_t fallback_id = 0;

    for (const auto& [id, record] : m_endpoints) {
        if (record.socket_state->local_port != local_port) {
            continue;
        }

        if (record.has_default_remote
            && record.default_remote.address() == sender.address()
            && record.default_remote.port() == sender.port()) {
            return id;
        }

        if (fallback_id == 0
            && (record.info.role == EndpointRole::RECEIVE
                || record.info.role == EndpointRole::BIDIRECTIONAL)) {
            fallback_id = id;
        }
    }

    return fallback_id;
}

void UDPBackend::transition_state(EndpointRecord& record, EndpointState new_state)
{
    EndpointState prev = record.info.state;
    record.info.state = new_state;

    if (m_state_callback && prev != new_state) {
        m_state_callback(record.info, prev, new_state);
    }
}

} // namespace MayaFlux::Core
