#pragma once

#include <expected>

namespace MayaFlux::Platform {
struct AcquireFailure;
}

namespace MayaFlux::Host {

/**
 * @brief Why a Host call was refused, with every path that was checked.
 */
struct Failure {
    std::string message;
    std::vector<std::filesystem::path> tried;
};

/**
 * @brief Result of a Host call.
 */
template <typename T = void>
using Result = std::expected<T, Failure>;

/**
 * @brief Start a Lila interpreter and TCP server inside this process.
 *
 * Applies everything staged before this call: include paths and flags,
 * then libraries, then evaluations, in the order they were staged. The
 * attempt consumes the staged items whether or not it succeeds. If any
 * staged library or evaluation fails, the session is torn down and the
 * failure is returned.
 *
 * Only one session is permitted per process. Staging calls made on other
 * threads while the start is in progress are refused, not queued.
 *
 * @param port TCP port for the Lila server to listen on.
 * @return Success, or the reason the session did not start.
 */
MAYAFLUX_HOST_API Result<void> start_session(uint16_t port = 9090);

/**
 * @brief Stop the running session.
 *
 * Function pointers obtained through the session become invalid. Libraries
 * loaded through Host are forgotten, so a later session loads them again.
 *
 * @param clear_persistent_store If true, also empties the persistent store.
 */
MAYAFLUX_HOST_API void stop_session(bool clear_persistent_store = false);

/**
 * @brief True if a session is running in this process.
 */
[[nodiscard]] MAYAFLUX_HOST_API bool session_active();

/**
 * @brief TCP port of the running session, or 0 if none is running.
 */
[[nodiscard]] MAYAFLUX_HOST_API uint16_t session_port();

/**
 * @brief Evaluate a code snippet.
 *
 * Before start_session the snippet is staged and evaluated at start.
 *
 * @param code C++ code to evaluate.
 * @return Success, or the interpreter's error text.
 */
MAYAFLUX_HOST_API Result<void> eval(const std::string& code);

/**
 * @brief Evaluate a source file by including it.
 *
 * The file is checked at call time. Before start_session the include is
 * staged and evaluated at start.
 *
 * @param path Path to an existing regular file, absolute or relative to the working directory.
 * @return Success, or the reason the file was refused or failed to evaluate.
 */
MAYAFLUX_HOST_API Result<void> eval_file(const std::filesystem::path& path);

/**
 * @brief Address of a symbol defined in the running session.
 *
 * @param name Symbol name.
 * @return The address, or the reason there is none.
 */
MAYAFLUX_HOST_API Result<void*> symbol_address(const std::string& name);

namespace detail {

    enum class Stage : uint8_t {
        IncludePath,
        CompileFlag,
        Library,
        Eval
    };

    /**
     * @brief Apply an item to the running session, or stage it until start_session.
     *
     * Include paths and flags cannot change a session that has started and
     * are refused then. Libraries are loaded once per path.
     */
    MAYAFLUX_HOST_API Result<void> apply_or_stage(Stage stage, std::string value);

    MAYAFLUX_HOST_API Failure to_failure(const Platform::AcquireFailure& failure);

}

} // namespace MayaFlux::Host
