#include "NetworkSubsystem.hpp"

#include "MayaFlux/Core/Backends/Network/TCPBackend.hpp"
#include "MayaFlux/Core/Backends/Network/UDPBackend.hpp"

#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/NetworkService.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Core {

NetworkSubsystem::NetworkSubsystem(const GlobalNetworkConfig& config)
    : m_config(config)
    , m_tokens {
        .Buffer = Buffers::ProcessingToken::EVENT_RATE,
        .Node = Nodes::ProcessingToken::EVENT_RATE,
        .Task = Vruta::ProcessingToken::EVENT_DRIVEN
    }
    , m_io_context(std::make_unique<asio::io_context>())
{
}

NetworkSubsystem::~NetworkSubsystem()
{
    shutdown();
}

// ─────────────────────────────────────────────────────────────────────────────
// ISubsystem lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void NetworkSubsystem::register_callbacks()
{
}

void NetworkSubsystem::initialize(SubsystemProcessingHandle& handle)
{
    MF_INFO(Journal::Component::Core, Journal::Context::Init,
        "Initializing Network Subsystem...");

    m_handle = &handle;

    if (m_config.udp.enabled) {
        initialize_udp_backend();
    }
    if (m_config.tcp.enabled) {
        initialize_tcp_backend();
    }
    if (m_config.shared_memory.enabled) {
        initialize_shm_backend();
    }

    register_backend_service();

    m_ready.store(true);

    MF_INFO(Journal::Component::Core, Journal::Context::Init,
        "Network Subsystem initialized with {} backend(s)", m_backends.size());
}

void NetworkSubsystem::start()
{
    if (!m_ready.load()) {
        MF_ERROR(Journal::Component::Core, Journal::Context::Init,
            "Cannot start NetworkSubsystem: not initialized");
        return;
    }

    if (m_running.load()) {
        return;
    }

    {
        std::shared_lock lock(m_backends_mutex);
        for (auto& [transport, backend] : m_backends) {
            backend->start();
        }
    }

    m_work_guard = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(
        m_io_context->get_executor());

    m_io_thread = std::jthread([this](const std::stop_token& token) {
        MF_DEBUG(Journal::Component::Core, Journal::Context::Init,
            "Network IO thread started");

        while (!token.stop_requested()) {
            try {
                m_io_context->run();
                break;
            } catch (const std::exception& e) {
                MF_ERROR(Journal::Component::Core, Journal::Context::Init,
                    "IO context exception: {}", e.what());
            }
        }

        MF_DEBUG(Journal::Component::Core, Journal::Context::Init,
            "Network IO thread exiting");
    });

    m_running.store(true);

    MF_INFO(Journal::Component::Core, Journal::Context::Init,
        "Network Subsystem started");
}

void NetworkSubsystem::pause()
{
    stop();
}

void NetworkSubsystem::resume()
{
    start();
}

void NetworkSubsystem::stop()
{
    if (!m_running.load()) {
        return;
    }

    m_running.store(false);

    {
        std::shared_lock lock(m_backends_mutex);
        for (auto& [transport, backend] : m_backends) {
            backend->stop();
        }
    }

    m_work_guard.reset();
    m_io_context->stop();

    if (m_io_thread.joinable()) {
        m_io_thread.request_stop();
        m_io_thread.join();
    }

    m_io_context->restart();

    MF_INFO(Journal::Component::Core, Journal::Context::Shutdown,
        "Network Subsystem stopped");
}

void NetworkSubsystem::shutdown()
{
    stop();

    {
        std::unique_lock lock(m_routing_mutex);
        m_endpoint_routing.clear();
    }

    {
        std::unique_lock lock(m_callbacks_mutex);
        m_endpoint_callbacks.clear();
    }

    {
        std::unique_lock lock(m_backends_mutex);
        for (auto& [transport, backend] : m_backends) {
            backend->shutdown();
        }
        m_backends.clear();
    }

    if (m_network_service) {
        Registry::BackendRegistry::instance()
            .unregister_service<Registry::Service::NetworkService>();
        m_network_service.reset();
    }

    m_ready.store(false);

    MF_INFO(Journal::Component::Core, Journal::Context::Init,
        "Network Subsystem shutdown complete");
}

// ─────────────────────────────────────────────────────────────────────────────
// Backend management
// ─────────────────────────────────────────────────────────────────────────────

bool NetworkSubsystem::add_backend(std::unique_ptr<INetworkBackend> backend)
{
    if (!backend) {
        return false;
    }

    NetworkTransport transport = backend->get_transport();

    std::unique_lock lock(m_backends_mutex);

    if (m_backends.contains(transport)) {
        MF_WARN(Journal::Component::Core, Journal::Context::NetworkSubsystem,
            "Network backend {} already registered", backend->get_name());
        return false;
    }

    if (!backend->initialize()) {
        MF_ERROR(Journal::Component::Core, Journal::Context::NetworkSubsystem,
            "Failed to initialize network backend: {}", backend->get_name());
        return false;
    }

    backend->set_receive_callback(
        [this](uint64_t id, const uint8_t* data, size_t size, std::string_view addr) {
            on_backend_receive(id, data, size, addr);
        });

    backend->set_state_callback(
        [this](const EndpointInfo& info, EndpointState prev, EndpointState curr) {
            on_backend_state_change(info, prev, curr);
        });

    if (auto* tcp = dynamic_cast<TCPBackend*>(backend.get())) {
        tcp->set_endpoint_id_allocator([this]() -> uint64_t {
            return m_next_endpoint_id.fetch_add(1);
        });
    }

    MF_INFO(Journal::Component::Core, Journal::Context::NetworkSubsystem,
        "Added network backend: {}", backend->get_name());

    m_backends[transport] = std::move(backend);
    return true;
}

INetworkBackend* NetworkSubsystem::get_backend(NetworkTransport transport) const
{
    std::shared_lock lock(m_backends_mutex);
    auto it = m_backends.find(transport);
    return (it != m_backends.end()) ? it->second.get() : nullptr;
}

std::vector<INetworkBackend*> NetworkSubsystem::get_backends() const
{
    std::shared_lock lock(m_backends_mutex);
    std::vector<INetworkBackend*> result;
    result.reserve(m_backends.size());
    for (const auto& [transport, backend] : m_backends) {
        result.push_back(backend.get());
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Endpoint management
// ─────────────────────────────────────────────────────────────────────────────

uint64_t NetworkSubsystem::open_endpoint(const EndpointInfo& info)
{
    auto* backend = get_backend(info.transport);
    if (!backend) {
        MF_ERROR(Journal::Component::Core, Journal::Context::NetworkSubsystem,
            "No backend for transport {}", static_cast<int>(info.transport));
        return 0;
    }

    EndpointInfo ep = info;
    ep.id = m_next_endpoint_id.fetch_add(1);

    uint64_t backend_id = backend->open_endpoint(ep);
    if (backend_id == 0) {
        return 0;
    }

    {
        std::unique_lock lock(m_routing_mutex);
        m_endpoint_routing[ep.id] = info.transport;
    }

    return ep.id;
}

void NetworkSubsystem::close_endpoint(uint64_t endpoint_id)
{
    auto* backend = resolve_backend(endpoint_id);
    if (!backend) {
        return;
    }

    backend->close_endpoint(endpoint_id);

    {
        std::unique_lock lock(m_routing_mutex);
        m_endpoint_routing.erase(endpoint_id);
    }

    {
        std::unique_lock lock(m_callbacks_mutex);
        m_endpoint_callbacks.erase(endpoint_id);
    }
}

bool NetworkSubsystem::send(uint64_t endpoint_id, const uint8_t* data, size_t size)
{
    auto* backend = resolve_backend(endpoint_id);
    if (!backend) {
        return false;
    }
    return backend->send(endpoint_id, data, size);
}

bool NetworkSubsystem::send_to(uint64_t endpoint_id, const uint8_t* data, size_t size,
    const std::string& address, uint16_t port)
{
    auto* backend = resolve_backend(endpoint_id);
    if (!backend) {
        return false;
    }
    return backend->send_to(endpoint_id, data, size, address, port);
}

EndpointState NetworkSubsystem::get_endpoint_state(uint64_t endpoint_id) const
{
    auto* backend = resolve_backend(endpoint_id);
    if (!backend) {
        return EndpointState::CLOSED;
    }
    return backend->get_endpoint_state(endpoint_id);
}

void NetworkSubsystem::set_endpoint_receive_callback(uint64_t endpoint_id,
    NetworkReceiveCallback callback)
{
    std::unique_lock lock(m_callbacks_mutex);
    m_endpoint_callbacks[endpoint_id] = std::move(callback);
}

std::vector<EndpointInfo> NetworkSubsystem::get_all_endpoints() const
{
    std::shared_lock lock(m_backends_mutex);
    std::vector<EndpointInfo> result;
    for (const auto& [transport, backend] : m_backends) {
        auto eps = backend->get_endpoints();
        result.insert(result.end(), eps.begin(), eps.end());
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Private: backend initialisation
// ─────────────────────────────────────────────────────────────────────────────

void NetworkSubsystem::initialize_udp_backend()
{
    auto udp = std::make_unique<UDPBackend>(m_config.udp, *m_io_context);
    add_backend(std::move(udp));
}

void NetworkSubsystem::initialize_tcp_backend()
{
    auto tcp = std::make_unique<TCPBackend>(m_config.tcp, *m_io_context);
    add_backend(std::move(tcp));
}

void NetworkSubsystem::initialize_shm_backend()
{
    MF_WARN(Journal::Component::Core, Journal::Context::NetworkSubsystem,
        "SharedMemory backend not yet implemented");
}

void NetworkSubsystem::register_backend_service()
{
    auto& registry = Registry::BackendRegistry::instance();

    auto service = std::make_shared<Registry::Service::NetworkService>();

    service->open_endpoint = [this](const EndpointInfo& info) {
        return open_endpoint(info);
    };

    service->close_endpoint = [this](uint64_t id) {
        close_endpoint(id);
    };

    service->send = [this](uint64_t id, const uint8_t* data, size_t size) {
        return send(id, data, size);
    };

    service->send_to = [this](uint64_t id, const uint8_t* data, size_t size,
                           const std::string& addr, uint16_t port) {
        return send_to(id, data, size, addr, port);
    };

    service->get_endpoint_state = [this](uint64_t id) {
        return get_endpoint_state(id);
    };

    service->set_endpoint_receive_callback = [this](uint64_t id, NetworkReceiveCallback cb) {
        set_endpoint_receive_callback(id, std::move(cb));
    };

    service->get_all_endpoints = [this]() {
        return get_all_endpoints();
    };

    m_network_service = service;

    registry.register_service<Registry::Service::NetworkService>(
        [service]() -> void* {
            return service.get();
        });
}

// ─────────────────────────────────────────────────────────────────────────────
// Private: callback routing
// ─────────────────────────────────────────────────────────────────────────────

void NetworkSubsystem::on_backend_receive(uint64_t endpoint_id, const uint8_t* data,
    size_t size, std::string_view sender_addr)
{
    std::shared_lock lock(m_callbacks_mutex);
    auto it = m_endpoint_callbacks.find(endpoint_id);
    if (it != m_endpoint_callbacks.end() && it->second) {
        it->second(endpoint_id, data, size, sender_addr);
    }
}

void NetworkSubsystem::on_backend_state_change(const EndpointInfo& info,
    EndpointState previous, EndpointState current)
{
    MF_DEBUG(Journal::Component::Core, Journal::Context::NetworkSubsystem,
        "Endpoint {} state: {} -> {}",
        info.id, static_cast<int>(previous), static_cast<int>(current));
}

// ─────────────────────────────────────────────────────────────────────────────
// Private: routing
// ─────────────────────────────────────────────────────────────────────────────

INetworkBackend* NetworkSubsystem::resolve_backend(uint64_t endpoint_id) const
{
    NetworkTransport transport {};
    {
        std::shared_lock lock(m_routing_mutex);
        auto it = m_endpoint_routing.find(endpoint_id);
        if (it == m_endpoint_routing.end()) {
            return nullptr;
        }
        transport = it->second;
    }

    return get_backend(transport);
}

} // namespace MayaFlux::Core
