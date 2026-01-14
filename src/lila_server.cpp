/**
 * @file lila_server.cpp
 * @brief Entry point for the Lila live coding TCP server binary.
 *
 * This program launches the Lila server, which enables interactive live coding sessions
 * over TCP for MayaFlux. It parses command-line options for port, verbosity, and log level,
 * sets up signal handling for graceful shutdown, and manages the main server loop.
 *
 * Usage:
 *   lila_server [OPTIONS]
 *
 * Options:
 *   -p, --port <port>     Server port (default: 9090)
 *   -v, --verbose         Enable verbose logging
 *   -l, --level <level>   Set log level (TRACE, DEBUG, INFO, WARN, ERROR, FATAL)
 *   -h, --help            Show help message
 */

#include "Lila/Commentator.hpp"
#include "Lila/Lila.hpp"

#include "Lila/EventBus.hpp"

#include <atomic>
#include <csignal>
#include <iostream>

/**
 * @brief Global flag to control server running state.
 */
std::atomic<bool> g_running { true };

/**
 * @brief Handles SIGINT and SIGTERM for graceful shutdown.
 * @param signal Signal number
 */
void signal_handler(int signal)
{
    if (signal == SIGINT || signal == SIGTERM) {
        LILA_INFO(Lila::Emitter::SYSTEM, "Received shutdown signal");
        g_running = false;
    }
}

/**
 * @brief Prints usage information for the server binary.
 * @param program_name Name of the executable
 */
void print_usage(const char* program_name)
{
    std::cout << "Usage: " << program_name << " [OPTIONS]\n"
              << "\nOptions:\n"
              << "  -p, --port <port>     Server port (default: 9090)\n"
              << "  -v, --verbose         Enable verbose logging\n"
              << "  -l, --level <level>   Set log level (TRACE, DEBUG, INFO, WARN, ERROR, FATAL)\n"
              << "  -h, --help            Show this help message\n"
              << "\nExamples:\n"
              << "  " << program_name << "                    # Start on default port 9090\n"
              << "  " << program_name << " -p 8080            # Start on port 8080\n"
              << "  " << program_name << " -v -l DEBUG        # Verbose mode with DEBUG level\n"
              << '\n';
}

/**
 * @brief Parses a string to a Lila::LogLevel value.
 * @param level_str Log level string
 * @return Corresponding LogLevel enum value
 */
Lila::LogLevel parse_log_level(const std::string& level_str)
{
    if (level_str == "TRACE")
        return Lila::LogLevel::TRACE;
    if (level_str == "DEBUG")
        return Lila::LogLevel::DEBUG;
    if (level_str == "INFO")
        return Lila::LogLevel::INFO;
    if (level_str == "WARN")
        return Lila::LogLevel::WARN;
    if (level_str == "ERROR")
        return Lila::LogLevel::ERROR;
    if (level_str == "FATAL")
        return Lila::LogLevel::FATAL;

    LILA_WARN(Lila::Emitter::SYSTEM,
        std::string("Unknown log level '") + level_str + "', using INFO");
    return Lila::LogLevel::INFO;
}

/**
 * @brief Main entry point for the Lila server binary.
 * @param argc Argument count
 * @param argv Argument vector
 * @return Exit code
 */
int main(int argc, char** argv)
{
    int port = 9090;
    bool verbose = false;
    Lila::LogLevel log_level = Lila::LogLevel::INFO;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }

        if (arg == "-p" || arg == "--port") {
            if (i + 1 < argc) {
                port = std::atoi(argv[++i]);
                if (port <= 0 || port > 65535) {
                    std::cerr << "Error: Invalid port number\n";
                    return 1;
                }
            } else {
                std::cerr << "Error: --port requires an argument\n";
                return 1;
            }
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if (arg == "-l" || arg == "--level") {
            if (i + 1 < argc) {
                log_level = parse_log_level(argv[++i]);
            } else {
                std::cerr << "Error: --level requires an argument\n";
                return 1;
            }
        } else {
            std::cerr << "Error: Unknown option '" << arg << "'\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    auto& logger = Lila::Commentator::instance();
    logger.set_level(log_level);
    logger.set_verbose(verbose);

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    LILA_INFO(Lila::Emitter::SYSTEM, "Starting Lila live coding server");
    LILA_INFO(Lila::Emitter::SYSTEM, std::string("Port: ") + std::to_string(port));

    std::string level_str;
    switch (log_level) {
    case Lila::LogLevel::TRACE:
        level_str = "TRACE";
        break;
    case Lila::LogLevel::DEBUG:
        level_str = "DEBUG";
        break;
    case Lila::LogLevel::INFO:
        level_str = "INFO";
        break;
    case Lila::LogLevel::WARN:
        level_str = "WARN";
        break;
    case Lila::LogLevel::ERROR:
        level_str = "ERROR";
        break;
    case Lila::LogLevel::FATAL:
        level_str = "FATAL";
        break;
    default:
        level_str = "UNKNOWN";
        break;
    }
    LILA_INFO(Lila::Emitter::SYSTEM, std::string("Log level: ") + level_str);

    if (verbose) {
        LILA_INFO(Lila::Emitter::SYSTEM, "Verbose mode enabled");
    }

    Lila::Lila playground;

    if (!playground.initialize(Lila::OperationMode::Server, port)) {
        LILA_FATAL(Lila::Emitter::SYSTEM,
            std::string("Failed to initialize: ") + playground.get_last_error());
        return 1;
    }

    playground.on_server_started([]() {
        std::cout << "LILA_SERVER_READY\n"
                  << std::flush;
        LILA_INFO(Lila::Emitter::SYSTEM, "Server is ready to accept connections");
    });

    playground.on_success([]() {
        LILA_INFO(Lila::Emitter::GENERAL, "Code evaluation succeeded");
    });

    /*
    playground.on_error([](const std::string& error) {
        LILA_ERROR(Lila::Emitter::GENERAL,
            std::string("Evaluation error: ") + error);
    });
    */

    playground.on_server_client_connected([](const Lila::ClientInfo& client) {
        LILA_INFO(Lila::Emitter::SERVER,
            std::string("New client connection (fd: ") + std::to_string(client.fd) + ", session: " + (client.session_id.empty() ? "none" : client.session_id) + ")");
    });

    playground.on_server_client_disconnected([](const Lila::ClientInfo& client) {
        LILA_INFO(Lila::Emitter::SERVER,
            std::string("Client disconnected (fd: ") + std::to_string(client.fd) + ", session: " + (client.session_id.empty() ? "none" : client.session_id) + ")");
    });

    LILA_INFO(Lila::Emitter::SYSTEM, "Server running. Press Ctrl+C to stop.");

    playground.await_shutdown(&g_running);

    LILA_INFO(Lila::Emitter::SYSTEM, "Shutting down...");
    playground.stop_server();
    LILA_INFO(Lila::Emitter::SYSTEM, "Goodbye!");

    return 0;
}
