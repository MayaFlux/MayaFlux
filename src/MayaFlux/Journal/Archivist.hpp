#pragma once

#include "Format.hpp"
#include "JournalEntry.hpp"

namespace MayaFlux::Journal {

class Archivist {

public:
    static Archivist& instance();
    static void init();
    static void shutdown();

    void scribe(Severity severity, Component component, Context context,
        std::string_view message,
        std::source_location location = std::source_location::current());

    void scribe_rt(Severity severity, Component component, Context context,
        std::string_view message,
        std::source_location location = std::source_location::current());

    void set_min_severity(Severity min_sev);
    void set_component_filter(Component comp, bool enabled);

    Archivist(const Archivist&) = delete;
    Archivist& operator=(const Archivist&) = delete;
    Archivist(Archivist&&) = delete;
    Archivist& operator=(Archivist&&) = delete;

private:
    Archivist();
    ~Archivist();

    class Impl;
    std::unique_ptr<Impl> impl_;
};

inline void scribe(Severity severity, Component component, Context context,
    std::string_view message,
    std::source_location location = std::source_location::current())
{
    Archivist::instance().scribe(severity, component, context, message, location);
}

inline void scribe_rt(Severity severity, Component component, Context context,
    std::string_view message,
    std::source_location location = std::source_location::current())
{
    Archivist::instance().scribe_rt(severity, component, context, message, location);
}

template <typename... Args>
void scribef(Severity severity, Component component, Context context,
    format_string<Args...> fmt_str, Args&&... args,
    std::source_location location = std::source_location::current())
{
    auto msg = format(fmt_str, std::forward<Args>(args)...);
    Archivist::instance().scribe(severity, component, context, msg, location);
}

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

#define MFF_INFO(comp, ctx, fmt, ...) MayaFlux::Journal::scribef(MayaFlux::Journal::Severity::INFO, comp, ctx, fmt, __VA_ARGS__)
#define MFF_WARN(comp, ctx, fmt, ...) MayaFlux::Journal::scribef(MayaFlux::Journal::Severity::WARN, comp, ctx, fmt, __VA_ARGS__)
#define MFF_ERROR(comp, ctx, fmt, ...) MayaFlux::Journal::scribef(MayaFlux::Journal::Severity::ERROR, comp, ctx, fmt, __VA_ARGS__)
