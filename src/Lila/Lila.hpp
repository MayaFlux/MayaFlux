#pragma once

#include "Address.hpp"
#include "EvalReport.hpp"
#include "Result.hpp"

namespace Lila {

class ClangInterpreter;
class Listener;

/**
 * @class Lila
 * @brief Central orchestrator and entry point for live coding in MayaFlux
 *
 * Lila owns the embedded Clang interpreter and, optionally, one listener that
 * lets other processes send it code. The two are independent: initialize()
 * brings up the interpreter, and listen() starts listening at an address.
 * Code can always be evaluated in-process through eval(), with or without a
 * listener. Every fallible call returns its reason.
 *
 * ## Usage Flow
 * 1. Construct a Lila instance
 * 2. Optionally add include paths and compile flags, then call initialize()
 * 3. Load libraries and evaluate code
 * 4. Call listen() with a TcpAddress or UnixAddress to accept remote clients
 */
class LILA_API Lila {
public:
    Lila();
    ~Lila();

    Lila(const Lila&) = delete;
    Lila& operator=(const Lila&) = delete;
    Lila(Lila&&) = delete;
    Lila& operator=(Lila&&) = delete;

    /**
     * @brief Brings up the interpreter. No server is started.
     *
     * @param skip_host_library_load If true, the interpreter will not call
     *        LoadDynamicLibrary on MayaFluxLib at startup. Used when MayaFluxLib
     *        is already resident in the calling process.
     */
    Status initialize(bool skip_host_library_load = false);

    /**
     * @brief Starts listening so remote clients can send code.
     *
     * A Lila listens at one address at most. Listening already is stopped first.
     *
     * @param where TcpAddress or UnixAddress.
     */
    Status listen(const Address& where);

    /**
     * @brief Stops listening and disconnects all clients
     */
    void stop_listening();

    /**
     * @brief Checks if a listener is running
     */
    [[nodiscard]] bool is_listening() const;

    /**
     * @brief Where this is listening, with anything resolved at start filled in.
     * @return The address, or nullptr if not listening. Valid until the
     *         listener stops or listen() is called again. Copy it to keep it.
     */
    [[nodiscard]] const Address* address() const;

    /**
     * @brief Evaluates a C++ code snippet
     * @param code Code to execute
     * @return Success, or the compiler's or runtime's error text
     */
    Status eval(const std::string& code);

    /**
     * @brief Registers an observer of every evaluation, local and from clients.
     *
     * Each eval, whether it succeeded or not, reaches every observer once, on the
     * thread that ran it. Registration and removal are safe from any thread and
     * from inside an observer. After remove_eval_observer returns, a call that
     * was already running may still be finishing on another thread, so keep
     * @p user alive until the Lila object is destroyed or listening has stopped.
     *
     * @param observer Called with @p user and a report. Must not be null.
     * @param user Handed back unchanged.
     * @return An id for remove_eval_observer, never zero.
     */
    uint64_t add_eval_observer(EvalObserver observer, void* user);

    /**
     * @brief Removes an observer. An unknown id is ignored.
     */
    void remove_eval_observer(uint64_t id);

    /**
     * @brief Load a shared library into the JIT symbol table.
     * @param path Full path to the shared library.
     */
    Status load_library(const std::string& path);

    /**
     * @brief Gets the address of a symbol defined in the interpreter
     * @param name Symbol name
     * @return Pointer to symbol, or nullptr if not found
     */
    void* get_symbol_address(const std::string& name);

    /**
     * @brief Adds an include path. Takes effect only before initialize().
     */
    void add_include_path(const std::string& path);

    /**
     * @brief Adds a compile flag. Takes effect only before initialize().
     */
    void add_compile_flag(const std::string& flag);

    /**
     * @brief Blocks until shutdown is requested or the listener stops
     * @param external_flag Optional flag that ends the wait when it becomes false
     *
     * On macOS this runs the main run loop, which the interpreter needs to
     * service work that must run on the main thread. Call it on the main thread
     * after listen().
     */
    void await_shutdown(const std::atomic<bool>* external_flag);

    /**
     * @brief Ends await_shutdown() from any thread
     */
    void request_shutdown();

private:
    struct Observers;

    Result<std::string> handle_message(std::string_view message);

    Status evaluate(const std::string& code, EvalOrigin origin);

    static Status refuse(std::string reason);

    std::unique_ptr<ClangInterpreter> m_interpreter;
    std::unique_ptr<Observers> m_observers;
    std::unique_ptr<Listener> m_listener;
    std::optional<Address> m_bound;
    std::atomic<bool> m_shutdown_requested { false };
};

} // namespace Lila
