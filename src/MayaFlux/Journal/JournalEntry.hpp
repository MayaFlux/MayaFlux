#pragma once

#include "MayaFlux/EnumUtils.hpp"

#include "chrono"
#include "source_location"
#include "string_view"

#ifdef MAYAFLUX_PLATFORM_WINDOWS
#ifdef ERROR
#undef ERROR
#endif // ERROR
#endif // MAYAFLUX_PLATFORM_WINDOWS

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
 * @enum Context
 * @brief Execution contexts for log messages.
 *
 * Represents the computational domain and thread context where a log message originates.
 * This enables context-aware filtering, real-time safety validation, and performance analysis.
 */
enum class Context : u_int8_t {
    // ============================================================================
    // REAL-TIME CONTEXTS (must never block, allocate, or throw)
    // ============================================================================

    AudioCallback, ///< Audio callback thread - strictest real-time requirements
    GraphicsCallback, ///< Graphics/visual rendering callback - frame-rate real-time
    Realtime, ///< Any real-time processing context (generic)

    // ============================================================================
    // BACKEND CONTEXTS
    // ============================================================================

    AudioBackend, ///< Audio processing backend (RtAudio, JACK, ASIO)
    GraphicsBackend, ///< Graphics/visual rendering backend (Vulkan, OpenGL)
    CustomBackend, ///< Custom user-defined backend

    // ============================================================================
    // SUBSYSTEM CONTEXTS
    // ============================================================================

    AudioSubsystem, ///< Audio subsystem operations (backend, device, stream management)
    WindowingSubsystem, ///< Windowing system operations (GLFW, SDL)
    GraphicsSubsystem, ///< Graphics subsystem operations (Vulkan, rendering pipeline)
    CustomSubsystem, ///< Custom user-defined subsystem

    // ============================================================================
    // PROCESSING CONTEXTS
    // ============================================================================

    NodeProcessing, ///< Node graph processing (Nodes::NodeGraphManager)
    BufferProcessing, ///< Buffer processing (Buffers::BufferManager, processing chains)
    CoroutineScheduling, ///< Coroutine scheduling and temporal coordination (Vruta::TaskScheduler)
    ContainerProcessing, ///< Container operations (Kakshya - file/stream/region processing)
    ComputeProcessing, ///< Compute operations (Yantra - algorithms, matrices, DSP)

    // ============================================================================
    // WORKER CONTEXTS
    // ============================================================================

    Worker, ///< Background worker thread (non-real-time scheduled tasks)
    AsyncIO, ///< Async I/O operations (file loading, network, streaming)
    BackgroundCompile, ///< Background compilation/optimization tasks

    // ============================================================================
    // LIFECYCLE CONTEXTS
    // ============================================================================

    Init, ///< Engine/subsystem initialization
    Shutdown, ///< Engine/subsystem shutdown and cleanup
    Configuration, ///< Configuration and parameter updates

    // ============================================================================
    // USER INTERACTION CONTEXTS
    // ============================================================================

    UI, ///< User interface thread (UI events, rendering)
    UserCode, ///< User script/plugin execution
    Interactive, ///< Interactive shell/REPL

    // ============================================================================
    // COORDINATION CONTEXTS
    // ============================================================================

    CrossSubsystem, ///< Cross-subsystem data sharing and synchronization
    ClockSync, ///< Clock synchronization (SampleClock, FrameClock coordination)
    EventDispatch, ///< Event dispatching and coordination

    // ============================================================================
    // SPECIAL CONTEXTS
    // ============================================================================

    Runtime, ///< General runtime operations (default fallback)
    Testing, ///< Testing/benchmarking context
    Unknown ///< Unknown or unspecified context
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
