#pragma once

#ifdef MAYAFLUX_LILA_ENABLED

namespace MayaFlux::Host {

/**
 * @brief Boot a Lila interpreter and TCP server inside this process.
 *
 * Constructs a Lila instance owned by MayaFluxLib and initializes it
 * in OperationMode::Both. The interpreter skips redundant loading of
 * MayaFluxLib because the host process already has it resident.
 *
 * Only one attached Lila is permitted per process. Subsequent calls
 * while one is already attached return false and log a warning.
 *
 * JIT'd code evaluated through the attached session can freely call
 * any MayaFlux symbol visible in the process symbol table. Objects
 * the JIT constructs with shorter lifetimes than the host should use
 * MayaFlux::make_persistent() or MayaFlux::store() to survive
 * declaration-group scoping.
 *
 * @param port TCP port for the Lila server to listen on.
 * @return true on successful attach, false otherwise.
 */
MAYAFLUX_API bool attach_lila(uint16_t port = 9090);

/**
 * @brief Shut down the attached Lila interpreter and TCP server.
 *
 * Any JIT'd function pointers obtained through the attached session
 * become invalid after this returns.
 *
 * @param clear_persistent_store If true, also empties the
 *        MayaFlux::internal::persistent_store() so objects created
 *        during the attached session are released. Defaults to false
 *        so a subsequent attach_lila() can resume against Persist-held
 *        state.
 */
MAYAFLUX_API void detach_lila(bool clear_persistent_store = false);

/**
 * @brief True if a Lila instance is currently attached to this process.
 */
[[nodiscard]] MAYAFLUX_API bool is_lila_attached();

/**
 * @brief TCP port of the attached Lila, or 0 if none is attached.
 */
[[nodiscard]] MAYAFLUX_API uint16_t attached_lila_port();

} // namespace MayaFlux::Host

#endif // MAYAFLUX_LILA_ENABLED
