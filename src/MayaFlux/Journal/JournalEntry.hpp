#pragma once

#include "MayaFlux/EnumUtils.hpp"

#include "chrono"
#include "source_location"
#include "string_view"

namespace MayaFlux::Journal {

/**
 * @enum Log Severity
 * @brief Severity levels for log messages.
 */
enum class Severity : u_int8_t {
    TRACE,
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FATAL
};

/**
 * @enum Namespace Component
 * @brief Components of the system for categorizing log messages.
 */
enum class Component : u_int8_t {
    API, ///< MayaFlux/API Wrapper and convenience functions
    Buffers, ///< Buffers, Managers, processors and processing chains
    Core, ///< Core engine, backend, subsystems
    Kakshya, ///< Containers[Signalsource, Stream, File], Regions, DataProcessors
    Kriya, ///< Automatable tasks and fluent scheduling api for Nodes and Buffers
    Nodes, ///< DSP Generator and Filter Nodes, graph pipeline, node management
    Vruta, ///< Coroutines, schedulers, clocks, task management
    Yantra, ///< DSP algorithms, computational units, matrix operations, Grammar
    IO, ///< Networking, file handling, streaming
    USER, ///< User code, scripts, plugins
    Unknown
};

/**
 * @enum Log Context
 * @brief Execution contexts for log messages.
 */
enum class Context : u_int8_t {
    Realtime, ///< Audio callback, render loop
    Worker, ///< Scheduled tasks
    UI, ///< User interface thread
    Init, ///< Startup/shutdown
    IO, ///< File/network operations
    Unknown
};

/**
 * @brief A log entry structure to encapsulate log message details.
 */
struct JournalEntry {
    Severity severity;
    Component component;
    Context context;
    std::string_view message;
    std::source_location location;
    std::chrono::steady_clock::time_point timestamp;

    JournalEntry(Severity sev, Component comp, Context ctx,
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
