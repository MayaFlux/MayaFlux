#include "NetworkSubsystem.hpp"

#include "MayaFlux/Core/Backends/Network/TCPBackend.hpp"
#include "MayaFlux/Core/Backends/Network/UDPBackend.hpp"
#include "MayaFlux/Core/Backends/Network/UDSBackend.hpp"

#include "MayaFlux/Portal/Network/Network.hpp"
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
    if (m_config.uds.enabled) {
        initialize_uds_backend();
    }

    register_backend_service();

    Portal::Network::initialize(m_network_service.get());

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

    if (Portal::Network::is_initialized()) {
        Portal::Network::stop();
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

    m_routes.reset();

    {
        std::unique_lock lock(m_backends_mutex);
        for (auto& [transport, backend] : m_backends) {
            backend->shutdown();
        }
        m_backends.clear();
    }

    if (Portal::Network::is_initialized()) {
        Portal::Network::shutdown();
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

void NetworkSubsystem::wait_until_running()
{
    while (!m_running.load(std::memory_order_acquire))
        std::this_thread::yield();
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

    backend->set_endpoint_id_allocator([this]() -> uint64_t {
        return m_next_endpoint_id.fetch_add(1);
    });

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

    m_routes.update([&](RouteTable& routes) {
        routes.insert_or_assign(ep.id, Route { .transport = info.transport });
    });

    uint64_t backend_id = backend->open_endpoint(ep);
    if (backend_id == 0) {
        m_routes.update([id = ep.id](RouteTable& routes) { return routes.erase(id); });
        return 0;
    }

    return ep.id;
}

void NetworkSubsystem::close_endpoint(uint64_t endpoint_id)
{
    auto* backend = resolve_backend(endpoint_id);
    if (!backend) {
        return;
    }

    std::vector<uint64_t> accepted;
    m_routes.update([&](RouteTable& routes) {
        accepted.clear();
        for (const auto& [child, route] : routes) {
            if (route.parent_id == endpoint_id) {
                accepted.push_back(child);
            }
        }
        routes.erase(endpoint_id);
    });

    for (uint64_t child : accepted) {
        close_endpoint(child);
    }

    backend->close_endpoint(endpoint_id);
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
    const auto held = callback
        ? std::make_shared<const NetworkReceiveCallback>(std::move(callback))
        : std::shared_ptr<const NetworkReceiveCallback> {};

    m_routes.update([&](RouteTable& routes) {
        const auto it = routes.find(endpoint_id);
        if (it == routes.end()) {
            return false;
        }
        Route route = it->second;
        route.callback = held;
        routes.insert_or_assign(endpoint_id, std::move(route));
        return true;
    });
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

void NetworkSubsystem::initialize_uds_backend()
{
    auto uds = std::make_unique<UDSBackend>(m_config.uds, *m_io_context);
    add_backend(std::move(uds));
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
    std::shared_ptr<const NetworkReceiveCallback> callback;

    {
        const auto routes = m_routes.read();
        if (!routes) {
            return;
        }

        const auto it = routes->find(endpoint_id);
        if (it == routes->end()) {
            return;
        }

        callback = it->second.callback;
        if (!callback && it->second.parent_id != 0) {
            const auto parent = routes->find(it->second.parent_id);
            if (parent != routes->end()) {
                callback = parent->second.callback;
            }
        }
    }

    if (callback && *callback) {
        (*callback)(endpoint_id, data, size, sender_addr);
    }
}

void NetworkSubsystem::on_backend_state_change(const EndpointInfo& info,
    EndpointState previous, EndpointState current)
{
    if (info.parent_id != 0 && previous == EndpointState::CLOSED
        && current == EndpointState::OPEN) {
        m_routes.update([&](RouteTable& routes) {
            if (routes.find(info.parent_id) == routes.end()) {
                return false;
            }
            routes.insert_or_assign(info.id,
                Route { .transport = info.transport, .parent_id = info.parent_id });
            return true;
        });
    }

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
        const auto routes = m_routes.read();
        if (!routes) {
            return nullptr;
        }
        const auto it = routes->find(endpoint_id);
        if (it == routes->end()) {
            return nullptr;
        }
        transport = it->second.transport;
    }

    return get_backend(transport);
}

} // namespace MayaFlux::Core
