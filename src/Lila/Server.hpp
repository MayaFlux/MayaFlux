#pragma once

#include <expected>
#ifdef MAYAFLUX_PLATFORM_WINDOWS
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#endif // MAYAFLUX_PLATFORM_WINDOWS

#include "EventBus.hpp"

namespace Lila {

/**
 * @class Server
 * @brief TCP server for interactive live coding sessions in MayaFlux
 *
 * The Server class provides the underlying network infrastructure for Lila's live coding capabilities.
 * It manages client connections, message handling, session management, and event broadcasting.
 * While end users interact with MayaFlux through higher-level interfaces, Server enables
 * real-time code evaluation and communication between clients and the interpreter.
 *
 * ## Core Responsibilities
 * - Accepts TCP client connections and manages their lifecycle
 * - Receives code snippets or control messages from clients
 * - Dispatches messages to the interpreter and returns results
 * - Tracks client sessions and broadcasts events (e.g., connection, evaluation, errors)
 * - Provides hooks for connection, disconnection, and server lifecycle events
 *
 * ## Usage Flow
 * 1. Construct a Server instance with a desired port (default: 9090)
 * 2. Set message and connection handlers as needed
 * 3. Call start() to begin listening for clients
 * 4. Use stop() to gracefully shut down the server
 * 5. Use event_bus() to subscribe to server events for monitoring or integration
 *
 * The Server is designed for internal use by Lila and is not intended for direct user interaction.
 */
class Server {
public:
    /**
     * @brief Handler for processing incoming client messages
     * @param message The message received from the client
     * @return Expected response string or error string
     *
     * The message handler is invoked for each message received from a client.
     * It typically forwards code snippets to the interpreter and returns the result.
     */
    using MessageHandler = std::function<std::expected<std::string, std::string>(std::string_view)>;

    /**
     * @brief Handler for client connection/disconnection events
     * @param info Information about the connected/disconnected client
     */
    using ConnectionHandler = std::function<void(const ClientInfo&)>;

    /**
     * @brief Handler for server start events
     */
    using StartHandler = std::function<void()>;

    /**
     * @brief Constructs a Server instance
     * @param port TCP port to listen on (default: 9090)
     */
    Server(int port = 9090);

    /**
     * @brief Destructor; stops the server if running
     */
    ~Server();

    /**
     * @brief Starts the server and begins accepting clients
     * @return True if server started successfully, false otherwise
     */
    [[nodiscard]] bool start() noexcept;

    /**
     * @brief Stops the server and disconnects all clients
     */
    void stop() noexcept;

    /**
     * @brief Checks if the server is currently running
     * @return True if running, false otherwise
     */
    [[nodiscard]] bool is_running() const { return m_running; }

    /**
     * @brief Sets the handler for incoming client messages
     * @param handler Function to process messages
     */
    void set_message_handler(MessageHandler handler) { m_message_handler = std::move(handler); }

    /**
     * @brief Registers a handler for client connection events
     * @param handler Function to call when a client connects
     */
    void on_client_connected(ConnectionHandler handler) { m_connect_handler = std::move(handler); }

    /**
     * @brief Registers a handler for client disconnection events
     * @param handler Function to call when a client disconnects
     */
    void on_client_disconnected(ConnectionHandler handler) { m_disconnect_handler = std::move(handler); }

    /**
     * @brief Registers a handler for server start events
     * @param handler Function to call when the server starts
     */
    void on_server_started(StartHandler handler) { m_start_handler = std::move(handler); }

    /**
     * @brief Accesses the event bus for subscribing to server events
     * @return Reference to the EventBus
     */
    EventBus& event_bus() { return m_event_bus; }

    /**
     * @brief Const access to the event bus
     * @return Const reference to the EventBus
     */
    const EventBus& event_bus() const noexcept { return m_event_bus; }

    /**
     * @brief Broadcasts an event to connected clients
     * @param event The event to broadcast
     * @param target_session Optional session ID to target a specific client
     */
    void broadcast_event(const StreamEvent& event, std::optional<std::string_view> target_session = std::nullopt);

    /**
     * @brief Broadcasts a raw message to all connected clients
     * @param message The message to send
     */
    void broadcast_to_all(std::string_view message);

    /**
     * @brief Sets the session ID for a client
     * @param client_fd Client file descriptor
     * @param session_id Session identifier string
     */
    void set_client_session(int client_fd, std::string session_id);

    /**
     * @brief Gets the session ID for a client
     * @param client_fd Client file descriptor
     * @return Optional session ID string
     */
    [[nodiscard]] std::optional<std::string> get_client_session(int client_fd) const;

    /**
     * @brief Gets a list of all currently connected clients
     * @return Vector of ClientInfo structures
     */
    [[nodiscard]] std::vector<ClientInfo> get_connected_clients() const;

private:
    int m_port; ///< TCP port to listen on
    int m_server_fd { -1 }; ///< Server socket file descriptor
    std::atomic<bool> m_running { false }; ///< Server running state
    std::jthread m_server_thread; ///< Server thread

    MessageHandler m_message_handler; ///< Handler for client messages
    ConnectionHandler m_connect_handler; ///< Handler for client connections
    ConnectionHandler m_disconnect_handler; ///< Handler for client disconnections
    StartHandler m_start_handler; ///< Handler for server start
    EventBus m_event_bus; ///< Event bus for publishing server events

    mutable std::shared_mutex m_clients_mutex; ///< Mutex for client map
    std::unordered_map<int, ClientInfo> m_connected_clients; ///< Map of connected clients

    /**
     * @brief Main server loop; accepts and manages clients
     * @param stop_token Token to signal server shutdown
     */
    void server_loop(const std::stop_token& stop_token);

    /**
     * @brief Handles communication with a single client
     * @param client_fd Client file descriptor
     */
    void handle_client(int client_fd);

    /**
     * @brief Reads a message from a client
     * @param client_fd Client file descriptor
     * @return Expected message string or error string
     */
    [[nodiscard]] std::expected<std::string, std::string> read_message(int client_fd);

    /**
     * @brief Sends a message to a client
     * @param client_fd Client file descriptor
     * @param message Message to send
     * @return True if message sent successfully
     */
    [[nodiscard]] bool send_message(int client_fd, std::string_view message);

    /**
     * @brief Processes control messages (e.g., session, ping)
     * @param client_fd Client file descriptor
     * @param message Control message string
     */
    void process_control_message(int client_fd, std::string_view message);

    /**
     * @brief Cleans up resources for a disconnected client
     * @param client_fd Client file descriptor
     */
    void cleanup_client(int client_fd);
};

} // namespace Lila
