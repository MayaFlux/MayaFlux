#pragma once

#include "SubsystemManager.hpp"

#include "GlobalGraphicsInfo.hpp"
#include "GlobalStreamInfo.hpp"

namespace MayaFlux::Nodes::Generator::Stochastics {
class Random;
}

namespace MayaFlux::Vruta {
class EventManager;
}

namespace MayaFlux::Core {

class WindowManager;

// struct GlobalEngineInfo {
//     GlobalStreamInfo audio;
//     GlobalWindowInfo window;
// };

/**
 * @class Engine
 * @brief Central lifecycle manager and component orchestrator for the MayaFlux processing system
 *
 * The Engine serves as the primary entry point and lifecycle coordinator for MayaFlux, acting as:
 * - **Lifecycle Manager**: Controls initialization, startup, pause/resume, and shutdown sequences
 * - **Component Initializer**: Creates and configures core system components with proper dependencies
 * - **Access Router**: Provides centralized access to all major subsystems and managers
 * - **Reference Holder**: Maintains shared ownership of core components to ensure proper lifetime management
 *
 * **Core Responsibilities:**
 * 1. **System Initialization**: Orchestrates the creation and configuration of all core components
 * 2. **Lifecycle Control**: Manages the start/stop/pause/resume cycle of the entire processing system
 * 3. **Component Access**: Provides unified access to subsystems (audio, scheduling, node graph, buffers)
 * 4. **Resource Management**: Ensures proper construction/destruction order and shared ownership
 *
 * **Architecture Philosophy:**
 * The Engine follows a "batteries included but replaceable" approach:
 * - Provides sensible defaults and automatic component wiring for ease of use
 * - Allows advanced users to access individual components directly for custom workflows
 * - Enables completely custom component instantiation when needed
 *
 * **Usage Patterns:**
 *
 * *Simple Usage (Recommended):*
 * ```cpp
 * Engine engine;
 * engine.Init(48000, 512, 2, 0);  // 48kHz, 512 samples, stereo out
 * engine.Start();
 * // Use engine.get_scheduler(), engine.get_node_graph_manager(), etc.
 * ```
 *
 * *Advanced Usage:*
 * ```cpp
 * Engine engine;
 * auto custom_scheduler = std::make_shared<CustomScheduler>();
 * engine.Init(stream_info);
 * // Replace default scheduler with custom implementation
 * engine.get_scheduler() = custom_scheduler;
 * ```
 *
 * *Offline Processing:*
 * ```cpp
 * // Engine components can be used without hardware I/O
 * auto scheduler = engine.get_scheduler();
 * auto node_graph = engine.get_node_graph_manager();
 * // Process manually without Start()
 * ```
 *
 * The Engine does not perform direct signal processing or scheduling - it delegates these
 * responsibilities to specialized subsystems while ensuring they work together coherently.
 */
class MAYAFLUX_API Engine {
public:
    //-------------------------------------------------------------------------
    // Initialization and Lifecycle
    //-------------------------------------------------------------------------

    /**
     * @brief Constructs a new Engine instance
     * @param type The type of audio backend to use (default: RtAudio)
     *
     * Creates a new Engine instance with the specified audio backend.
     * The backend type determines the underlying audio API used for device management
     * and stream processing.
     */
    // Engine(Utils::AudioBackendType audio_type = Utils::AudioBackendType::RTAUDIO);
    Engine();

    /**
     * @brief Destroys the Engine instance and cleans up resources
     *
     * Stops and closes any active streams and frees allocated resources.
     */
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    /**
     * @brief Move constructor
     * @param other Engine instance to move from
     */
    Engine(Engine&& other) noexcept;

    /**
     * @brief Move assignment operator
     * @param other Engine instance to move from
     * @return Reference to this Engine
     */
    Engine& operator=(Engine&& other) noexcept;

    /**
     * @brief Initializes all system components and prepares for processing
     *
     * Orchestrates the initialization sequence for all core components:
     * - Creates and configures the task scheduler with the specified sample rate
     * - Initializes the node graph manager and buffer manager
     * - Sets up subsystem managers and audio backend
     * - Establishes proper component interconnections
     *
     * This method must be called before Start().
     */
    void Init();

    /**
     * @brief Initializes the processing engine with a custom stream configuration
     * @param streamInfo Configuration for sample rate, buffer size, and channels
     *
     * Configures the processing engine with the specified stream information.
     * This method must be called before starting the engine.
     */
    void Init(const GlobalStreamInfo& streamInfo);

    /**
     * @brief Initializes the processing engine with custom stream and graphics configurations
     * @param streamInfo Configuration for sample rate, buffer size, and channels
     * @param graphics_config Configuration for graphics/windowing backend
     *
     * Configures the processing engine with the specified stream and graphics information.
     * This method must be called before starting the engine.
     */
    void Init(const GlobalStreamInfo& streamInfo, const GlobalGraphicsConfig& graphics_config);

    /**
     * @brief Starts the coordinated processing of all subsystems
     *
     * Initiates the processing lifecycle by:
     * - Starting the audio backend and opening streams
     * - Beginning task scheduler execution
     * - Activating node graph processing
     * - Enabling real-time audio I/O
     *
     * Init() must be called first to prepare all components.
     */
    void Start();

    /**
     * @brief Pauses all processing while maintaining system state
     *
     * Temporarily halts processing activities:
     * - Pauses the audio stream
     * - Suspends task scheduler execution
     * - Maintains all component state for later resumption
     */
    void Pause();

    /**
     * @brief Resumes processing from paused state
     *
     * Restarts all processing activities:
     * - Resumes the audio stream
     * - Reactivates task scheduler
     * - Continues from the exact state when paused
     */
    void Resume();

    /**
     * @brief Stops all processing and performs clean shutdown
     *
     * Orchestrates the shutdown sequence:
     * - Terminates all active tasks and coroutines
     * - Stops and closes audio streams
     * - Releases all resources and buffers
     * - Resets components to uninitialized state
     */
    void End();

    /**
     * @brief Checks if the coordinated processing system is currently active
     * @return true if all subsystems are running and processing, false otherwise
     *
     * This reflects the overall system state - true only when the audio stream
     * is active, schedulers are running, and the system is processing data.
     */
    bool is_running() const;

    //-------------------------------------------------------------------------
    // Configuration Access
    //-------------------------------------------------------------------------

    /**
     * @brief Gets the current stream configuration
     * @return Reference to the GlobalStreamInfo struct
     */
    inline GlobalStreamInfo& get_stream_info() { return m_stream_info; }

    /**
     * @brief Gets the current graphics configuration
     * @return Reference to the GlobalGraphicsConfig struct
     */
    inline GlobalGraphicsConfig& get_graphics_config() { return m_graphics_config; }

    //-------------------------------------------------------------------------
    // Component Access - Engine acts as access router to all subsystems
    //-------------------------------------------------------------------------

    /**
     * @brief Gets the node graph manager
     * @return Shared pointer to the NodeGraphManager for node-based processing
     *
     * The NodeGraphManager handles the computational graph of processing nodes.
     * Access through Engine ensures proper initialization and lifetime management.
     */
    inline std::shared_ptr<Nodes::NodeGraphManager> get_node_graph_manager() { return m_node_graph_manager; }

    /**
     * @brief Gets the task scheduler
     * @return Shared pointer to the TaskScheduler for coroutine-based timing
     *
     * The TaskScheduler manages sample-accurate timing and coroutine execution.
     * Access through Engine ensures proper clock synchronization with audio.
     */
    inline std::shared_ptr<Vruta::TaskScheduler> get_scheduler() { return m_scheduler; }

    /**
     * @brief Gets the buffer manager
     * @return Shared pointer to the BufferManager for memory management
     *
     * The BufferManager handles efficient allocation and reuse of audio buffers.
     * Access through Engine ensures buffers are sized correctly for the stream.
     */
    inline std::shared_ptr<Buffers::BufferManager> get_buffer_manager() { return m_buffer_manager; }

    /**
     * @brief Gets the window manager
     * @return Shared pointer to the WindowManager for windowing operations
     *
     * The WindowManager handles creation and management of application windows.
     * Access through Engine ensures proper graphics backend initialization.
     */
    inline std::shared_ptr<WindowManager> get_window_manager() { return m_window_manager; }

    /**
     * @brief Gets the event manager
     * @return Shared pointer to the EventManager for input/event handling
     *
     * The EventManager processes input events (keyboard, mouse, etc.).
     * Access through Engine ensures events are routed correctly to windows.
     */
    inline std::shared_ptr<Vruta::EventManager> get_event_manager() { return m_event_manager; }

    /**
     * @brief Gets the stochastic signal generator engine
     * @return Pointer to the Random node for random signal generation
     *
     * The Random node provides various stochastic signal sources.
     * Managed directly by Engine for optimal performance in generator nodes.
     */
    inline Nodes::Generator::Stochastics::Random* get_random_engine() { return m_rng.get(); }

    /**
     * @brief Gets the subsystem manager for advanced component access
     * @return Shared pointer to SubsystemManager for subsystem coordination
     *
     * The SubsystemManager provides access to specialized subsystems like
     * audio backends, graphics systems, and custom processing domains.
     */
    inline std::shared_ptr<SubsystemManager> get_subsystem_manager() { return m_subsystem_manager; }

    /**
     * @brief Get typed access to a specific subsystem
     * @tparam SubsystemType Expected type of the subsystem
     * @param tokens Token configuration identifying the subsystem
     * @return Shared pointer to subsystem or nullptr if not found
     */
    std::shared_ptr<ISubsystem> get_subsystem(SubsystemType type)
    {
        return m_subsystem_manager->get_subsystem(type);
    }

    /**
     * @brief Blocks until shutdown is requested (main thread event loop)
     *
     * Pumps platform-specific events on the main thread and waits for shutdown.
     * On macOS: Runs CFRunLoop to process dispatch queue (required for GLFW)
     * On other platforms: Simple blocking wait for user input
     *
     * Should be called on the main thread after Start().
     */
    void await_shutdown();

    /**
     * @brief Request shutdown from any thread
     *
     * Signals the event loop to exit. Thread-safe.
     * Call this to gracefully terminate await_shutdown().
     */
    void request_shutdown();

    /**
     * @brief Check if shutdown has been requested
     * @return true if shutdown was requested
     */
    bool is_shutdown_requested() const;

private:
    //-------------------------------------------------------------------------
    // System Components
    //-------------------------------------------------------------------------

    GlobalStreamInfo m_stream_info {}; ///< Stream configuration
    GlobalGraphicsConfig m_graphics_config {}; ///< Graphics/windowing configuration

    bool m_is_paused {}; ///< Pause state flag
    bool m_is_initialized {};

    std::atomic<bool> m_should_shutdown { false };

    //-------------------------------------------------------------------------
    // Core Components
    //-------------------------------------------------------------------------

    std::shared_ptr<Vruta::TaskScheduler> m_scheduler; ///< Task scheduler
    std::shared_ptr<Nodes::NodeGraphManager> m_node_graph_manager; ///< Node graph manager
    std::shared_ptr<Buffers::BufferManager> m_buffer_manager; ///< Buffer manager
    std::shared_ptr<SubsystemManager> m_subsystem_manager;
    std::shared_ptr<WindowManager> m_window_manager; ///< Window manager (Windowing subsystem)
    std::shared_ptr<Vruta::EventManager> m_event_manager; ///< Event manager (currently only glfw events)
    std::unique_ptr<Nodes::Generator::Stochastics::Random> m_rng; ///< Stochastic signal generator

#ifdef MAYAFLUX_PLATFORM_MACOS
    void run_macos_event_loop();
#endif

#ifdef MAYAFLUX_PLATFORM_WINDOWS
    void run_windows_event_loop();
#endif
};

} // namespace MayaFlux::Core
