#pragma once

#include "SubsystemManager.hpp"

namespace MayaFlux::Nodes::Generator::Stochastics {
class NoiseEngine;
}

#include "GlobalGraphicsInfo.hpp"
#include "GlobalStreamInfo.hpp"

namespace MayaFlux::Core {

class SubsystemManager;
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
class Engine {
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
     * @param sample_rate Audio sample rate in Hz
     * @param buffer_size Size of audio processing buffer in frames
     * @param num_out_channels Number of output channels
     * @param num_in_channels Number of input channels
     *
     * Orchestrates the initialization sequence for all core components:
     * - Creates and configures the task scheduler with the specified sample rate
     * - Initializes the node graph manager and buffer manager
     * - Sets up subsystem managers and audio backend
     * - Establishes proper component interconnections
     *
     * This method must be called before Start().
     */
    void Init(u_int32_t sample_rate = 48000U, u_int32_t buffer_size = 512U, u_int32_t num_out_channels = 2U, u_int32_t num_in_channels = 0U);

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
     * @param graphicsInfo Configuration for graphics/windowing backend
     *
     * Configures the processing engine with the specified stream and graphics information.
     * This method must be called before starting the engine.
     */
    void Init(const GlobalStreamInfo& streamInfo, const GraphicsSurfaceInfo& graphicsInfo);

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
     * @return Reference to the GraphicsSurfaceInfo struct
     */
    inline GraphicsSurfaceInfo& get_graphics_info() { return m_graphics_info; }

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
     * @brief Gets the stochastic signal generator engine
     * @return Pointer to the NoiseEngine for random signal generation
     *
     * The NoiseEngine provides various stochastic signal sources.
     * Managed directly by Engine for optimal performance in generator nodes.
     */
    inline Nodes::Generator::Stochastics::NoiseEngine* get_random_engine() { return m_rng.get(); }

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

private:
    //-------------------------------------------------------------------------
    // System Components
    //-------------------------------------------------------------------------

    GlobalStreamInfo m_stream_info; ///< Stream configuration
    GraphicsSurfaceInfo m_graphics_info; ///< Graphics/windowing configuration

    bool m_is_paused {}; ///< Pause state flag
    bool m_is_initialized {};

    //-------------------------------------------------------------------------
    // Core Components
    //-------------------------------------------------------------------------

    std::shared_ptr<Vruta::TaskScheduler> m_scheduler; ///< Task scheduler
    std::shared_ptr<Nodes::NodeGraphManager> m_node_graph_manager; ///< Node graph manager
    std::shared_ptr<Buffers::BufferManager> m_buffer_manager; ///< Buffer manager
    std::shared_ptr<SubsystemManager> m_subsystem_manager;
    std::shared_ptr<WindowManager> m_window_manager; ///< Window manager (Windowing subsystem)
    std::unique_ptr<Nodes::Generator::Stochastics::NoiseEngine> m_rng; ///< Stochastic signal generator
};

} // namespace MayaFlux::Core
