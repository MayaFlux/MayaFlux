#pragma once

#include "MayaFlux/Vruta/Scheduler.hpp"

namespace MayaFlux::Nodes {
class NodeGraphManager;
namespace Generator::Stochastics {
    class NoiseEngine;
}
}

namespace MayaFlux::Buffers {
class BufferManager;
}

namespace MayaFlux::Core {

class Device;
class Stream;

/**
 * @struct GlobalStreamInfo
 * @brief Configuration settings for signal processing stream
 */
struct GlobalStreamInfo {
    unsigned int sample_rate = 48000; ///< Processing sample rate in Hz
    unsigned int buffer_size = 512; ///< Size of processing buffer in frames
    unsigned int num_channels = 2; ///< Number of processing channels
};

/**
 * @enum HookPosition
 * @brief Defines the position in the processing cycle where a hook should be executed
 *
 * Process hooks can be registered to run either before or after the main audio processing
 * to perform additional operations or monitoring at specific points in the signal chain.
 */
enum class HookPosition {
    PRE_PROCESS, ///< Execute hook before any audio processing occurs
    POST_PROCESS ///< Execute hook after all audio processing is complete
};

/**
 * @typedef ProcessHook
 * @brief Function type for process hooks that can be registered with the engine
 *
 * Process hooks are callbacks that execute at specific points in the audio processing cycle.
 * They receive the current number of frames being processed and can be used for monitoring,
 * debugging, or additional processing operations.
 */
using ProcessHook = std::function<void(unsigned int num_frames)>;

/**
 * @class Engine
 * @brief Core processing engine that manages signal flow, scheduling, and node graph operations
 *
 * The Engine is the central component of Maya Flux, responsible for initializing the processing system,
 * managing data streams, scheduling tasks, and coordinating the node-based computational graph.
 *
 * Engine provides centrally managed instances of key components:
 * - NodeGraphManager: Manages the computational processing node graph
 * - BufferManager: Handles data buffer allocation and management
 * - TaskScheduler: Schedules and executes processing tasks with precise timing
 *
 * While these centrally managed components provide a convenient way to work with Maya Flux,
 * users are free to:
 * - Create their own instances of these components
 * - Make custom connections between nodes
 * - Call processing methods manually for offline processing
 * - Process data without sending to hardware output
 *
 * For core system components like Device and Stream, users can create their own instances,
 * but this is risky unless they're familiar with the underlying factory API. Custom implementations
 * lose the guarantees provided by the Engine class, but advanced users are free to create and
 * manage these components directly if needed.
 *
 * Future versions will provide more methods to override default behaviors beyond
 * just swapping the contents of the shared pointers.
 */
class Engine {
public:
    //-------------------------------------------------------------------------
    // Initialization and Lifecycle
    //-------------------------------------------------------------------------

    /**
     * @brief Constructs a new Engine instance
     *
     * Initializes the processing context and device but doesn't start data flow.
     * Call Init() to configure and Start() to begin processing.
     */
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
     * @brief Initializes the processing engine with specified stream settings
     * @param stream_info Configuration for sample rate, buffer size, and channels
     *
     * Sets up the stream manager, scheduler, buffer manager, and node graph manager.
     * Must be called before Start().
     */
    void Init(GlobalStreamInfo stream_info = GlobalStreamInfo { 48000, 512, 2 });

    /**
     * @brief Starts data processing
     *
     * Opens and starts the processing stream. Init() must be called first.
     */
    void Start();

    /**
     * @brief Pauses processing
     *
     * Stops the data stream but maintains the current state of all components.
     */
    void Pause();

    /**
     * @brief Resumes processing after a pause
     *
     * Restarts the data stream with the current state of all components.
     */
    void Resume();

    /**
     * @brief Stops and cleans up all processing
     *
     * Terminates all tasks, stops and closes the data stream, and clears buffers.
     */
    void End();

    /**
     * @brief Checks if the processing engine is currently running
     * @return true if the data stream is active, false otherwise
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
     * @brief Gets the stream manager
     * @return Pointer to the Stream object
     */
    inline const Stream* get_stream_manager() const { return m_Stream_manager.get(); }

    /**
     * @brief Gets the RtAudio context handle
     * @return Pointer to the RtAudio object
     */
    RtAudio* get_handle() { return m_Context.get(); }

    //-------------------------------------------------------------------------
    // Component Access
    //-------------------------------------------------------------------------

    /**
     * @brief Gets the node graph manager
     * @return Shared pointer to the NodeGraphManager
     */
    inline std::shared_ptr<Nodes::NodeGraphManager> get_node_graph_manager() { return m_node_graph_manager; }

    /**
     * @brief Gets the task scheduler
     * @return Shared pointer to the TaskScheduler
     */
    inline std::shared_ptr<Vruta::TaskScheduler> get_scheduler() { return m_scheduler; }

    /**
     * @brief Gets the buffer manager
     * @return Shared pointer to the BufferManager
     */
    inline std::shared_ptr<Buffers::BufferManager> get_buffer_manager() { return m_Buffer_manager; }

    /**
     * @brief Gets the stochastic signal generator engine
     * @return Pointer to the NoiseEngine
     */
    inline Nodes::Generator::Stochastics::NoiseEngine* get_random_engine() { return m_rng; }

    //-------------------------------------------------------------------------
    // Signal Processing
    //-------------------------------------------------------------------------

    /**
     * @brief Processes input data
     * @param input_buffer Pointer to input data buffer
     * @param num_frames Number of frames to process
     * @return Status code (0 for success)
     */
    int process_input(double* input_buffer, unsigned int num_frames);

    /**
     * @brief Processes output data
     * @param output_buffer Pointer to output data buffer
     * @param num_frames Number of frames to process
     * @return Status code (0 for success)
     *
     * Processes scheduled tasks and fills the output buffer with processed data.
     */
    int process_output(double* output_buffer, unsigned int num_frames);

    /**
     * @brief Processes both input and output data
     * @param input_buffer Pointer to input data buffer
     * @param output_buffer Pointer to output data buffer
     * @param num_frames Number of frames to process
     * @return Status code (0 for success)
     */
    int process_audio(double* input_buffer, double* output_buffer, unsigned int num_frames);

    //-------------------------------------------------------------------------
    // Task Scheduling
    //-------------------------------------------------------------------------

    /**
     * @brief Gets a pointer to a task's current value
     * @param name Name of the task
     * @return Pointer to the float value, or nullptr if not found
     */
    float* get_line_value(const std::string& name);

    /**
     * @brief Creates a function that returns a task's current value
     * @param name Name of the task
     * @return Function that returns the current float value
     */
    std::function<float()> line_value(const std::string& name);

    /**
     * @brief Schedules a new computational routine task
     * @param name Unique name for the task
     * @param task The computational routine to schedule
     * @param initialize Whether to initialize the task immediately
     */
    void schedule_task(std::string name, Vruta::SoundRoutine&& task, bool initialize = false);

    /**
     * @brief Cancels a scheduled task
     * @param name Name of the task to cancel
     * @return true if task was found and canceled, false otherwise
     */
    bool cancel_task(const std::string& name);

    /**
     * @brief Restarts a scheduled task
     * @param name Name of the task to restart
     * @return true if task was found and restarted, false otherwise
     */
    bool restart_task(const std::string& name);

    /**
     * @brief Updates parameters of a scheduled task
     * @tparam Args Parameter types
     * @param name Name of the task to update
     * @param args New parameter values
     * @return true if task was found and updated, false otherwise
     */
    template <typename... Args>
    inline bool update_task_params(const std::string& name, Args... args)
    {
        auto it = m_named_tasks.find(name);
        if (it != m_named_tasks.end() && it->second->is_active()) {
            it->second->update_params(std::forward<Args>(args)...);
            return true;
        }
        return false;
    }

    /**
     * @brief Registers a process hook to be executed at a specific point in the processing cycle
     * @param name Unique identifier for the hook
     * @param hook The callback function to execute
     * @param position When to execute the hook (pre or post processing)
     *
     * Process hooks allow for custom code execution at specific points in the audio processing cycle.
     * They can be used for monitoring, debugging, or additional processing operations.
     */
    void register_process_hook(const std::string& name, ProcessHook hook, HookPosition position = HookPosition::POST_PROCESS);

    /**
     * @brief Removes a previously registered process hook
     * @param name The identifier of the hook to remove
     */
    void unregister_process_hook(const std::string& name);

    /**
     * @brief Checks if a process hook with the given name exists
     * @param name The identifier of the hook to check
     * @return true if a hook with the given name exists, false otherwise
     */
    bool has_process_hook(const std::string& name) const;

private:
    //-------------------------------------------------------------------------
    // System Components
    //-------------------------------------------------------------------------

    std::unique_ptr<RtAudio> m_Context; ///< RtAudio context
    std::unique_ptr<Device> m_Device; ///< Device manager
    std::unique_ptr<Stream> m_Stream_manager; ///< Stream manager
    GlobalStreamInfo m_stream_info; ///< Stream configuration

    bool m_is_paused; ///< Pause state flag

    //-------------------------------------------------------------------------
    // Core Components
    //-------------------------------------------------------------------------

    std::shared_ptr<Vruta::TaskScheduler> m_scheduler; ///< Task scheduler
    std::shared_ptr<Nodes::NodeGraphManager> m_node_graph_manager; ///< Node graph manager
    std::shared_ptr<Buffers::BufferManager> m_Buffer_manager; ///< Buffer manager
    Nodes::Generator::Stochastics::NoiseEngine* m_rng; ///< Stochastic signal generator

    //-------------------------------------------------------------------------
    // Task Management
    //-------------------------------------------------------------------------

    std::unordered_map<std::string, std::shared_ptr<Vruta::SoundRoutine>> m_named_tasks; ///< Named task registry

    std::map<std::string, ProcessHook> m_pre_process_hooks;
    std::map<std::string, ProcessHook> m_post_process_hooks;
};

} // namespace MayaFlux::Core
