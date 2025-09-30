#pragma once

#include "Format.hpp"
#include "LogEntry.hpp"

namespace MayaFlux::Log {

class Logger {
public:
    static Logger& instance();
    static void init();
    static void shutdown();

    void log(Severity severity, Component component, Context context,
        std::string_view message,
        std::source_location location = std::source_location::current());

    void log_rt(Severity severity, Component component, Context context,
        std::string_view message,
        std::source_location location = std::source_location::current());

    void set_min_severity(Severity min_sev);
    void set_component_filter(Component comp, bool enabled);

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    Logger();
    ~Logger();

    class Impl;
    std::unique_ptr<Impl> impl_;
};

inline void log(Severity severity, Component component, Context context,
    std::string_view message,
    std::source_location location = std::source_location::current())
{
    Logger::instance().log(severity, component, context, message, location);
}

inline void log_rt(Severity severity, Component component, Context context,
    std::string_view message,
    std::source_location location = std::source_location::current())
{
    Logger::instance().log_rt(severity, component, context, message, location);
}

template <typename... Args>
void logf(Severity severity, Component component, Context context,
    format_string<Args...> fmt_str, Args&&... args,
    std::source_location location = std::source_location::current())
{
    auto msg = format(fmt_str, std::forward<Args>(args)...);
    Logger::instance().log(severity, component, context, msg, location);
}

template <typename... Args>
[[noreturn]] void fatal(Component component, Context context,
    format_string<Args...> fmt_str, Args&&... args,
    std::source_location location = std::source_location::current())
{
    auto msg = format(fmt_str, std::forward<Args>(args)...);
    Logger::instance().log(Severity::FATAL, component, context, msg, location);
    std::abort();
}

} // namespace MayaFlux::Log

#define MFLOG_TRACE(comp, ctx, msg) MayaFlux::Log::log(::MayaFlux::Log::Severity::TRACE, comp, ctx, msg)
#define MFLOG_DEBUG(comp, ctx, msg) MayaFlux::Log::log(::MayaFlux::Log::Severity::DEBUG, comp, ctx, msg)
#define MFLOG_INFO(comp, ctx, msg) MayaFlux::Log::log(::MayaFlux::Log::Severity::INFO, comp, ctx, msg)
#define MFLOG_WARN(comp, ctx, msg) MayaFlux::Log::log(::MayaFlux::Log::Severity::WARN, comp, ctx, msg)
#define MFLOG_ERROR(comp, ctx, msg) MayaFlux::Log::log(::MayaFlux::Log::Severity::ERROR, comp, ctx, msg)

#define MFLOG_RT_TRACE(comp, ctx, msg) MayaFlux::Log::log_rt(::MayaFlux::Log::Severity::TRACE, comp, ctx, msg)
#define MFLOG_RT_WARN(comp, ctx, msg) MayaFlux::Log::log_rt(::MayaFlux::Log::Severity::WARN, comp, ctx, msg)
#define MFLOG_RT_ERROR(comp, ctx, msg) MayaFlux::Log::log_rt(::MayaFlux::Log::Severity::ERROR, comp, ctx, msg)

#define MFLOGF_INFO(comp, ctx, fmt, ...) MayaFlux::Log::logf(::MayaFlux::Log::Severity::INFO, comp, ctx, fmt, __VA_ARGS__)
#define MFLOGF_WARN(comp, ctx, fmt, ...) MayaFlux::Log::logf(::MayaFlux::Log::Severity::WARN, comp, ctx, fmt, __VA_ARGS__)
#define MFLOGF_ERROR(comp, ctx, fmt, ...) MayaFlux::Log::logf(::MayaFlux::Log::Severity::ERROR, comp, ctx, fmt, __VA_ARGS__)
