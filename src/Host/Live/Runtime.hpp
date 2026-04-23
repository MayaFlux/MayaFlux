#pragma once

namespace MayaFlux::Host::Live {

/**
 * @brief Start a Lila interpreter and TCP server inside this process.
 *
 * Constructs a Lila instance and initializes it in OperationMode::Both.
 * The interpreter skips loading MayaFluxLib because the host process
 * already has it resident in the symbol table.
 *
 * Only one Lila instance is permitted per process. Subsequent calls
 * while one is already running return false and log a warning.
 *
 * JIT'd code evaluated through this session can call any MayaFlux
 * symbol visible in the process symbol table. Objects constructed
 * with shorter lifetimes than the host should use MayaFlux::store()
 * or MayaFlux::make_persistent() to survive declaration-group scoping.
 *
 * @param port TCP port for the Lila server to listen on.
 * @return true on successful start, false otherwise.
 */
[[nodiscard]] MAYAFLUX_HOST_API bool start_lila(uint16_t port = 9090);

/**
 * @brief Stop the running Lila interpreter and TCP server.
 *
 * Any JIT'd function pointers obtained through this session become
 * invalid after this returns.
 *
 * @param clear_persistent_store If true, also empties the persistent
 *        store so objects created during the session are released.
 *        Defaults to false so a subsequent start_lila() can resume
 *        against store()-held state.
 */
MAYAFLUX_HOST_API void stop_lila(bool clear_persistent_store = false);

/**
 * @brief True if a Lila instance is currently running in this process.
 */
[[nodiscard]] MAYAFLUX_HOST_API bool lila_active();

/**
 * @brief TCP port of the running Lila instance, or 0 if none is running.
 */
[[nodiscard]] MAYAFLUX_HOST_API uint16_t lila_port();

} // namespace MayaFlux::Host::Live
