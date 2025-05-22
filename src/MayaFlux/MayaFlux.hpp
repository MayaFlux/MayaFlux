#pragma once

#include "config.h"

/**
 * @namespace MayaFlux
 * @brief Main namespace for the Maya Flux audio engine
 *
 * This namespace provides convenience wrappers around the core functionality of the Maya Flux
 * audio engine. These wrappers simplify access to the centrally managed components and common
 * operations, making it easier to work with the engine without directly managing the Engine
 * instance.
 *
 * All functions in this namespace operate on the default Engine instance and its managed
 * components. For custom or non-default components, use their specific handles and methods
 * directly rather than these wrappers.
 */
namespace MayaFlux {

namespace Core {
    struct GlobalStreamInfo;
    class Engine;

}

namespace Vruta {
    class TaskScheduler;
    class SoundRoutine;
}

namespace Buffers {
    class AudioBuffer;
    class BufferManager;
    class BufferProcessor;
}

namespace Nodes {
    class Node;
    class NodeGraphManager;
    class RootNode;
}

namespace Kriya {
    class ActionToken;
}

/**
 * @typedef AudioProcessingFunction
 * @brief Function type for audio buffer processing callbacks
 */
using AudioProcessingFunction = std::function<void(std::shared_ptr<Buffers::AudioBuffer>)>;

//-------------------------------------------------------------------------
// Engine Management
//-------------------------------------------------------------------------

/**
 * @brief Checks if the default engine has been initialized
 * @return true if the engine is initialized, false otherwise
 */
bool is_engine_initialized();

/**
 * @brief Gets the default engine instance
 * @return Reference to the default Engine
 *
 * Creates the engine if it doesn't exist yet. This is the centrally managed
 * engine instance that all convenience functions in this namespace operate on.
 */
Core::Engine& get_context();

/**
 * @brief Replaces the default engine with a new instance
 * @param instance New engine instance to use as default
 *
 * Transfers state from the old engine to the new one if possible.
 *
 * @warning This function uses move semantics. After calling this function,
 * the engine instance passed as parameter will be left in a moved-from state
 * and should not be used further. This is intentional to avoid resource duplication
 * and ensure proper transfer of ownership. Users should be careful to not access
 * the moved-from instance after calling this function.
 *
 * This function is intended for advanced use cases where custom engine configuration
 * is required beyond what the standard initialization functions provide.
 */
void set_and_transfer_context(Core::Engine instance);

/**
 * @brief Initializes the default engine with specified parameters
 * @param sample_rate Audio sample rate in Hz
 * @param buffer_size Size of audio processing buffer in frames
 * @param num_out_channels Number of output channels
 * @param num_in_channels Number of input channels
 *
 * Convenience wrapper for Engine::Init() on the default engine.
 */
void Init(u_int32_t sample_rate = 48000, u_int32_t buffer_size = 512, u_int32_t num_out_channels = 2, u_int32_t num_in_channels = 0);

/**
 * @brief Initializes the default engine with specified stream info
 * @param stream_info Configuration for sample rate, buffer size, and channels
 *
 * Convenience wrapper for Engine::Init() on the default engine.
 */
void Init(Core::GlobalStreamInfo stream_info);

/**
 * @brief Starts audio processing on the default engine
 *
 * Convenience wrapper for Engine::Start() on the default engine.
 */
void Start();

/**
 * @brief Pauses audio processing on the default engine
 *
 * Convenience wrapper for Engine::Pause() on the default engine.
 */
void Pause();

/**
 * @brief Resumes audio processing on the default engine
 *
 * Convenience wrapper for Engine::Resume() on the default engine.
 */
void Resume();

/**
 * @brief Stops and cleans up the default engine
 *
 * Convenience wrapper for Engine::End() on the default engine.
 */
void End();

//-------------------------------------------------------------------------
// Configuration Access
//-------------------------------------------------------------------------

/**
 * @brief Gets the stream configuration from the default engine
 * @return Copy of the GlobalStreamInfo struct
 */
Core::GlobalStreamInfo get_global_stream_info();

/**
 * @brief Gets the sample rate from the default engine
 * @return Current sample rate in Hz
 */
u_int32_t get_sample_rate();

/**
 * @brief Gets the buffer size from the default engine
 * @return Current buffer size in frames
 */
u_int32_t get_buffer_size();

/**
 * @brief Gets the number of output channels from the default engine
 * @return Current number of output channels
 */
u_int32_t get_num_out_channels();

//-------------------------------------------------------------------------
// Component Access
//-------------------------------------------------------------------------

/**
 * @brief Gets the task scheduler from the default engine
 * @return Shared pointer to the centrally managed TaskScheduler
 *
 * Returns the scheduler that's managed by the default engine instance.
 * All scheduled tasks using the convenience functions will use this scheduler.
 */
std::shared_ptr<Vruta::TaskScheduler> get_scheduler();

/**
 * @brief Gets the buffer manager from the default engine
 * @return Shared pointer to the centrally managed BufferManager
 *
 * Returns the buffer manager that's managed by the default engine instance.
 * All buffer operations using the convenience functions will use this manager.
 */
std::shared_ptr<Buffers::BufferManager> get_buffer_manager();

/**
 * @brief Gets the node graph manager from the default engine
 * @return Shared pointer to the centrally managed NodeGraphManager
 *
 * Returns the node graph manager that's managed by the default engine instance.
 * All node operations using the convenience functions will use this manager.
 */
std::shared_ptr<Nodes::NodeGraphManager> get_node_graph_manager();

//-------------------------------------------------------------------------
// Random Number Generation
//-------------------------------------------------------------------------

/**
 * @brief Generates a uniform random number
 * @param start Lower bound (inclusive)
 * @param end Upper bound (exclusive)
 * @return Random number with uniform distribution
 *
 * Uses the random number generator from the default engine.
 */
double get_uniform_random(double start = 0, double end = 1);

/**
 * @brief Generates a gaussian (normal) random number
 * @param start Lower bound for scaling
 * @param end Upper bound for scaling
 * @return Random number with gaussian distribution
 *
 * Uses the random number generator from the default engine.
 */
double get_gaussian_random(double start = 0, double end = 1);

/**
 * @brief Generates an exponential random number
 * @param start Lower bound for scaling
 * @param end Upper bound for scaling
 * @return Random number with exponential distribution
 *
 * Uses the random number generator from the default engine.
 */
double get_exponential_random(double start = 0, double end = 1);

/**
 * @brief Generates a poisson random number
 * @param start Lower bound for scaling
 * @param end Upper bound for scaling
 * @return Random number with poisson distribution
 *
 * Uses the random number generator from the default engine.
 */
double get_poisson_random(double start = 0, double end = 1);

//-------------------------------------------------------------------------
// Task Scheduling
//-------------------------------------------------------------------------

/**
 * @brief Creates a metronome task that calls a function at regular intervals
 * @param interval_seconds Time between calls in seconds
 * @param callback Function to call on each tick
 * @return SoundRoutine object representing the scheduled task
 *
 * Uses the task scheduler from the default engine.
 */
Vruta::SoundRoutine schedule_metro(double interval_seconds, std::function<void()> callback);

/**
 * @brief Creates a sequence task that calls functions at specified times
 * @param sequence Vector of (time, function) pairs
 * @return SoundRoutine object representing the scheduled task
 *
 * Uses the task scheduler from the default engine.
 */
Vruta::SoundRoutine schedule_sequence(std::vector<std::pair<double, std::function<void()>>> sequence);

/**
 * @brief Creates a line generator that interpolates between values over time
 * @param start_value Starting value
 * @param end_value Ending value
 * @param duration_seconds Total duration in seconds
 * @param step_duration Time between steps in seconds
 * @param loop Whether to loop back to start after reaching end
 * @return SoundRoutine object representing the line generator
 *
 * Uses the task scheduler from the default engine.
 */
Vruta::SoundRoutine create_line(float start_value, float end_value, float duration_seconds, float step_duration, bool loop);

/**
 * @brief Schedules a pattern generator that produces values based on a pattern function
 * @param pattern_func Function that generates pattern values based on step index
 * @param callback Function to call with each pattern value
 * @param interval_seconds Time between pattern steps
 * @return SoundRoutine object representing the scheduled task
 *
 * Uses the task scheduler from the default engine.
 */
Vruta::SoundRoutine schedule_pattern(std::function<std::any(u_int64_t)> pattern_func, std::function<void(std::any)> callback, double interval_seconds);

/**
 * @brief Gets a pointer to a task's current value
 * @param name Name of the task
 * @return Pointer to the float value, or nullptr if not found
 *
 * Convenience wrapper for Engine::get_line_value() on the default engine.
 */
float* get_line_value(const std::string& name);

/**
 * @brief Creates a function that returns a task's current value
 * @param name Name of the task
 * @return Function that returns the current float value
 *
 * Convenience wrapper for Engine::line_value() on the default engine.
 */
std::function<float()> line_value(const std::string& name);

/**
 * @brief Schedules a new sound routine task
 * @param name Unique name for the task
 * @param task The sound routine to schedule
 * @param initialize Whether to initialize the task immediately
 *
 * Convenience wrapper for Engine::schedule_task() on the default engine.
 */
void schedule_task(std::string name, Vruta::SoundRoutine&& task, bool initialize = false);

/**
 * @brief Cancels a scheduled task
 * @param name Name of the task to cancel
 * @return true if task was found and canceled, false otherwise
 *
 * Convenience wrapper for Engine::cancel_task() on the default engine.
 */
bool cancel_task(const std::string& name);

/**
 * @brief Restarts a scheduled task
 * @param name Name of the task to restart
 * @return true if task was found and restarted, false otherwise
 *
 * Convenience wrapper for Engine::restart_task() on the default engine.
 */
bool restart_task(const std::string& name);

/**
 * @brief Updates parameters of a scheduled task
 * @tparam Args Parameter types
 * @param name Name of the task to update
 * @param args New parameter values
 * @return true if task was found and updated, false otherwise
 *
 * Convenience wrapper for Engine::update_task_params() on the default engine.
 */
template <typename... Args>
bool update_task_params(const std::string& name, Args... args);

/**
 * @brief Creates an action to play a node
 * @param node Node to play
 * @return ActionToken representing the action
 *
 * Adds the node to the default engine's processing chain.
 */
Kriya::ActionToken Play(std::shared_ptr<Nodes::Node> node);

/**
 * @brief Creates a wait action
 * @param seconds Time to wait in seconds
 * @return ActionToken representing the wait
 *
 * Uses the task scheduler from the default engine.
 */
Kriya::ActionToken Wait(double seconds);

/**
 * @brief Creates a custom action
 * @param func Function to execute
 * @return ActionToken representing the action
 *
 * Uses the task scheduler from the default engine.
 */
Kriya::ActionToken Action(std::function<void()> func);

//-------------------------------------------------------------------------
// Audio Processing
//-------------------------------------------------------------------------

/**
 * @brief Attaches a processing function to a specific buffer
 * @param processor Function to process the buffer
 * @param buffer Buffer to process
 *
 * The processor will be called during the default engine's audio processing cycle.
 */
std::shared_ptr<Buffers::BufferProcessor> attach_quick_process(AudioProcessingFunction processor, std::shared_ptr<Buffers::AudioBuffer> buffer);

/**
 * @brief Attaches a processing function to a specific channel
 * @param processor Function to process the buffer
 * @param channel_id Channel index to process
 *
 * The processor will be called during the default engine's audio processing cycle
 * and will operate on the specified output channel buffer.
 */
std::shared_ptr<Buffers::BufferProcessor> attach_quick_process_to_channel(AudioProcessingFunction processor, unsigned int channel_id = 0);

/**
 * @brief Attaches a processing function to multiple channels
 * @param processor Function to process the buffer
 * @param channels Vector of channel indices to process
 *
 * The processor will be called during the default engine's audio processing cycle
 * for each of the specified channel buffers.
 */
std::shared_ptr<Buffers::BufferProcessor> attach_quick_process_to_channels(AudioProcessingFunction processor, const std::vector<unsigned int> channels);

//-------------------------------------------------------------------------
// Buffer Management
//-------------------------------------------------------------------------

/**
 * @brief Gets the audio buffer for a specific channel
 * @param channel Channel index
 * @return Reference to the channel's AudioBuffer
 *
 * Returns the buffer from the default engine's buffer manager.
 */
Buffers::AudioBuffer& get_channel(unsigned int channel);

/**
 * @brief Connects a node to a specific output channel
 * @param node Node to connect
 * @param channel_index Channel index to connect to
 * @param mix Mix level (0.0 to 1.0) for blending node output with existing channel content
 * @param clear_before Whether to clear the channel buffer before adding node output
 *
 * Uses the default engine's buffer manager and node graph manager.
 */
void connect_node_to_channel(std::shared_ptr<Nodes::Node> node, u_int32_t channel_index = 0, float mix = 0.5f, bool clear_before = false);

/**
 * @brief Connects a node to a specific buffer
 * @param node Node to connect
 * @param buffer Buffer to connect to
 * @param mix Mix level (0.0 to 1.0) for blending node output with existing buffer content
 * @param clear_before Whether to clear the buffer before adding node output
 *
 * Uses the default engine's node graph manager.
 */
void connect_node_to_buffer(std::shared_ptr<Nodes::Node> node, std::shared_ptr<Buffers::AudioBuffer> buffer, float mix = 0.5f, bool clear_before = true);

//-------------------------------------------------------------------------
// Node Graph Management
//-------------------------------------------------------------------------

/**
 * @brief Adds a node to the root node of a specific channel
 * @param node Node to add
 * @param channel Channel index
 *
 * Adds the node as a child of the root node for the specified channel.
 * Uses the default engine's node graph manager.
 */
void add_node_to_root(std::shared_ptr<Nodes::Node> node, unsigned int channel = 0);

/**
 * @brief Removes a node from the root node of a specific channel
 * @param node Node to remove
 * @param channel Channel index
 *
 * Removes the node from being a child of the root node for the specified channel.
 * Uses the default engine's node graph manager.
 */
void remove_node_from_root(std::shared_ptr<Nodes::Node> node, unsigned int channel = 0);

/**
 * @brief Connects two nodes by their string identifiers
 * @param source Source node identifier
 * @param target Target node identifier
 *
 * Establishes a connection where the output of the source node
 * becomes an input to the target node.
 * Uses the default engine's node graph manager.
 */
void connect_nodes(std::string& source, std::string& target);

/**
 * @brief Connects two nodes directly
 * @param source Source node
 * @param target Target node
 *
 * Establishes a connection where the output of the source node
 * becomes an input to the target node.
 * Uses the default engine's node graph manager.
 */
void connect_nodes(std::shared_ptr<Nodes::Node> source, std::shared_ptr<Nodes::Node> target);

/**
 * @brief Gets the root node for a specific channel
 * @param channel Channel index
 * @return The root node for the specified channel
 *
 * The root node is the top-level node in the processing hierarchy for a channel.
 * Uses the default engine's node graph manager.
 */
Nodes::RootNode get_root_node(u_int32_t channel = 0);

}
