#pragma once

#include <typeindex>

namespace MayaFlux::Registry {

/**
 * @brief Thread-safe singleton registry for backend service discovery
 *
 * Provides centralized, type-safe registry for backend capabilities without
 * creating dependencies between processing components and specific backend
 * implementations. Backends register their services on initialization, and
 * processing components (Buffers, Nodes, Coroutines, etc.) query for needed
 * capabilities at runtime.
 *
 * Design Philosophy:
 * - Zero coupling between consumers and backends
 * - Runtime service discovery with compile-time type safety
 * - Graceful degradation when services unavailable
 * - Thread-safe for concurrent access across subsystems
 * - Singleton to avoid engine/subsystem dependencies
 * - Hot-swappable backends via re-registration
 *
 * Thread Safety:
 * All methods are thread-safe. Registration and queries can occur concurrently
 * from different threads. Uses shared_mutex for optimal read performance
 * (multiple concurrent queries, exclusive write for registration).
 *
 * Lifecycle:
 * 1. Backend initializes and registers services
 * 2. Consumers query for services as needed
 * 3. Backend shuts down and unregisters services
 * 4. Registry remains valid for next backend initialization
 *
 * Example Usage:
 *
 * Backend registration:
 *   auto& registry = BackendRegistry::instance();
 *   registry.register_service<IBufferService>([this]() -> void* {
 *       return m_buffer_service.get();
 *   });
 *
 * Consumer query:
 *   auto* service = BackendRegistry::instance().get_service<IBufferService>();
 *   if (service) {
 *       service->create_buffer(...);
 *   }
 */
class MAYAFLUX_API BackendRegistry {
public:
    using ServiceId = std::type_index;
    using ServiceFactory = std::function<void*()>;

    /**
     * @brief Get the global registry instance
     * @return Reference to singleton registry
     *
     * Thread-safe initialization via static local variable (C++11 guarantee).
     * Instance is never destroyed - lives for program duration.
     */
    static BackendRegistry& instance()
    {
        static BackendRegistry registry;
        return registry;
    }

    BackendRegistry(const BackendRegistry&) = delete;
    BackendRegistry& operator=(const BackendRegistry&) = delete;
    BackendRegistry(BackendRegistry&&) = delete;
    BackendRegistry& operator=(BackendRegistry&&) = delete;

    /**
     * @brief Register a backend service capability
     * @tparam Interface The service interface type being provided
     * @param factory Factory function returning pointer to service implementation
     *
     * Thread-safe. Multiple registrations of the same interface type will
     * overwrite previous registrations, enabling backend hot-swapping.
     *
     * The factory is called each time get_service() is invoked, allowing
     * backends to return context-specific implementations if needed.
     *
     * Example:
     *   registry.register_service<IBufferService>([this]() -> void* {
     *       return static_cast<IBufferService*>(&m_buffer_service_impl);
     *   });
     */
    template <typename Interface>
    void register_service(ServiceFactory factory)
    {
        std::unique_lock lock(m_mutex);
        m_services[typeid(Interface)] = std::move(factory);
    }

    /**
     * @brief Query for a backend service
     * @tparam Interface The service interface type needed
     * @return Pointer to service implementation or nullptr if unavailable
     *
     * Thread-safe. Returns nullptr if service not registered - callers
     * must always check for nullptr before use.
     *
     * The returned pointer is valid as long as the backend remains alive.
     * Do not cache pointers across backend lifetime boundaries.
     *
     * Example:
     *   auto* service = registry.get_service<IBufferService>();
     *   if (service) {
     *       auto buffer = service->create_buffer(...);
     *   } else {
     *       // Handle missing service gracefully
     *   }
     */
    template <typename Interface>
    Interface* get_service()
    {
        std::shared_lock lock(m_mutex);
        auto it = m_services.find(typeid(Interface));
        if (it != m_services.end()) {
            return static_cast<Interface*>(it->second());
        }
        return nullptr;
    }

    /**
     * @brief Check if a service is available
     * @tparam Interface The service interface type to check
     * @return True if service is currently registered
     *
     * Thread-safe. Useful for capability detection without error handling.
     * Note: Service availability can change between has_service() and
     * get_service() calls due to concurrent unregistration.
     *
     * Example:
     *   if (registry.has_service<IComputeService>()) {
     *       // Compute shaders available
     *   }
     */
    template <typename Interface>
    bool has_service() const
    {
        std::shared_lock lock(m_mutex);
        return m_services.contains(typeid(Interface));
    }

    /**
     * @brief Unregister a service
     * @tparam Interface The service interface type to unregister
     *
     * Thread-safe. Typically called during backend shutdown.
     * Safe to call even if service not registered (no-op).
     * After unregistration, get_service() will return nullptr.
     *
     * Example:
     *   registry.unregister_service<IBufferService>();
     */
    template <typename Interface>
    void unregister_service()
    {
        std::unique_lock lock(m_mutex);
        m_services.erase(typeid(Interface));
    }

    /**
     * @brief Clear all registered services
     *
     * Thread-safe. Useful for testing or complete system reset.
     * Typically called during engine shutdown to ensure clean state.
     */
    void clear_all_services()
    {
        std::unique_lock lock(m_mutex);
        m_services.clear();
    }

    /**
     * @brief Get count of currently registered services
     * @return Number of registered service types
     *
     * Thread-safe. Primarily for debugging and diagnostics.
     */
    size_t get_service_count() const
    {
        std::shared_lock lock(m_mutex);
        return m_services.size();
    }

private:
    BackendRegistry() = default;
    ~BackendRegistry() = default;

    mutable std::shared_mutex m_mutex;
    std::unordered_map<ServiceId, ServiceFactory> m_services;
};

} // namespace MayaFlux::Registry
