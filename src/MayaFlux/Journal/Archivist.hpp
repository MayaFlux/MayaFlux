#pragma once

#include "Format.hpp"
#include "JournalEntry.hpp"

namespace MayaFlux::Journal {

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
 * This function captures the source location automatically.
 *
 * @param severity The severity level of the log message.
 * @param component The component generating the log message.
 * @param context The execution context of the log message.
 * @param message The log message content.
 * @param location The source location (file, line, function) of the log call.
 */
inline void scribe(Severity severity, Component component, Context context,
    std::string_view message,
    std::source_location location = std::source_location::current())
{
    Archivist::instance().scribe(severity, component, context, message, location);
}

/**
 * @brief Log a message from a real-time context with the specified severity, component, and context.
 *
 * This function is optimized for real-time contexts and captures the source location automatically.
 *
 * @param severity The severity level of the log message.
 * @param component The component generating the log message.
 * @param context The execution context of the log message.
 * @param message The log message content.
 * @param location The source location (file, line, function) of the log call.
 */
inline void scribe_rt(Severity severity, Component component, Context context,
    std::string_view message,
    std::source_location location = std::source_location::current())
{
    Archivist::instance().scribe_rt(severity, component, context, message, location);
}

/**
 * @brief Log a formatted message with the specified severity, component, and context.
 *
 * This function captures the source location automatically and uses fmt library for formatting.
 *
 * @tparam Args The types of the format arguments.
 * @param severity The severity level of the log message.
 * @param component The component generating the log message.
 * @param context The execution context of the log message.
 * @param fmt_str The format string.
 * @param args The format arguments.
 * @param location The source location (file, line, function) of the log call.
 */
template <typename... Args>
void scribef(Severity severity, Component component, Context context,
    format_string<Args...> fmt_str, Args&&... args,
    std::source_location location = std::source_location::current())
{
    auto msg = format(fmt_str, std::forward<Args>(args)...);
    Archivist::instance().scribe(severity, component, context, msg, location);
}

template <typename... Args>
void scribef(Severity severity, Component component, Context context,
    const char* fmt_str,
    std::source_location location,
    Args&&... args)
{
    auto msg = format_runtime(fmt_str, std::forward<Args>(args)...);
    Archivist::instance().scribe(severity, component, context, msg, location);
}

/**
 * @brief Log a formatted message from a real-time context with the specified severity, component, and context.
 *
 * This function is optimized for real-time contexts, captures the source location automatically,
 * and uses fmt library for formatting.
 *
 * @tparam Args The types of the format arguments.
 * @param severity The severity level of the log message.
 * @param component The component generating the log message.
 * @param context The execution context of the log message.
 * @param fmt_str The format string.
 * @param args The format arguments.
 * @param location The source location (file, line, function) of the log call.
 */
template <typename... Args>
[[noreturn]] void fatal(Component component, Context context,
    format_string<Args...> fmt_str, Args&&... args,
    std::source_location location = std::source_location::current())
{
    auto msg = format(fmt_str, std::forward<Args>(args)...);
    Archivist::instance().scribe(Severity::FATAL, component, context, msg, location);
    std::abort();
}

} // namespace MayaFlux::Journal

#define MF_TRACE(comp, ctx, msg) MayaFlux::Journal::scribe(MayaFlux::Journal::Severity::TRACE, comp, ctx, msg)
#define MF_DEBUG(comp, ctx, msg) MayaFlux::Journal::scribe(MayaFlux::Journal::Severity::DEBUG, comp, ctx, msg)
#define MF_INFO(comp, ctx, msg) MayaFlux::Journal::scribe(MayaFlux::Journal::Severity::INFO, comp, ctx, msg)
#define MF_WARN(comp, ctx, msg) MayaFlux::Journal::scribe(MayaFlux::Journal::Severity::WARN, comp, ctx, msg)
#define MF_ERROR(comp, ctx, msg) MayaFlux::Journal::scribe(MayaFlux::Journal::Severity::ERROR, comp, ctx, msg)

#define MF_RT_TRACE(comp, ctx, msg) MayaFlux::Journal::scribe_rt(MayaFlux::Journal::Severity::TRACE, comp, ctx, msg)
#define MF_RT_WARN(comp, ctx, msg) MayaFlux::Journal::scribe_rt(MayaFlux::Journal::Severity::WARN, comp, ctx, msg)
#define MF_RT_ERROR(comp, ctx, msg) MayaFlux::Journal::scribe_rt(MayaFlux::Journal::Severity::ERROR, comp, ctx, msg)

#define MFF_TRACE(comp, ctx, fmt, ...)                                             \
    MayaFlux::Journal::scribef(MayaFlux::Journal::Severity::TRACE, comp, ctx, fmt, \
        std::source_location::current() __VA_OPT__(, ) __VA_ARGS__)
#define MFF_DEBUG(comp, ctx, fmt, ...)                                             \
    MayaFlux::Journal::scribef(MayaFlux::Journal::Severity::DEBUG, comp, ctx, fmt, \
        std::source_location::current() __VA_OPT__(, ) __VA_ARGS__)
#define MFF_INFO(comp, ctx, fmt, ...)                                             \
    MayaFlux::Journal::scribef(MayaFlux::Journal::Severity::INFO, comp, ctx, fmt, \
        std::source_location::current() __VA_OPT__(, ) __VA_ARGS__)
#define MFF_WARN(comp, ctx, fmt, ...)                                             \
    MayaFlux::Journal::scribef(MayaFlux::Journal::Severity::WARN, comp, ctx, fmt, \
        std::source_location::current() __VA_OPT__(, ) __VA_ARGS__)
#define MFF_ERROR(comp, ctx, fmt, ...)                                             \
    MayaFlux::Journal::scribef(MayaFlux::Journal::Severity::ERROR, comp, ctx, fmt, \
        std::source_location::current() __VA_OPT__(, ) __VA_ARGS__)
