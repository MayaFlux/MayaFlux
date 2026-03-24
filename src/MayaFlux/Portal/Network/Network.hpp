#pragma once

namespace MayaFlux::Registry::Service {
struct NetworkService;
}

namespace MayaFlux::Portal::Network {

/**
 * @brief Initialize Portal::Network
 * @param service Pointer to the registered NetworkService.
 * @return true if initialization succeeded.
 *
 * Must be called after NetworkSubsystem has started and registered
 * its NetworkService into BackendRegistry. Sequence mirrors
 * Portal::Graphics::initialize(backend).
 *
 * Initializes, in order:
 *   - NetworkFoundry  (endpoint resource authority)  [not yet implemented]
 *   - StreamForge     (stream lifecycle and schema)  [not yet implemented]
 *   - PacketFlow      (serialization and dispatch)   [not yet implemented]
 *
 * Today this is a stub. The function signature and call site are
 * established so the engine init sequence does not need to change
 * when the singletons are added.
 */
MAYAFLUX_API bool initialize(Registry::Service::NetworkService* service);

/**
 * @brief Stop active Portal::Network operations.
 *
 * Drains any in-flight sends, stops background workers.
 * Call before shutdown().
 */
MAYAFLUX_API void stop();

/**
 * @brief Shutdown Portal::Network and release all resources.
 *
 * Closes all managed endpoints and destroys singletons.
 * Must be called after stop().
 */
MAYAFLUX_API void shutdown();

/**
 * @brief Return true if Portal::Network has been initialized.
 */
MAYAFLUX_API bool is_initialized();

} // namespace MayaFlux::Portal::Network
