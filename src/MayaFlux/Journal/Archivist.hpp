#pragma once

#include "Format.hpp"
#include "JournalEntry.hpp"

namespace MayaFlux::Journal {

class Sink;

/**
 * @class Archivist
 * @brief Singleton class responsible for managing log entries.
 *
 * The Archivist class provides methods to log messages with various severity levels,
 * components, and contexts. It supports both standard and real-time logging.
 */
class Archivist {

public:
    /**
     * @brief Get the singleton instance of the Archivist.
     * @return Reference to the Archivist instance.
     */
    static Archivist& instance();

    /**
     * @brief Initialize the logging system.
     * This should be called once at the start of the application.
     */
    static void init();

    /**
     * @brief Shutdown the logging system.
     * This should be called once at the end of the application.
     */
    static void shutdown();

    /**
     * @brief Log a message with the specified severity, component, and context.
     *
     * This method captures the source location automatically.
     *
     * @param severity The severity level of the log message.
     * @param component The component generating the log message.
     * @param context The execution context of the log message.
     * @param message The log message content.
     * @param location The source location (file, line, function) of the log call.
     */
    void scribe(Severity severity, Component component, Context context,
        std::string_view message,
        std::source_location location = std::source_location::current());

    /**
     * @brief Log a message from a real-time context with the specified severity, component, and context.
     * This method is optimized for real-time contexts and captures the source location automatically.
     * @param severity The severity level of the log message.
     * @param component The component generating the log message.
     * @param context The execution context of the log message.
     * @param message The log message content.
     * @param location The source location (file, line, function) of the log call.
     */
    void scribe_rt(Severity severity, Component component, Context context,
        std::string_view message,
        std::source_location location = std::source_location::current());

    /**
     * @brief Log a simple message without source location information.
     * This method is intended for use in contexts where source location is not available or needed.
     * @param severity The severity level of the log message.
     * @param component The component generating the log message.
     * @param context The execution context of the log message.
     * @param message The log message content.
     */
    void scribe_simple(Severity severity, Component component, Context context,
        std::string_view message);

    /**
     * @brief Add a log sink for output
     * @param sink Unique pointer to a LogSink implementation
     */
    void add_sink(std::unique_ptr<Sink> sink);

    /**
     * @brief Remove all sinks
     */
    void clear_sinks();

    /**
     * @brief Set the minimum severity level for logging.
     * Messages with a severity lower than this level will be ignored.
     * @param min_sev The minimum severity level to log.
     */
    void set_min_severity(Severity min_sev);

    /**
     * @brief Enable or disable logging for a specific component.
     * @param comp The component to enable or disable.
     * @param enabled True to enable logging for the component, false to disable.
     */
    void set_component_filter(Component comp, bool enabled);

    Archivist(const Archivist&) = delete;
    Archivist& operator=(const Archivist&) = delete;
    Archivist(Archivist&&) = delete;
    Archivist& operator=(Archivist&&) = delete;

private:
    Archivist();
    ~Archivist();

    class Impl;
    std::unique_ptr<Impl> m_impl;
};

/**
 * @brief Log a message with the specified severity, component, and context.
 *
 * Captures the source location automatically.
 *
 * @param severity   The severity level of the log message.
 * @param component  The component generating the log message.
 * @param context    The execution context of the log message.
 * @param location   Source location (file, line, function) of the log call.
 * @param message    The log message content.
 */
inline void scribe(Severity severity, Component component, Context context,
    std::source_location location, std::string_view message)
{
    Archivist::instance().scribe(severity, component, context, message, location);
}

/**
 * @brief printf-style overload of scribe().
 *
 * @copydoc scribe(Severity,Component,Context,std::string_view,std::source_location)
 *
 * @param msg_or_fmt  The format string.
 * @param args        The format arguments.
 */
template <typename... Args>
void scribe(Severity severity, Component component, Context context,
    std::source_location location, const char* msg_or_fmt, Args&&... args)
{
    if constexpr (sizeof...(Args) == 0) {
        Archivist::instance().scribe(severity, component, context,
            std::string_view(msg_or_fmt), location);
    } else {
        auto msg = format_runtime(msg_or_fmt, std::forward<Args>(args)...);
        Archivist::instance().scribe(severity, component, context, msg, location);
    }
}

/**
 * @brief Log a message in a real-time context with automatic source location.
 *
 * Optimized for real-time usage and captures the source location automatically.
 *
 * @param severity   The severity level of the log message.
 * @param component  The component generating the log message.
 * @param context    The execution context of the log message.
 * @param location   Source location (file, line, function) of the log call.
 * @param message    The log message content.
 */
inline void scribe_rt(Severity severity, Component component, Context context,
    std::source_location location, std::string_view message)
{
    Archivist::instance().scribe_rt(severity, component, context, message, location);
}

/**
 * @brief printf-style overload of scribe_rt().
 *
 * @copydoc scribe_rt(Severity,Component,Context,std::string_view,std::source_location)
 *
 * @param msg_or_fmt  The format string.
 * @param args        The format arguments.
 */
template <typename... Args>
void scribe_rt(Severity severity, Component component, Context context,
    std::source_location location,
    const char* msg_or_fmt, Args&&... args)
{
    if constexpr (sizeof...(Args) == 0) {
        Archivist::instance().scribe_rt(severity, component, context,
            std::string_view(msg_or_fmt), location);
    } else {
        auto msg = format_runtime(msg_or_fmt, std::forward<Args>(args)...);
        Archivist::instance().scribe_rt(severity, component, context, msg, location);
    }
}

/**
 * @brief Log a simple message without source-location.
 *
 * Intended for contexts where source location is unavailable or unnecessary.
 *
 * @param severity   The severity level of the log message.
 * @param component  The component generating the log message.
 * @param context    The execution context of the log message.
 * @param message    The log message content.
 */
inline void print(Severity severity, Component component, Context context,
    std::string_view message)
{
    Archivist::instance().scribe_simple(severity, component, context, message);
}

/**
 * @brief printf-style overload of print().
 *
 * @copydoc print(Severity,Component,Context,std::string_view)
 *
 * @param msg_or_fmt  The format string.
 * @param args        The format arguments.
 */
template <typename... Args>
void print(Severity severity, Component component, Context context,
    const char* msg_or_fmt, Args&&... args)
{
    if constexpr (sizeof...(Args) == 0) {
        Archivist::instance().scribe_simple(severity, component, context,
            std::string_view(msg_or_fmt));
    } else {
        auto msg = format_runtime(msg_or_fmt, std::forward<Args>(args)...);
        Archivist::instance().scribe_simple(severity, component, context, msg);
    }
}

/**
 * @brief Log a fatal message and abort the program.
 *
 * @param component  The component generating the log message.
 * @param context    The execution context of the log message.
 * @param location   Source location (file, line, function) of the log call.
 * @param message    The fatal message content.
 */
[[noreturn]] inline void fatal(Component component, Context context,
    std::source_location location, std::string_view message)
{
    Archivist::instance().scribe(Severity::FATAL, component, context, message, location);
    std::abort();
}

/**
 * @brief fmt-style overload of fatal().
 *
 * @copydoc fatal(Component,Context,std::string_view,std::source_location)
 *
 * @tparam Args      Types of the format arguments.
 * @param fmt_str    The format string.
 * @param args       The format arguments.
 */
template <typename... Args>
[[noreturn]] void fatal(Component component, Context context,
    std::source_location location, format_string<Args...> fmt_str, Args&&... args)
{
    auto msg = format(fmt_str, std::forward<Args>(args)...);
    Archivist::instance().scribe(Severity::FATAL, component, context, msg, location);
    std::abort();
}

/**
 * @brief Log an error message and optionally throw an exception.
 *
 * @tparam ExceptionType  The exception type to throw when behavior == LogAndThrow.
 * @param context    The execution context of the log message.
 * @param location   Source location (file, line, function) of the log call.
 * @param message    The error message content.
 */
template <typename ExceptionType = std::runtime_error>
void error(Component component, Context context,
    std::source_location location,
    std::string_view message)
{
    Archivist::instance().scribe(Severity::ERROR, component, context, message, location);

    throw ExceptionType(std::string(message));
}

/**
 * @brief fmt-style overload of error().
 *
 * @copydoc error(Component,Context,ExceptionBehavior,std::string_view,std::source_location)
 *
 * @tparam ExceptionType  The exception type to throw when behavior == LogAndThrow.
 * @tparam Args           Types of the format arguments.
 * @param fmt_str         The format string.
 * @param args            The format arguments.
 */
template <typename ExceptionType = std::runtime_error, typename... Args>
void error(Component component, Context context,
    std::source_location location, const char* fmt_str, Args&&... args)
{
    if constexpr (sizeof...(Args) == 0) {
        Archivist::instance().scribe(Severity::ERROR, component, context,
            std::string_view(fmt_str), location);
        throw ExceptionType(std::string(fmt_str));
    } else {
        auto msg = format_runtime(fmt_str, std::forward<Args>(args)...);
        Archivist::instance().scribe(Severity::ERROR, component, context, msg, location);
        throw ExceptionType(msg);
    }
}

/**
 * @brief Catch and log an exception, then rethrow it.
 * This function is intended to be called within a catch block.
 * @param Component The component generating the log message.
 * @param Context The execution context of the log message.
 * @param location The source location (file, line, function) of the log call.
 * @param additional_context Optional additional context to prepend to the exception message.
 */
inline void error_rethrow(Component component, Context context,
    std::source_location location = std::source_location::current(),
    std::string_view additional_context = "")
{
    try {
        throw;
    } catch (const std::exception& e) {
        std::string msg = std::string(e.what());
        if (!additional_context.empty()) {
            msg = std::string(additional_context) + ": " + msg;
        }
        Archivist::instance().scribe(Severity::ERROR, component, context, msg, location);
        throw;
    } catch (...) {
        std::string msg = "Unknown exception";
        if (!additional_context.empty()) {
            msg = std::string(additional_context) + ": " + msg;
        }
        Archivist::instance().scribe(Severity::ERROR, component, context, msg, location);
        throw;
    }
}

/**
 * @brief fmt-style overload of error_rethrow().
 *
 * @copydoc error_rethrow(Component,Context,std::source_location,std::string_view)
 *
 * @tparam Args      Types of the format arguments.
 * @param fmt_str    The format string.
 * @param args       The format arguments.
 */
template <typename... Args>
inline void error_rethrow(Component component, Context context,
    std::source_location location, const char* fmt_str, Args&&... args)
{
    if constexpr (sizeof...(Args) == 0) {
        error_rethrow(component, context, location,
            std::string_view(fmt_str));
        return;
    }
    auto additional_context = format_runtime(fmt_str, std::forward<Args>(args)...);
    error_rethrow(component, context, location, additional_context);
}

} // namespace MayaFlux::Journal

// ============================================================================
// CONVENIENCE MACROS (for regular logging only)
// ============================================================================

#define MF_TRACE(comp, ctx, ...)                                             \
    MayaFlux::Journal::scribe(MayaFlux::Journal::Severity::TRACE, comp, ctx, \
        std::source_location::current(), __VA_ARGS__)

#define MF_DEBUG(comp, ctx, ...)                                             \
    MayaFlux::Journal::scribe(MayaFlux::Journal::Severity::DEBUG, comp, ctx, \
        std::source_location::current(), __VA_ARGS__)

#define MF_INFO(comp, ctx, ...)                                             \
    MayaFlux::Journal::scribe(MayaFlux::Journal::Severity::INFO, comp, ctx, \
        std::source_location::current(), __VA_ARGS__)

#define MF_WARN(comp, ctx, ...)                                             \
    MayaFlux::Journal::scribe(MayaFlux::Journal::Severity::WARN, comp, ctx, \
        std::source_location::current(), __VA_ARGS__)

#define MF_ERROR(comp, ctx, ...)                                             \
    MayaFlux::Journal::scribe(MayaFlux::Journal::Severity::ERROR, comp, ctx, \
        std::source_location::current(), __VA_ARGS__)

// ============================================================================
// CONVENIENCE MACROS for REAL-TIME LOGGING ONLY
// ============================================================================

#define MF_RT_TRACE(comp, ctx, ...)                                             \
    MayaFlux::Journal::scribe_rt(MayaFlux::Journal::Severity::TRACE, comp, ctx, \
        std::source_location::current(), __VA_ARGS__)

#define MF_RT_WARN(comp, ctx, ...)                                             \
    MayaFlux::Journal::scribe_rt(MayaFlux::Journal::Severity::WARN, comp, ctx, \
        std::source_location::current(), __VA_ARGS__)

#define MF_RT_ERROR(comp, ctx, ...)                                             \
    MayaFlux::Journal::scribe_rt(MayaFlux::Journal::Severity::ERROR, comp, ctx, \
        std::source_location::current(), __VA_ARGS__)

// ============================================================================
// CONVENIENCE MACROS for SIMPLE PRINTING (no source-location)
// ============================================================================
#define MF_PRINT(comp, ctx, ...) \
    MayaFlux::Journal::print(MayaFlux::Journal::Severity::INFO, comp, ctx, __VA_ARGS__)
