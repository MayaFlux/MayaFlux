#pragma once

#include "BufferSpec.hpp"
#include "MayaFlux/Buffers/Managers/TokenUnitManager.hpp"

namespace MayaFlux::Nodes {
class Node;
}

namespace MayaFlux::Buffers {

class BufferProcessor;
class BufferProcessingChain;
class TokenUnitManager;
class BufferAccessControl;
class BufferProcessingControl;
class BufferInputControl;
class BufferSupplyMixing;

/**
 * @class BufferManager
 * @brief Token-based multimodal buffer management system for unified data stream processing
 *
 * BufferManager serves as the central orchestrator for buffer processing in the MayaFlux engine,
 * implementing a token-based architecture that enables seamless integration of different processing
 * domains while maintaining proven audio processing patterns.
 *
 * **Architecture:**
 * - **Token-Based Routing**: Operations use tokens to route to appropriate units
 * - **Unified Interface**: Most operations work generically across domains, no audio_/graphics_ prefixes needed
 * - **Delegating Facade**: Thin delegation layer over functional helper classes
 * - **Functional Helpers**: Encapsulate specific concerns (TokenUnitManager, BufferAccessControl, etc.)
 *
 * This design scales to new domains without API explosion—just add token support.
 */
class MAYAFLUX_API BufferManager {
public:
    /**
     * @brief Creates a new multimodal buffer manager
     * @param default_out_channels Number of output channels for the default domain (default: 2)
     * @param default_in_channels Number of input channels for the default domain (default: 0)
     * @param default_buffer_size Buffer size for the default domain (default: 512)
     * @param default_processing_token Primary processing domain (default: AUDIO_BACKEND)
     */
    BufferManager(
        uint32_t default_out_channels = 2,
        uint32_t default_in_channels = 0,
        uint32_t default_buffer_size = 512,
        ProcessingToken default_audio_token = ProcessingToken::AUDIO_BACKEND,
        ProcessingToken default_graphics_token = ProcessingToken::GRAPHICS_BACKEND);

    ~BufferManager();

    // =========================================================================
    // Processing and Token Management
    // =========================================================================

    /**
     * @brief Processes all buffers for a specific token
     * @param token Processing domain to process
     * @param processing_units Number of processing units (samples for audio, frames for video)
     */
    void process_token(ProcessingToken token, uint32_t processing_units);

    /**
     * @brief Processes all active tokens with their configured processing units
     */
    void process_all_tokens();

    /**
     * @brief Processes a specific channel within a token domain
     * @param token Processing domain
     * @param channel Channel index (audio-specific)
     * @param processing_units Number of processing units to process
     * @param node_output_data Optional output data from a node
     */
    void process_channel(
        ProcessingToken token,
        uint32_t channel,
        uint32_t processing_units,
        const std::vector<double>& node_output_data = {});

    /**
     * @brief Gets all currently active processing tokens
     * @return Vector of tokens that have active buffers
     */
    [[nodiscard]] std::vector<ProcessingToken> get_active_tokens() const;

    /**
     * @brief Registers a custom processor for an audio token domain
     * @param token Processing domain to handle
     * @param processor Function that processes all channels for this domain
     */
    void register_audio_token_processor(ProcessingToken token, RootAudioProcessingFunction processor);

    /**
     * @brief Gets the default processing token used by the manager
     */
    [[nodiscard]] ProcessingToken get_default_audio_token() const;

    // =========================================================================
    // Buffer Access (Token-Generic)
    // =========================================================================

    /**
     * @brief Gets a root buffer for a specific token and channel (audio-specific due to channels)
     * @param token Processing domain
     * @param channel Channel index
     * @return Shared pointer to the root audio buffer
     */
    std::shared_ptr<RootAudioBuffer> get_root_audio_buffer(
        ProcessingToken token,
        uint32_t channel = 0);

    /**
     * @brief Gets a root graphics buffer for a specific token
     * @param token Processing domain
     * @return Shared pointer to the root graphics buffer
     */
    std::shared_ptr<RootGraphicsBuffer> get_root_graphics_buffer(
        ProcessingToken token);

    /**
     * @brief Gets data from a specific token and channel (audio-specific)
     * @param token Processing domain
     * @param channel Channel index
     * @return Reference to the channel's data vector
     */
    std::vector<double>& get_buffer_data(ProcessingToken token, uint32_t channel);
    [[nodiscard]] const std::vector<double>& get_buffer_data(ProcessingToken token, uint32_t channel) const;

    /**
     * @brief Gets the number of channels for a token (audio-specific)
     * @param token Processing domain
     * @return Number of channels
     */
    [[nodiscard]] uint32_t get_num_channels(ProcessingToken token) const;

    /**
     * @brief Gets the buffer size for a token
     * @param token Processing domain
     * @return Buffer size in processing units
     */
    [[nodiscard]] uint32_t get_buffer_size(ProcessingToken token) const;

    /**
     * @brief Resizes buffers for a token
     * @param token Processing domain
     * @param buffer_size New buffer size
     */
    void resize_buffers(ProcessingToken token, uint32_t buffer_size);

    /**
     * @brief Ensures minimum number of channels exist for an audio token
     * @param token Processing domain
     * @param channel_count Minimum number of channels
     */
    void ensure_channels(ProcessingToken token, uint32_t channel_count);

    /**
     * @brief Validates the number of channels and resizes buffers if necessary (audio-specific)
     * @param token Processing domain
     * @param num_channels Number of channels to validate
     * @param buffer_size New buffer size to set
     *
     * This method ensures that the specified number of channels exists for the given token,
     * resizing the root audio buffers accordingly.
     */
    void validate_num_channels(ProcessingToken token, uint32_t num_channels, uint32_t buffer_size)
    {
        ensure_channels(token, num_channels);
        resize_buffers(token, buffer_size);
    }

    /**
     * @brief Gets the processing chain for a token and channel (audio-specific)
     * @param token Processing domain
     * @param channel Channel index
     * @return Shared pointer to the processing chain
     */
    std::shared_ptr<BufferProcessingChain> get_processing_chain(ProcessingToken token, uint32_t channel);

    /**
     * @brief Gets the global processing chain (applied to all tokens)
     * @return Shared pointer to the global processing chain
     */
    std::shared_ptr<BufferProcessingChain> get_global_processing_chain();

    // =========================================================================
    // Buffer Management (Token-Generic via Dynamic Dispatch)
    // =========================================================================

    /**
     * @brief Adds a buffer to a token and channel
     * @param buffer Buffer to add (AudioBuffer or VKBuffer depending on token)
     * @param token Processing domain
     * @param channel Channel index (optional, used for audio)
     */
    void add_buffer(
        const std::shared_ptr<Buffer>& buffer,
        ProcessingToken token,
        uint32_t channel = 0);

    /**
     * @brief Removes a buffer from a token
     * @param buffer Buffer to remove
     * @param token Processing domain
     * @param channel Channel index (optional, used for audio)
     */
    void remove_buffer(
        const std::shared_ptr<Buffer>& buffer,
        ProcessingToken token,
        uint32_t channel = 0);

    /**
     * @brief Gets buffers for a token (audio-specific due to channels)
     * @param token Processing domain
     * @param channel Channel index
     * @return Const reference to vector of buffers
     */
    [[nodiscard]] const std::vector<std::shared_ptr<AudioBuffer>>& get_buffers(ProcessingToken token, uint32_t channel) const;

    /**
     * @brief Gets graphics buffers for a token
     * @param token Processing domain
     * @return Const reference to vector of VKBuffers
     */
    [[nodiscard]] const std::vector<std::shared_ptr<VKBuffer>>& get_graphics_buffers(ProcessingToken token) const;

    /**
     * @brief Gets graphics buffers filtered by usage
     * @param usage VKBuffer usage type
     * @param token Processing domain
     * @return Vector of matching buffers
     */
    [[nodiscard]] std::vector<std::shared_ptr<VKBuffer>> get_buffers_by_usage(
        VKBuffer::Usage usage,
        ProcessingToken token) const;

    /**
     * @brief Creates a specialized audio buffer and adds it to the specified token/channel
     * @tparam BufferType Type of buffer to create (must be compatible with token)
     * @tparam Args Constructor argument types
     * @param token Processing domain
     * @param channel Channel index
     * @param args Constructor arguments
     * @return Shared pointer to the created buffer
     */
    template <typename BufferType, typename... Args>
    std::shared_ptr<BufferType> create_audio_buffer(ProcessingToken token, uint32_t channel, Args&&... args)
    {
        auto& unit = m_unit_manager->ensure_and_get_audio_unit(token, channel);
        auto buffer = std::make_shared<BufferType>(channel, unit.buffer_size, std::forward<Args>(args)...);

        if (auto audio_buffer = std::dynamic_pointer_cast<AudioBuffer>(buffer)) {
            add_buffer(audio_buffer, token, channel);
        }

        return buffer;
    }

    /**
     * @brief Creates a specialized vulkan buffer and adds it to the specified token/channel
     * @tparam BufferType Type of buffer to create (must be compatible with token)
     * @tparam Args Constructor argument types
     * @param token Processing domain
     * @param args Constructor arguments
     * @return Shared pointer to the created buffer
     */
    template <typename BufferType, typename... Args>
    std::shared_ptr<BufferType> create_graphics_buffer(ProcessingToken token, Args&&... args)
    {
        auto& unit = m_unit_manager->get_or_create_graphics_unit(token);
        auto buffer = std::make_shared<BufferType>(std::forward<Args>(args)...);

        if (auto vk_buffer = std::dynamic_pointer_cast<VKBuffer>(buffer)) {
            add_buffer(vk_buffer, token);
        }

        return buffer;
    }

    // =========================================================================
    // Processor Management (Token-Generic)
    // =========================================================================

    /**
     * @brief Adds a processor to a buffer
     * @param processor Processor to add
     * @param buffer Target buffer (dispatches based on type)
     */
    void add_processor(
        const std::shared_ptr<BufferProcessor>& processor,
        const std::shared_ptr<Buffer>& buffer, ProcessingToken token = ProcessingToken::AUDIO_BACKEND);

    /**
     * @brief Adds a processor to a token and channel (audio-specific)
     * @param processor Processor to add
     * @param token Processing domain
     * @param channel Channel index
     */
    void add_processor_to_channel(
        const std::shared_ptr<BufferProcessor>& processor,
        ProcessingToken token,
        uint32_t channel);

    /**
     * @brief Adds a processor to all channels in a token (audio-specific)
     * @param processor Processor to add
     * @param token Processing domain
     */
    void add_processor_to_token(
        const std::shared_ptr<BufferProcessor>& processor,
        ProcessingToken token);

    /**
     * @brief Removes a processor from a buffer
     * @param processor Processor to remove
     * @param buffer Target buffer
     */
    void remove_processor(
        const std::shared_ptr<BufferProcessor>& processor,
        const std::shared_ptr<Buffer>& buffer);

    /**
     * @brief Removes a processor from a token and channel (audio-specific)
     * @param processor Processor to remove
     * @param token Processing domain
     * @param channel Channel index
     */
    void remove_processor_from_channel(
        const std::shared_ptr<BufferProcessor>& processor,
        ProcessingToken token,
        uint32_t channel);

    /**
     * @brief Removes a processor from all channels in a token (audio-specific)
     * @param processor Processor to remove
     * @param token Processing domain
     */
    void remove_processor_from_token(
        const std::shared_ptr<BufferProcessor>& processor,
        ProcessingToken token);

    /**
     * @brief Sets a final processor for a token (audio-specific)
     * @param processor Final processor to apply
     * @param token Processing domain
     */
    void set_final_processor(
        const std::shared_ptr<BufferProcessor>& processor,
        ProcessingToken token);

    // =========================================================================
    // Quick Processing (Audio-Specific)
    // =========================================================================

    std::shared_ptr<BufferProcessor> attach_quick_process(
        BufferProcessingFunction processor,
        const std::shared_ptr<Buffer>& buffer, ProcessingToken token = ProcessingToken::AUDIO_BACKEND);

    std::shared_ptr<BufferProcessor> attach_quick_process(
        BufferProcessingFunction processor,
        ProcessingToken token,
        uint32_t channel);

    std::shared_ptr<BufferProcessor> attach_quick_process(
        BufferProcessingFunction processor,
        ProcessingToken token);

    // =========================================================================
    // Node Connection (Audio-Specific)
    // =========================================================================

    void connect_node_to_channel(
        const std::shared_ptr<Nodes::Node>& node,
        ProcessingToken token,
        uint32_t channel,
        float mix = 0.5F,
        bool clear_before = false);

    void connect_node_to_buffer(
        const std::shared_ptr<Nodes::Node>& node,
        const std::shared_ptr<AudioBuffer>& buffer,
        float mix = 0.5F,
        bool clear_before = true);

    // =========================================================================
    // Data I/O (Audio-Specific)
    // =========================================================================

    void fill_from_interleaved(
        const double* interleaved_data,
        uint32_t num_frames,
        ProcessingToken token,
        uint32_t num_channels);

    void fill_interleaved(
        double* interleaved_data,
        uint32_t num_frames,
        ProcessingToken token,
        uint32_t num_channels) const;

    void clone_buffer_for_channels(
        const std::shared_ptr<AudioBuffer>& buffer,
        const std::vector<uint32_t>& channels,
        ProcessingToken token);

    // =========================================================================
    // Input Handling (Audio-Specific)
    // =========================================================================

    void process_input(double* input_data, uint32_t num_channels, uint32_t num_frames);

    void register_input_listener(const std::shared_ptr<AudioBuffer>& buffer, uint32_t channel);

    void unregister_input_listener(const std::shared_ptr<AudioBuffer>& buffer, uint32_t channel);

    // =========================================================================
    // Buffer Supply/Mixing (Audio-Specific)
    // =========================================================================

    bool supply_buffer_to(
        const std::shared_ptr<AudioBuffer>& buffer,
        ProcessingToken token,
        uint32_t channel,
        double mix = 1.0);

    bool remove_supplied_buffer(
        const std::shared_ptr<AudioBuffer>& buffer,
        ProcessingToken token,
        uint32_t channel);

    // =========================================================================
    // Utility
    // =========================================================================

    void initialize_buffer_service();

    /**
     * @brief Terminates all active buffers, clearing their data
     */
    void terminate_active_buffers();

private:
    void process_audio_token_default(ProcessingToken token, uint32_t processing_units);

    void process_graphics_token_default(ProcessingToken token, uint32_t processing_units);

    /// Token/unit storage and lifecycle
    std::unique_ptr<TokenUnitManager> m_unit_manager;

    /// Buffer and unit access operations
    std::unique_ptr<BufferAccessControl> m_access_control;

    /// Processor attachment/removal operations
    std::unique_ptr<BufferProcessingControl> m_processor_control;

    /// Audio input management
    std::unique_ptr<BufferInputControl> m_input_control;

    /// Buffer supply and mixing operations
    std::unique_ptr<BufferSupplyMixing> m_supply_mixing;

    /// Global processing chain applied to all tokens
    std::shared_ptr<BufferProcessingChain> m_global_processing_chain;
};

} // namespace MayaFlux::Buffers
