#pragma once

#include <source_location>
#include <string_view>

#ifdef MAYAFLUX_PLATFORM_WINDOWS
#include <windows.h>
#ifdef ERROR
#undef ERROR
#endif // ERROR
#endif // MAYAFLUX_PLATFORM_WINDOWS

namespace Lila {

/**
 * @namespace Colors
 * @brief ANSI color codes for terminal output.
 *
 * Provides color codes for use in log messages. Automatically enabled on supported platforms.
 */
namespace Colors {
    constexpr std::string_view Reset = "\033[0m";
    constexpr std::string_view Red = "\033[31m";
    constexpr std::string_view Green = "\033[32m";
    constexpr std::string_view Yellow = "\033[33m";
    constexpr std::string_view Blue = "\033[34m";
    constexpr std::string_view Magenta = "\033[35m";
    constexpr std::string_view Cyan = "\033[36m";
    constexpr std::string_view BrightRed = "\033[91m";
    constexpr std::string_view BrightBlue = "\033[94m";
}

/**
 * @enum LogLevel
 * @brief Log severity levels for Commentator.
 *
 * Controls filtering and formatting of log output.
 */
enum class LogLevel : uint8_t {
    TRACE, ///< Fine-grained debug information
    DEBUG, ///< Debug-level information
    INFO, ///< Informational messages
    WARN, ///< Warnings
    ERROR, ///< Errors
    FATAL ///< Fatal errors
};

/**
 * @enum Emitter
 * @brief Source category for log messages.
 *
 * Used to indicate which subsystem emitted the log.
 */
enum class Emitter : uint8_t {
    SERVER, ///< TCP server, connection handling
    INTERPRETER, ///< Clang interpreter, compilation, symbol resolution
    SYSTEM, ///< System-level operations, initialization
    GENERAL ///< General/uncategorized
};

/**
 * @class Commentator
 * @brief Centralized, thread-safe logging and announcement system for Lila.
 *
 * The Commentator class provides formatted, colorized, and categorized logging for all Lila subsystems.
 * It supports log levels, verbosity, and source tagging, and is designed for both interactive and automated use.
 *
 * ## Core Features
 * - Singleton access via Commentator::instance()
 * - Log level filtering and verbosity control
 * - Colorized output (ANSI codes, auto-enabled on supported platforms)
 * - Source location reporting (file, line) for verbose/error/fatal logs
 * - Thread-safe logging (mutex-protected)
 * - Macros for convenient logging (LILA_INFO, LILA_ERROR, etc.)
 *
 * ## Usage Example
 * ```cpp
 * LILA_INFO(Lila::Emitter::SYSTEM, "Server started");
 * LILA_ERROR(Lila::Emitter::INTERPRETER, "Compilation failed");
 * Lila::Commentator::instance().set_level(Lila::LogLevel::DEBUG);
 * Lila::Commentator::instance().set_verbose(true);
 * ```
 *
 * Commentator is intended for internal use by Lila and its binaries.
 */
class Commentator {
public:
    static Commentator& instance()
    {
        static Commentator commentary;
        return commentary;
    }

    void set_level(LogLevel level)
    {
        std::lock_guard lock(m_mutex);
        m_min_level = level;
    }

    /**
     * @brief Enables or disables verbose output (shows source location).
     * @param verbose True to enable verbose mode.
     */
    void set_verbose(bool verbose)
    {
        m_verbose = verbose;
    }

    /**
     * @brief Announces a log message with full control.
     * @param level Log severity.
     * @param source Emitter subsystem.
     * @param message Log message.
     * @param location Source location (auto-filled).
     */
    void announce(LogLevel level, Emitter source, std::string_view message,
        std::source_location location = std::source_location::current())
    {
        if (level < m_min_level)
            return;

        std::lock_guard lock(m_mutex);

        auto use_color = [this](std::string_view color_code) {
            return m_colors_enabled ? color_code : "";
        };

        std::cout << "        "; // 8 spaces indent

        std::cout << use_color(Colors::BrightBlue)
                  << "▶ LILA "
                  << use_color(Colors::Reset);

        std::cout << use_color(level_color(level))
                  << "|" << level_string(level) << "| "
                  << use_color(Colors::Reset);

        std::cout << use_color(Colors::Cyan)
                  << emitter_string(source) << " → "
                  << use_color(Colors::Reset);

        std::cout << message;

        if (m_verbose || level >= LogLevel::ERROR) {
            if (location.file_name() != nullptr) {
                std::cout << use_color(Colors::BrightBlue)
                          << " (" << extract_filename(location.file_name())
                          << ":" << location.line() << ")"
                          << use_color(Colors::Reset);
            }
        }

        std::cout << "\n";
    }

    /**
     * @brief Logs a TRACE-level message.
     * @param source Emitter subsystem.
     * @param message Log message.
     * @param location Source location (auto-filled).
     */
    void trace(Emitter source, std::string_view message,
        std::source_location location = std::source_location::current())
    {
        announce(LogLevel::TRACE, source, message, location);
    }

    /**
     * @brief Logs a DEBUG-level message.
     * @param source Emitter subsystem.
     * @param message Log message.
     * @param location Source location (auto-filled).
     */
    void debug(Emitter source, std::string_view message,
        std::source_location location = std::source_location::current())
    {
        announce(LogLevel::DEBUG, source, message, location);
    }

    /**
     * @brief Logs an INFO-level message.
     * @param source Emitter subsystem.
     * @param message Log message.
     * @param location Source location (auto-filled).
     */
    void info(Emitter source, std::string_view message,
        std::source_location location = std::source_location::current())
    {
        announce(LogLevel::INFO, source, message, location);
    }

    /**
     * @brief Logs a WARN-level message.
     * @param source Emitter subsystem.
     * @param message Log message.
     * @param location Source location (auto-filled).
     */
    void warn(Emitter source, std::string_view message,
        std::source_location location = std::source_location::current())
    {
        announce(LogLevel::WARN, source, message, location);
    }

    /**
     * @brief Logs an ERROR-level message.
     * @param source Emitter subsystem.
     * @param message Log message.
     * @param location Source location (auto-filled).
     */
    void error(Emitter source, std::string_view message,
        std::source_location location = std::source_location::current())
    {
        announce(LogLevel::ERROR, source, message, location);
    }

    /**
     * @brief Logs a FATAL-level message.
     * @param source Emitter subsystem.
     * @param message Log message.
     * @param location Source location (auto-filled).
     */
    void fatal(Emitter source, std::string_view message,
        std::source_location location = std::source_location::current())
    {
        announce(LogLevel::FATAL, source, message, location);
    }

    Commentator(const Commentator&) = delete;
    Commentator& operator=(const Commentator&) = delete;

private:
    /**
     * @brief Initializes console color support (platform-specific).
     * @return True if colors are enabled.
     */
    Commentator()
        : m_colors_enabled(initialize_console_colors())
    {
    }

    std::mutex m_mutex;
    LogLevel m_min_level = LogLevel::INFO;
    bool m_verbose = false;
    bool m_colors_enabled;

    static bool initialize_console_colors()
    {
#ifdef _WIN32
        // Enable ANSI escape sequences on Windows 10+
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut == INVALID_HANDLE_VALUE) {
            return false;
        }

        DWORD dwMode = 0;
        if (!GetConsoleMode(hOut, &dwMode)) {
            return false;
        }

        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        if (!SetConsoleMode(hOut, dwMode)) {
            // Fallback: colors not supported on this Windows version
            return false;
        }
        return true;
#else
        // Unix/Linux/macOS: ANSI colors always supported
        return true;
#endif
    }

    /**
     * @brief Gets the ANSI color code for a log level.
     * @param level LogLevel value.
     * @return Color code string.
     */
    static constexpr std::string_view level_color(LogLevel level)
    {
        switch (level) {
        case LogLevel::TRACE:
            return Colors::Cyan;
        case LogLevel::DEBUG:
            return Colors::Blue;
        case LogLevel::INFO:
            return Colors::Green;
        case LogLevel::WARN:
            return Colors::Yellow;
        case LogLevel::ERROR:
        case LogLevel::FATAL:
            return Colors::BrightRed;
        default:
            return Colors::Reset;
        }
    }

    /**
     * @brief Gets the string representation of a log level.
     * @param level LogLevel value.
     * @return Log level string.
     */
    static constexpr std::string_view level_string(LogLevel level)
    {
        switch (level) {
        case LogLevel::TRACE:
            return "TRACE";
        case LogLevel::DEBUG:
            return "DEBUG";
        case LogLevel::INFO:
            return "INFO";
        case LogLevel::WARN:
            return "WARN";
        case LogLevel::ERROR:
            return "ERROR";
        case LogLevel::FATAL:
            return "FATAL";
        default:
            return "UNKNOWN";
        }
    }

    /**
     * @brief Gets the string representation of an emitter.
     * @param source Emitter value.
     * @return Emitter string.
     */
    static constexpr std::string_view emitter_string(Emitter source)
    {
        switch (source) {
        case Emitter::SERVER:
            return "SERVER";
        case Emitter::INTERPRETER:
            return "INTERP";
        case Emitter::SYSTEM:
            return "SYSTEM";
        case Emitter::GENERAL:
            return "GENERAL";
        default:
            return "UNKNOWN";
        }
    }

    /**
     * @brief Extracts the filename from a file path.
     * @param path File path.
     * @return Filename string view.
     */
    static std::string_view extract_filename(const char* path)
    {
        std::string_view sv(path);
        auto pos = sv.find_last_of("/\\");
        return (pos == std::string_view::npos) ? sv : sv.substr(pos + 1);
    }
};

} // namespace Lila

#define LILA_TRACE(emitter, msg) ::Lila::Commentator::instance().trace(emitter, msg)
#define LILA_DEBUG(emitter, msg) ::Lila::Commentator::instance().debug(emitter, msg)
#define LILA_INFO(emitter, msg) ::Lila::Commentator::instance().info(emitter, msg)
#define LILA_WARN(emitter, msg) ::Lila::Commentator::instance().warn(emitter, msg)
#define LILA_ERROR(emitter, msg) ::Lila::Commentator::instance().error(emitter, msg)
#define LILA_FATAL(emitter, msg) ::Lila::Commentator::instance().fatal(emitter, msg)
