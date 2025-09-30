#pragma once

#include "MayaFlux/EnumUtils.hpp"

#include "chrono"
#include "source_location"
#include "string_view"

namespace MayaFlux::Log {

enum class Severity : u_int8_t {
    TRACE,
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FATAL
};

enum class Component : u_int8_t {
    API,
    Buffers,
    Core,
    Kakshya,
    Kriya,
    Nodes,
    Vruta,
    Yantra,
    IO,
    Unknown
};

enum class Context : u_int8_t {
    Realtime, // Audio callback, render loop
    Worker, // Scheduled tasks
    UI, // User interface thread
    Init, // Startup/shutdown
    IO, // File/network operations
    Unknown
};

struct LogEntry {
    Severity severity;
    Component component;
    Context context;
    std::string_view message;
    std::source_location location;
    std::chrono::steady_clock::time_point timestamp;

    LogEntry(Severity sev, Component comp, Context ctx,
        std::string_view msg,
        std::source_location loc = std::source_location::current())
        : severity(sev)
        , component(comp)
        , context(ctx)
        , message(msg)
        , location(loc)
        , timestamp(std::chrono::steady_clock::now())
    {
    }

    // Convenience helpers for logging context
    static inline std::string severity_to_string(Severity sev)
    {
        return std::string(Utils::enum_to_string(sev));
    }

    static inline std::string component_to_string(Component comp)
    {
        return std::string(Utils::enum_to_string(comp));
    }

    static inline std::string context_to_string(Context ctx)
    {
        return std::string(Utils::enum_to_string(ctx));
    }
};

}
