#pragma once

namespace MayaFlux::Core {
class SystemBackend;
}

namespace MayaFlux::Portal::System {

/**
 * @brief Access the active platform backend.
 *
 * Returns nullptr if Portal::System has not been initialized.
 * Intended for use by Portal::System sub-modules only.
 */
MAYAFLUX_API Core::SystemBackend* get_backend();

/**
 * @brief Initialize Portal::System.
 *
 * Must be called before any Portal::System free functions are used.
 * No subsystem dependency is required; call at any point after engine
 * construction and before the first dialog or OS interaction.
 *
 * Initializes, in order:
 *   - Portal::System::Dialog (file chooser, save, folder picker)
 *
 * @return True if initialization succeeded.
 */
MAYAFLUX_API bool initialize();

/**
 * @brief Stop active Portal::System operations.
 *
 * Cancels any in-progress dialog if the platform backend supports it.
 * Call before shutdown().
 */
MAYAFLUX_API void stop();

/**
 * @brief Shutdown Portal::System and release all resources.
 *
 * Must be called after stop().
 */
MAYAFLUX_API void shutdown();

/**
 * @brief Return true if Portal::System has been initialized.
 */
MAYAFLUX_API bool is_initialized();

} // namespace MayaFlux::Portal::System
