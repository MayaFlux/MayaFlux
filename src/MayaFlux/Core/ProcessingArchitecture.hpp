#pragma once

#include "MayaFlux/Core/ProcessingTokens.hpp"

namespace MayaFlux::Nodes {
class NodeGraphManager;
}

namespace MayaFlux::Buffers {
class BufferManager;
}

namespace MayaFlux::Core {

/**
 * @file ProcessingArchitecture.hpp
 * @brief Unified processing architecture for multimodal subsystem coordination
 *
 * Token-based system where each processing domain (audio, video, custom) can have
 * its own processing characteristics while maintaining unified interfaces.
 */

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

    /** @brief Equality comparison for efficient token matching */
    bool operator==(const SubsystemTokens& other) const
    {
        return Buffer == other.Buffer && Node == other.Node;
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
    void process(u_int32_t processing_units);

    /** @brief Process specific channel */
    void process_channel(u_int32_t channel, u_int32_t processing_units);

    /** @brief Process channel with node output data integration */
    void process_channel_with_node_data(
        u_int32_t channel,
        u_int32_t processing_units,
        const std::vector<double>& node_data);

    /** @brief Get read-only access to channel data */
    std::span<const double> read_channel_data(u_int32_t channel) const;

    /** @brief Get write access to channel data with automatic locking */
    std::span<double> write_channel_data(u_int32_t channel);

    /** @brief Configure channel layout for token domain */
    void setup_channels(u_int32_t num_channels, u_int32_t buffer_size);

    /** @brief Fill interleaved data buffer from token channels */
    void fill_interleaved(double* interleaved_data, u_int32_t num_frames, u_int32_t num_channels);

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
    void process(unsigned int num_samples);

    /** @brief Process nodes for specific channel and return output */
    std::vector<double> process_channel(unsigned int channel, unsigned int num_samples);

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
        SubsystemTokens tokens);

    /** @brief Buffer processing interface */
    BufferProcessingHandle buffers;

    /** @brief Node processing interface */
    NodeProcessingHandle nodes;

    /** @brief Get processing token configuration */
    inline SubsystemTokens get_tokens() const { return m_tokens; }

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
        return hash<int>()(static_cast<int>(tokens.Node)) ^ hash<int>()(static_cast<int>(tokens.Buffer));
    }
};

} // namespace std
