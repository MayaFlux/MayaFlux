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

/**
 * @brief Find the directory containing a header staged for inclusion.
 *
 * Used by the JIT to resolve #include directives for staged headers.
 *
 * @param filename Header filename to find, e.g. "Dust.hpp"
 * @return optional path to the directory containing the header, or nullopt
 *         if the header was not staged or could not be found.
 */
MAYAFLUX_HOST_API std::optional<std::filesystem::path> find_include_dir(const std::string& filename);

/**
 * @brief Add an include path to the Lila interpreter.
 *
 * Must be called before start_lila(). Paths registered after initialization
 * have no effect on the compiler state already built by initialize().
 *
 * @param path Directory to add to the JIT include search path.
 */
MAYAFLUX_HOST_API void lila_add_include_path(const std::string& path);

/**
 * @brief Add a compile flag to the Lila interpreter.
 *
 * Must be called before start_lila(). Flags registered after initialization
 * have no effect on the compiler state already built by initialize().
 *
 * @param flag Compiler flag string, e.g. "-DMY_DEFINE" or "-I/some/path".
 */
MAYAFLUX_HOST_API void lila_add_compile_flag(const std::string& flag);

/**
 * @brief Stage a header for inclusion in the Lila interpreter.
 *
 * Resolves the header's directory and pre-evaluates the include
 * so the type is available to JIT clients without manual inclusion.
 *
 * @param filename Header filename to find and include, e.g. "Dust.hpp"
 * @return true if the file was found.
 */
MAYAFLUX_HOST_API bool lila_add_header(const std::string& filename);

/**
 * @brief Load a shared library into the JIT symbol table at runtime.
 *
 * Can be called before or after start_lila(). Libraries loaded before
 * start_lila() are staged and loaded immediately after interpreter
 * initialization. Use this for external .so/.dylib/.dll files that
 * were not linked into the host process.
 *
 * @param path Full path to the shared library.
 */
MAYAFLUX_HOST_API void lila_load_library(const std::string& path);

/**
 * @brief Evaluate a code snippet in the Lila interpreter.
 *
 * Can be called after start_lila() to pre-warm the JIT state,
 * e.g. to include community module headers before clients connect.
 *
 * @param code C++ code to evaluate, e.g. "#include \"Dust.hpp\""
 * @return true if evaluation succeeded.
 */
MAYAFLUX_HOST_API bool lila_eval(const std::string& code);

} // namespace MayaFlux::Host::Live
