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

enum class LogLevel : uint8_t {
    TRACE,
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FATAL
};

enum class Emitter : uint8_t {
    SERVER, ///< TCP server, connection handling
    INTERPRETER, ///< Clang interpreter, compilation, symbol resolution
    SYSTEM, ///< System-level operations, initialization
    GENERAL ///< General/uncategorized
};

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

    void set_verbose(bool verbose)
    {
        m_verbose = verbose;
    }

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

    void trace(Emitter source, std::string_view message,
        std::source_location location = std::source_location::current())
    {
        announce(LogLevel::TRACE, source, message, location);
    }

    void debug(Emitter source, std::string_view message,
        std::source_location location = std::source_location::current())
    {
        announce(LogLevel::DEBUG, source, message, location);
    }

    void info(Emitter source, std::string_view message,
        std::source_location location = std::source_location::current())
    {
        announce(LogLevel::INFO, source, message, location);
    }

    void warn(Emitter source, std::string_view message,
        std::source_location location = std::source_location::current())
    {
        announce(LogLevel::WARN, source, message, location);
    }

    void error(Emitter source, std::string_view message,
        std::source_location location = std::source_location::current())
    {
        announce(LogLevel::ERROR, source, message, location);
    }

    void fatal(Emitter source, std::string_view message,
        std::source_location location = std::source_location::current())
    {
        announce(LogLevel::FATAL, source, message, location);
    }

    Commentator(const Commentator&) = delete;
    Commentator& operator=(const Commentator&) = delete;

private:
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
