#pragma once

#include "MayaFlux/Core/ProcessingTokens.hpp"

namespace MayaFlux::Nodes {
class NodeGraphManager;
}

namespace MayaFlux::Buffers {
class BufferManager;
class Buffer;
class VKBuffer;
class VKProcessingContext;
}

namespace MayaFlux::Vruta {
class TaskScheduler;
class Routine;

using token_processing_func_t = std::function<void(const std::vector<std::shared_ptr<Routine>>&, uint64_t)>;
}

/**
 * @file ProcessingArchitecture.hpp
 * @brief Unified processing architecture for multimodal subsystem coordination
 *
 * Token-based system where each processing domain (audio, video, custom) can have
 * its own processing characteristics while maintaining unified interfaces.
 */
namespace MayaFlux::Core {

class Window;
class WindowManager;

/**
 * @enum HookPosition
 * @brief Defines the position in the processing cycle where a hook should be executed
 *
 * Process hooks can be registered to run either before or after the main audio processing
 * to perform additional operations or monitoring at specific points in the signal chain.
 */
enum class HookPosition : uint8_t {
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
 * @struct SubsystemTokens
 * @brief Processing token configuration for subsystem operation
 *
 * Defines processing characteristics by specifying how buffers and nodes
 * should be processed for each subsystem domain.
 */
struct SubsystemTokens {
    /** @brief Processing token for buffer operations */
    MayaFlux::Buffers::ProcessingToken Buffer;

    /** @brief Processing token for node graph operations */
    MayaFlux::Nodes::ProcessingToken Node;

    /** @brief Processing token for task scheduling operations */
    MayaFlux::Vruta::ProcessingToken Task;

    /** @brief Equality comparison for efficient token matching */
    bool operator==(const SubsystemTokens& other) const
    {
        return Buffer == other.Buffer && Node == other.Node && Task == other.Task;
    }
};

/**
 * @class BufferProcessingHandle
 * @brief Thread-safe interface for buffer operations within a processing domain
 *
 * Provides scoped, thread-safe access to buffer operations with automatic token validation.
 */
class BufferProcessingHandle {
public:
    /** @brief Constructs handle for specific buffer manager and token */
    BufferProcessingHandle(
        std::shared_ptr<Buffers::BufferManager> manager,
        Buffers::ProcessingToken token);

    // Non-copyable, moveable
    BufferProcessingHandle(const BufferProcessingHandle&) = delete;
    BufferProcessingHandle& operator=(const BufferProcessingHandle&) = delete;
    BufferProcessingHandle(BufferProcessingHandle&&) = default;
    BufferProcessingHandle& operator=(BufferProcessingHandle&&) = default;

    /** @brief Process all channels in token domain */
    void process(uint32_t processing_units);

    /** @brief Process specific channel */
    void process_channel(uint32_t channel, uint32_t processing_units);

    /** @brief Process channel with node output data integration */
    void process_channel_with_node_data(
        uint32_t channel,
        uint32_t processing_units,
        const std::vector<double>& node_data);

    /** @brienf Process Input from backend into buffer manager */
    void process_input(double* input_data, uint32_t num_channels, uint32_t num_frames);

    /** @brief Get read-only access to channel data */
    [[nodiscard]] std::span<const double> read_channel_data(uint32_t channel) const;

    /** @brief Get write access to channel data with automatic locking */
    std::span<double> write_channel_data(uint32_t channel);

    /** @brief Configure channel layout for token domain */
    void setup_channels(uint32_t num_channels, uint32_t buffer_size);

    /** @brief Unregister buffer initialization callback for token domain */
    void unregister_contexts();

    /** @brief Set Vulkan processing context for graphics buffers */
    void set_graphics_processing_context(const std::shared_ptr<Buffers::VKProcessingContext>& context);

private:
    void ensure_valid() const;
    void acquire_write_lock();

    std::shared_ptr<Buffers::BufferManager> m_manager;
    Buffers::ProcessingToken m_token;
    bool m_locked;
};

/**
 * @class NodeProcessingHandle
 * @brief Interface for node graph operations within a processing domain
 *
 * Provides scoped access to node operations with automatic token assignment.
 */
class NodeProcessingHandle {
public:
    /** @brief Constructs handle for specific node manager and token */
    NodeProcessingHandle(
        std::shared_ptr<Nodes::NodeGraphManager> manager,
        Nodes::ProcessingToken token);

    /** @brief Process all nodes in token domain */
    void process(uint32_t num_samples);

    /** @brief Process nodes for specific channel and return output */
    std::vector<double> process_channel(uint32_t channel, uint32_t num_samples);

    double process_sample(uint32_t channel);

    /** @brief Create node with automatic token assignment */
    template <typename NodeType, typename... Args>
    std::shared_ptr<NodeType> create_node(Args&&... args)
    {
        auto node = std::make_shared<NodeType>(std::forward<Args>(args)...);
        node->set_processing_token(m_token);
        return node;
    }

private:
    std::shared_ptr<Nodes::NodeGraphManager> m_manager;
    Nodes::ProcessingToken m_token;
};

class TaskSchedulerHandle {
public:
    TaskSchedulerHandle(
        std::shared_ptr<MayaFlux::Vruta::TaskScheduler> task_manager,
        MayaFlux::Vruta::ProcessingToken token);

    /** @brief Register custom processing function for token domain */
    void register_token_processor(Vruta::token_processing_func_t processor);

    /** @brief Process all tasks in token domain */
    void process(uint64_t processing_units);

    /** @brief Check if handle is valid */
    [[nodiscard]] bool is_valid() const { return m_scheduler != nullptr; }

    /** @brief Process all tasks scheduled for current buffer cycle */
    void process_buffer_cycle();

private:
    std::shared_ptr<Vruta::TaskScheduler> m_scheduler;
    Vruta::ProcessingToken m_token;
};

class WindowManagerHandle {
public:
    /** @brief Constructs handle for specific window manager */
    WindowManagerHandle(std::shared_ptr<Core::WindowManager> window_manager);

    /** @brief Process window events and frame hooks */
    void process();

    /** @brief Get list of windows that are open and not minimized */
    [[nodiscard]] std::vector<std::shared_ptr<Core::Window>> get_processing_windows() const;

private:
    std::shared_ptr<Core::WindowManager> m_window_manager;
};

/**
 * @class SubsystemProcessingHandle
 * @brief Unified interface combining buffer and node processing for subsystems
 *
 * Single interface that coordinates buffer and node operations for a subsystem.
 */
class SubsystemProcessingHandle {
public:
    /** @brief Constructs unified handle with buffer and node managers */
    SubsystemProcessingHandle(
        std::shared_ptr<Buffers::BufferManager> buffer_manager,
        std::shared_ptr<Nodes::NodeGraphManager> node_manager,
        std::shared_ptr<Vruta::TaskScheduler> task_scheduler,
        SubsystemTokens tokens);

    SubsystemProcessingHandle(
        std::shared_ptr<Buffers::BufferManager> buffer_manager,
        std::shared_ptr<Nodes::NodeGraphManager> node_manager,
        std::shared_ptr<Vruta::TaskScheduler> task_scheduler,
        std::shared_ptr<Core::WindowManager> window_manager,
        SubsystemTokens tokens);

    /** @brief Buffer processing interface */
    BufferProcessingHandle buffers;

    /** @brief Node processing interface */
    NodeProcessingHandle nodes;

    TaskSchedulerHandle tasks;

    WindowManagerHandle windows;

    /** @brief Get processing token configuration */
    [[nodiscard]] inline SubsystemTokens get_tokens() const { return m_tokens; }

    std::map<std::string, ProcessHook> pre_process_hooks;
    std::map<std::string, ProcessHook> post_process_hooks;

private:
    SubsystemTokens m_tokens;
};

} // namespace MayaFlux::Core

namespace std {

/** @brief Hash function specialization for SubsystemTokens */
template <>
struct hash<MayaFlux::Core::SubsystemTokens> {
    size_t operator()(const MayaFlux::Core::SubsystemTokens& tokens) const noexcept
    {
        return hash<int>()(static_cast<int>(tokens.Node)) ^ hash<int>()(static_cast<int>(tokens.Buffer)) ^ hash<int>()(static_cast<int>(tokens.Task));
    }
};

} // namespace std
