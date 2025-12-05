#pragma once

#include "MayaFlux/Buffers/BufferSpec.hpp"
#include "MayaFlux/Core/ProcessingTokens.hpp"

namespace MayaFlux::Nodes {
class Node;
}

namespace MayaFlux::Buffers {

class BufferProcessor;
class BufferProcessingChain;
class TokenUnitManager;
class BufferAccessControl;

/**
 * @class BufferProcessingControl
 * @brief Processor attachment, removal, and processing chain management
 *
 * Manages all operations related to adding/removing processors to buffers
 * and processing chains. Handles processor routing for different tokens,
 * processor lifecycle (quick processes), and node-to-buffer connections.
 *
 * Design Principles:
 * - Token-aware: Routes processors to appropriate token chains
 * - Domain-agnostic: Works with any token's processing chains
 * - Single responsibility: Only handles processor management, not access/input
 * - Delegates to TokenUnitManager and BufferAccessControl for underlying operations
 *
 * This class centralizes all processor-related logic that was scattered
 * throughout the original BufferManager.
 */
class MAYAFLUX_API BufferProcessingControl {
public:
    /**
     * @brief Creates a new processing control handler
     * @param unit_manager Reference to the TokenUnitManager
     * @param access_control Reference to the BufferAccessControl
     */
    BufferProcessingControl(TokenUnitManager& unit_manager, BufferAccessControl& access_control);

    ~BufferProcessingControl() = default;

    // =========================================================================
    // Processor Management (Token-Dispatching)
    // =========================================================================

    /**
     * @brief Adds a processor to a buffer (dispatches based on buffer/token)
     * @param processor Processor to add
     * @param buffer Target buffer
     * @param token Processing domain (optional, used for routing)
     */
    void add_processor(
        const std::shared_ptr<BufferProcessor>& processor,
        const std::shared_ptr<Buffer>& buffer,
        ProcessingToken token = ProcessingToken::AUDIO_BACKEND);

    /**
     * @brief Adds a processor to a token (dispatches based on token)
     * @param processor Processor to add
     * @param token Processing domain
     * @param channel Channel index (optional, used for audio tokens)
     */
    void add_processor(
        const std::shared_ptr<BufferProcessor>& processor,
        ProcessingToken token,
        uint32_t channel);

    /**
     * @brief Adds a processor to all channels in a token (dispatches based on token)
     * @param processor Processor to add
     * @param token Processing domain
     */
    void add_processor(
        const std::shared_ptr<BufferProcessor>& processor,
        ProcessingToken token);

    /**
     * @brief Removes a processor from a buffer (dispatches based on buffer/token)
     * @param processor Processor to remove
     * @param buffer Target buffer
     * @param token Processing domain (optional, used for routing)
     */
    void remove_processor(
        const std::shared_ptr<BufferProcessor>& processor,
        const std::shared_ptr<Buffer>& buffer,
        ProcessingToken token = ProcessingToken::AUDIO_BACKEND);

    /**
     * @brief Removes a processor from a token (dispatches based on token)
     * @param processor Processor to remove
     * @param token Processing domain
     * @param channel Channel index (optional, used for audio tokens)
     */
    void remove_processor_from_token(
        const std::shared_ptr<BufferProcessor>& processor,
        ProcessingToken token,
        uint32_t channel = 0);

    /**
     * @brief Sets a final processor for a token (dispatches based on token)
     * @param processor Final processor to apply
     * @param token Processing domain
     */
    void set_final_processor(
        const std::shared_ptr<BufferProcessor>& processor,
        ProcessingToken token);

    // =========================================================================
    // Processor Management - Audio
    // =========================================================================

    /**
     * @brief Adds a processor to a specific audio buffer
     * @param processor Processor to add
     * @param buffer Target audio buffer
     *
     * Routes the processor to the appropriate processing chain based on the buffer's channel ID.
     */
    void add_audio_processor(
        const std::shared_ptr<BufferProcessor>& processor,
        const std::shared_ptr<AudioBuffer>& buffer);

    /**
     * @brief Adds a processor to a specific audio token and channel
     * @param processor Processor to add
     * @param token Processing domain
     * @param channel Channel index
     */
    void add_audio_processor_to_channel(
        const std::shared_ptr<BufferProcessor>& processor,
        ProcessingToken token,
        uint32_t channel);

    /**
     * @brief Adds a processor to all channels in an audio token
     * @param processor Processor to add
     * @param token Processing domain
     */
    void add_audio_processor_to_token(
        const std::shared_ptr<BufferProcessor>& processor,
        ProcessingToken token);

    /**
     * @brief Removes a processor from a specific audio buffer
     * @param processor Processor to remove
     * @param buffer Target audio buffer
     */
    void remove_audio_processor(
        const std::shared_ptr<BufferProcessor>& processor,
        const std::shared_ptr<AudioBuffer>& buffer);

    /**
     * @brief Removes a processor from a specific audio token and channel
     * @param processor Processor to remove
     * @param token Processing domain
     * @param channel Channel index
     */
    void remove_audio_processor_from_channel(
        const std::shared_ptr<BufferProcessor>& processor,
        ProcessingToken token,
        uint32_t channel);

    /**
     * @brief Removes a processor from all channels in an audio token
     * @param processor Processor to remove
     * @param token Processing domain
     */
    void remove_audio_processor_from_token(
        const std::shared_ptr<BufferProcessor>& processor,
        ProcessingToken token);

    /**
     * @brief Sets a final processor for an audio token (applied to all channels)
     * @param processor Final processor to apply
     * @param token Processing domain
     *
     * Final processors are applied as the last step in the processing chain.
     */
    void set_audio_final_processor(
        const std::shared_ptr<BufferProcessor>& processor,
        ProcessingToken token);

    // =========================================================================
    // Quick Processing - Audio
    // =========================================================================

    /**
     * @brief Creates and attaches a quick processing function to buffer
     * @param processor Processing function
     * @param buffer Target buffer
     * @return Shared pointer to the created processor
     *
     * Quick processes are simple lambda-based processors for one-off transformations.
     */
    std::shared_ptr<BufferProcessor> attach_quick_process(
        AudioProcessingFunction processor,
        const std::shared_ptr<Buffer>& buffer, ProcessingToken token = ProcessingToken::AUDIO_BACKEND);

    std::shared_ptr<BufferProcessor> attach_quick_process(
        GraphicsProcessingFunction processor,
        const std::shared_ptr<Buffer>& buffer, ProcessingToken token = ProcessingToken::GRAPHICS_BACKEND);

    /**
     * @brief Creates and attaches a quick processing function to an audio token/channel
     * @param processor Processing function
     * @param token Processing domain
     * @param channel Channel index
     * @return Shared pointer to the created processor
     */
    std::shared_ptr<BufferProcessor> attach_quick_process(
        AudioProcessingFunction processor,
        ProcessingToken token,
        uint32_t channel);

    /**
     * @brief Creates and attaches a quick processing function to all channels in a token
     * @param processor Processing function
     * @param token Processing domain
     * @return Shared pointer to the created processor
     */
    std::shared_ptr<BufferProcessor> attach_quick_process(
        AudioProcessingFunction processor,
        ProcessingToken token);

    std::shared_ptr<BufferProcessor> attach_quick_process(
        GraphicsProcessingFunction processor,
        ProcessingToken token);

    // =========================================================================
    // Node Connection - Audio
    // =========================================================================

    /**
     * @brief Connects a node to a specific audio token and channel
     * @param node Node to connect
     * @param token Processing domain
     * @param channel Channel index
     * @param mix Mix level (default: 0.5)
     * @param clear_before Whether to clear buffer before adding node output (default: false)
     *
     * Creates a NodeSourceProcessor that feeds node output into the channel.
     */
    void connect_node_to_audio_channel(
        const std::shared_ptr<Nodes::Node>& node,
        ProcessingToken token,
        uint32_t channel,
        float mix = 0.5F,
        bool clear_before = false);

    /**
     * @brief Connects a node directly to a specific audio buffer
     * @param node Node to connect
     * @param buffer Target audio buffer
     * @param mix Mix level (default: 0.5)
     * @param clear_before Whether to clear buffer before adding node output (default: true)
     */
    void connect_node_to_audio_buffer(
        const std::shared_ptr<Nodes::Node>& node,
        const std::shared_ptr<AudioBuffer>& buffer,
        float mix = 0.5F,
        bool clear_before = true);

    // =========================================================================
    // Processor Management - Graphics
    // =========================================================================

    /**
     * @brief Adds a processor to the graphics processing chain
     * @param processor Processor to add
     * @param token Processing domain
     */
    void add_graphics_processor(
        const std::shared_ptr<BufferProcessor>& processor,
        ProcessingToken token);

    /**
     * @brief Adds a processor to a specific graphics buffer
     * @param processor Processor to add
     * @param buffer Target graphics buffer
     * @param token Processing domain
     */
    void add_graphics_processor(
        const std::shared_ptr<BufferProcessor>& processor,
        const std::shared_ptr<class Buffer>& buffer,
        ProcessingToken token);

    /**
     * @brief Sets a final processor for the graphics processing chain
     * @param processor Final processor to apply
     * @param token Processing domain
     */
    void set_graphics_final_processor(
        const std::shared_ptr<BufferProcessor>& processor,
        ProcessingToken token);

    /**
     * @brief Removes a processor from the graphics processing chain
     * @param processor Processor to remove
     * @param token Processing domain
     */
    void remove_graphics_processor(
        const std::shared_ptr<BufferProcessor>& processor,
        ProcessingToken token);

private:
    /// Reference to the token/unit manager
    TokenUnitManager& m_unit_manager;

    /// Reference to the buffer access control
    BufferAccessControl& m_access_control;
};

} // namespace MayaFlux::Buffers
