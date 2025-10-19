#pragma once

#include <expected>

namespace Lila {

class ClangInterpreter;
class Server;
struct ClientInfo;

enum class OperationMode : uint8_t {
    Direct, ///< Only direct evaluation
    Server, ///< Only server mode
    Both ///< Both direct eval and server
};

/**
 * @class Lila
 * @brief Central orchestrator and entry point for live coding in MayaFlux
 *
 * The Lila class provides a unified interface for interactive live coding in MayaFlux.
 * It manages the lifecycle and coordination of the embedded Clang interpreter and the TCP server,
 * enabling both direct code evaluation and networked live coding sessions.
 *
 * ## Core Responsibilities
 * - Initialize and configure the live coding environment (interpreter and/or server)
 * - Evaluate C++ code snippets and files at runtime
 * - Manage TCP server for remote live coding sessions
 * - Provide symbol lookup and introspection
 * - Allow customization of include paths and compile flags
 * - Expose event/callback hooks for success, error, and server events
 *
 * ## Usage Flow
 * 1. Construct a Lila instance
 * 2. Call initialize() with desired mode (Direct, Server, Both)
 * 3. For direct evaluation, use eval() or eval_file()
 * 4. For server mode, use start_server(), stop_server(), and event hooks
 * 5. Use get_symbol_address() and get_defined_symbols() for runtime introspection
 * 6. Optionally configure include paths and compile flags before initialization
 * 7. Register callbacks for success, error, and server/client events as needed
 *
 * Lila is intended as the main API for embedding live coding capabilities in MayaFlux.
 * End users interact with Lila via higher-level interfaces or the live_server binary,
 * not directly with its internal components.
 */
class LILA_API Lila {
public:
    /**
     * @brief Constructs a Lila instance
     */
    Lila();

    /**
     * @brief Destructor; cleans up interpreter and server resources
     */
    ~Lila();

    /**
     * @brief Initializes the live coding environment
     * @param mode Operation mode (Direct, Server, Both)
     * @param server_port TCP port for server mode (default: 9090)
     * @return True if initialization succeeded, false otherwise
     */
    bool initialize(OperationMode mode = OperationMode::Direct, int server_port = 9090) noexcept;

    /**
     * @brief Evaluates a C++ code snippet directly
     * @param code Code to execute
     * @return True if evaluation succeeded, false otherwise
     */
    bool eval(const std::string& code);

    /**
     * @brief Evaluates a C++ code file directly
     * @param filepath Path to the file to execute
     * @return True if evaluation succeeded, false otherwise
     */
    bool eval_file(const std::string& filepath);

    /**
     * @brief Starts the TCP server for remote live coding
     * @param port TCP port to listen on (default: 9090)
     */
    void start_server(int port = 9090);

    /**
     * @brief Stops the TCP server and disconnects all clients
     */
    void stop_server();

    /**
     * @brief Checks if the server is currently running
     * @return True if running, false otherwise
     */
    [[nodiscard]] bool is_server_running() const;

    /**
     * @brief Gets the address of a symbol defined in the interpreter
     * @param name Symbol name
     * @return Pointer to symbol, or nullptr if not found
     */
    void* get_symbol_address(const std::string& name);

    /**
     * @brief Gets a list of all defined symbols
     * @return Vector of symbol names
     */
    std::vector<std::string> get_defined_symbols();

    /**
     * @brief Adds an include path for code evaluation
     * @param path Directory to add to the include search path
     */
    void add_include_path(const std::string& path);

    /**
     * @brief Adds a compile flag for code evaluation
     * @param flag Compiler flag to add
     */
    void add_compile_flag(const std::string& flag);

    /**
     * @brief Registers a callback for successful code evaluation
     * @param callback Function to call on success
     */
    void on_success(std::function<void()> callback);

    /**
     * @brief Registers a callback for code evaluation errors
     * @param callback Function to call on error (receives error string)
     */
    void on_error(std::function<void(const std::string&)> callback);

    /**
     * @brief Registers a callback for new client connections (server mode)
     * @param callback Function to call when a client connects
     */
    void on_server_client_connected(std::function<void(const ClientInfo&)> callback);

    /**
     * @brief Registers a callback for client disconnections (server mode)
     * @param callback Function to call when a client disconnects
     */
    void on_server_client_disconnected(std::function<void(const ClientInfo&)> callback);

    /**
     * @brief Registers a callback for server start events
     * @param callback Function to call when the server starts
     */
    void on_server_started(std::function<void()> callback);

    /**
     * @brief Gets the last error message
     * @return String containing the last error
     */
    [[nodiscard]] std::string get_last_error() const;

    /**
     * @brief Gets the current operation mode
     * @return OperationMode value
     */
    [[nodiscard]] OperationMode get_current_mode() const;

private:
    std::unique_ptr<ClangInterpreter> m_interpreter; ///< Embedded Clang interpreter
    std::unique_ptr<Server> m_server; ///< TCP server for live coding

    OperationMode m_current_mode; ///< Current operation mode
    std::function<void()> m_success_callback; ///< Success callback
    std::function<void(const std::string&)> m_error_callback; ///< Error callback

    bool initialize_interpreter();
    bool initialize_server(int port);

    std::expected<std::string, std::string> handle_server_message(std::string_view message);

    /**
     * @brief Escapes a string for safe JSON encoding
     * @param str Input string
     * @return Escaped string
     */
    static std::string escape_json(const std::string& str);
};

} // namespace Lila
