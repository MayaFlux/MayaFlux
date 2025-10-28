#pragma once

#include "MayaFlux/Buffers/Root/RootAudioBuffer.hpp"
#include "MayaFlux/Buffers/Root/RootGraphicsBuffer.hpp"

namespace MayaFlux::Nodes {
class Node;
}

namespace MayaFlux::Buffers {

class InputAudioBuffer;

using AudioProcessingFunction = std::function<void(std::shared_ptr<AudioBuffer>)>;
using RootProcessingFunction = std::function<void(std::vector<std::shared_ptr<RootAudioBuffer>>&, uint32_t)>;

struct RootAudioUnit {
    std::vector<std::shared_ptr<RootAudioBuffer>> root_buffers;
    std::vector<std::shared_ptr<BufferProcessingChain>> processing_chains;
    RootProcessingFunction custom_processor;
    uint32_t channel_count = 0;
    uint32_t buffer_size = 512;

    [[nodiscard]] std::shared_ptr<RootAudioBuffer> get_buffer(uint32_t channel) const
    {
        return root_buffers[channel];
    }

    [[nodiscard]] std::shared_ptr<BufferProcessingChain> get_chain(uint32_t channel) const
    {
        return processing_chains[channel];
    }

    void resize_channels(uint32_t new_count, uint32_t new_buffer_size, ProcessingToken token);

    void resize_buffers(uint32_t new_buffer_size);
};

struct RootGraphicsUnit {
    std::shared_ptr<RootGraphicsBuffer> root_buffer;
    std::shared_ptr<BufferProcessingChain> processing_chain;
    uint32_t frame_count = 0; // For tracking processed frames

    RootGraphicsUnit();

    void initialize(ProcessingToken token);

    [[nodiscard]] std::shared_ptr<RootGraphicsBuffer> get_buffer() const { return root_buffer; }

    [[nodiscard]] std::shared_ptr<BufferProcessingChain> get_chain() const { return processing_chain; }
};

/**
 * @class BufferManager
 * @brief Token-based multimodal buffer management system for unified data stream processing
 *
 * BufferManager serves as the central orchestrator for buffer processing in the MayaFlux engine,
 * implementing a token-based architecture that enables seamless integration of different processing
 * domains while maintaining proven audio processing patterns. This design enables true digital-first,
 * data-driven multimedia processing where audio, video, and custom data streams can interact
 * through shared processing primitives.
 *
 * **Core Architecture:**
 * - **Token-Based Processing**: Each processing domain (AUDIO_BACKEND, GRAPHICS_BACKEND, etc.)
 *   has its own dedicated processing characteristics and backend delegation
 * - **Explicit Root Buffer Types**: Audio uses RootAudioBuffer, video will use RootVideoBuffer, etc.
 *   No generic templates - each domain has its optimized root buffer implementation
 * - **Proven Processing Patterns**: Maintains the existing successful audio processing flow
 *   while extending it to other domains through token-based routing
 * - **Cross-Modal Interaction**: Enables data sharing between domains for advanced effects
 *
 * The manager creates appropriate root buffers and processing chains based on processing tokens,
 * ensuring optimal performance for each domain while enabling cross-domain data interaction.
 */
class MAYAFLUX_API BufferManager {
public:
    /**
     * @brief Creates a new multimodal buffer manager
     * @param default_processing_token Primary processing domain (default: AUDIO_BACKEND)
     * @param default_out_channels Number of output channels for the default domain (default: 2)
     * @param default_in_channels Number of input channels for the default domain (default: 0)
     * @param default_buffer_size Buffer size for the default domain (default: 512)
     *
     * Initializes the buffer manager with a default processing domain. Additional domains
     * are created on-demand when buffers with different tokens are first accessed.
     * This maintains backward compatibility while enabling multimodal processing.
     */
    BufferManager(
        uint32_t default_out_channels = 2,
        uint32_t default_in_channels = 0,
        uint32_t default_buffer_size = 512,
        ProcessingToken default_processing_token = ProcessingToken::AUDIO_BACKEND);

    ~BufferManager() = default;

    /**
     * @brief Processes all buffers for a specific processing token
     * @param token Processing domain to process
     * @param processing_units Number of processing units (samples for audio, frames for video)
     */
    void process_token(ProcessingToken token, uint32_t processing_units);

    /**
     * @brief Processes all active tokens with their configured processing units
     */
    void process_all_tokens();

    /**
     * @brief Processes a specific channel within a processing token domain
     * @param token Processing domain
     * @param channel Channel index within that domain
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
     * @return Vector of processing tokens that have active buffers
     */
    std::vector<ProcessingToken> get_active_tokens() const;

    /**
     * @brief Registers a custom processor for a processing token domain
     * @param token Processing domain to handle
     * @param processor Function that processes all channels for this domain
     */
    void register_token_processor(ProcessingToken token, RootProcessingFunction processor);

    /**
     * @brief Gets the root audio unit for a specific processing token
     * @param token Processing domain
     * @return Reference to the root audio unit for that token
     *
     * This method does not create a new unit or channel if they don't exist.
     */
    const RootAudioUnit& get_audio_unit(ProcessingToken token) const;

    /**
     * @brief Gets a root buffer for a specific token and channel
     * @param token Processing domain
     * @param channel Channel index (default: 0)
     * @return Shared pointer to the appropriate root buffer type
     */
    std::shared_ptr<RootAudioBuffer> get_root_audio_buffer(ProcessingToken token = ProcessingToken::AUDIO_BACKEND, uint32_t channel = 0);

    /**
     * @brief Gets data from a specific token and channel
     * @param token Processing domain
     * @param channel Channel index
     * @return Reference to the channel's data vector
     */
    std::vector<double>& get_buffer_data(ProcessingToken token, uint32_t channel);
    const std::vector<double>& get_buffer_data(ProcessingToken token, uint32_t channel) const;

    /**
     * @brief Gets the number of channels for a specific processing token
     * @param token Processing domain
     * @return Number of channels in that domain
     */
    uint32_t get_num_channels(ProcessingToken token) const;

    /**
     * @brief Gets the buffer size for a specific processing token
     * @param token Processing domain
     * @return Buffer size in processing units for that domain
     */
    uint32_t get_root_audio_buffer_size(ProcessingToken token) const;

    /**
     * @brief Resizes all buffers in a specific token domain
     * @param token Processing domain
     * @param buffer_size New buffer size
     */
    void resize_root_audio_buffers(ProcessingToken token, uint32_t buffer_size);

    /**
     * @brief Gets the root graphics buffer for a specific token
     * @param token Processing domain (typically GRAPHICS_BACKEND)
     * @return Shared pointer to the root graphics buffer
     */
    std::shared_ptr<RootGraphicsBuffer> get_root_graphics_buffer(
        ProcessingToken token = ProcessingToken::GRAPHICS_BACKEND);

    /**
     * @brief Gets the graphics processing chain for a specific token
     * @param token Processing domain
     * @return Shared pointer to the processing chain
     */
    std::shared_ptr<BufferProcessingChain> get_graphics_processing_chain(
        ProcessingToken token = ProcessingToken::GRAPHICS_BACKEND);

    /**
     * @brief Adds a buffer to a specific token and channel
     * @param buffer Buffer to add (must be compatible with the token's root buffer type)
     * @param token Processing domain
     * @param channel Channel index within that domain
     */
    void add_audio_buffer(const std::shared_ptr<AudioBuffer>& buffer, ProcessingToken token, uint32_t channel);

    /**
     * @brief Removes a buffer from a specific token and channel
     * @param buffer Buffer to remove
     * @param token Processing domain
     * @param channel Channel index within that domain
     */
    void remove_audio_buffer(const std::shared_ptr<AudioBuffer>& buffer, ProcessingToken token, uint32_t channel);

    /**
     * @brief Gets all buffers for a specific token and channel
     * @param token Processing domain
     * @param channel Channel index
     * @return Vector of buffers in that token/channel
     */
    const std::vector<std::shared_ptr<AudioBuffer>>& get_audio_buffers(ProcessingToken token, uint32_t channel) const;

    /**
     * @brief Adds a graphics buffer to the specified token
     * @param buffer Graphics buffer to add (must be VKBuffer-derived)
     * @param token Processing domain (default: GRAPHICS_BACKEND)
     *
     * This method:
     * 1. Validates the buffer is a VKBuffer
     * 2. Calls the init hook to allocate GPU resources
     * 3. Adds the buffer to the root graphics buffer for the token
     * 4. Sets up the processing chain for the buffer
     */
    void add_graphics_buffer(const std::shared_ptr<Buffer>& buffer, ProcessingToken token);

    /**
     * @brief Removes a graphics buffer from the specified token
     * @param buffer Graphics buffer to remove
     * @param token Processing domain (default: GRAPHICS_BACKEND)
     *
     * This method:
     * 1. Removes the buffer from the root graphics buffer
     * 2. Calls the cleanup hook to free GPU resources
     * 3. Cleans up the buffer's processing chain
     */
    void remove_graphics_buffer(const std::shared_ptr<Buffer>& buffer, ProcessingToken token);

    /**
     * @brief Gets all graphics buffers for a specific token
     * @param token Processing domain
     * @return Vector of graphics buffers for that token
     */
    const std::vector<std::shared_ptr<VKBuffer>>& get_vulkan_buffers(ProcessingToken token = ProcessingToken::GRAPHICS_BACKEND) const;

    /**
     * @brief Gets graphics buffers filtered by usage type
     * @param usage VKBuffer::Usage type to filter by
     * @param token Processing domain (default: GRAPHICS_BACKEND)
     * @return Vector of buffers matching the specified usage
     */
    std::vector<std::shared_ptr<VKBuffer>> get_vulkan_buffers_by_usage(
        VKBuffer::Usage usage,
        ProcessingToken token = ProcessingToken::GRAPHICS_BACKEND) const;

    /**
     * @brief Adds a processor to the graphics processing chain
     * @param processor Processor to add
     * @param token Processing domain (default: GRAPHICS_BACKEND)
     */
    void add_graphics_processor(
        const std::shared_ptr<BufferProcessor>& processor,
        ProcessingToken token = ProcessingToken::GRAPHICS_BACKEND);

    /**
     * @brief Adds a processor to a specific graphics buffer
     * @param processor Processor to add
     * @param buffer Target buffer
     * @param token Processing domain (default: GRAPHICS_BACKEND)
     */
    void add_graphics_processor(
        const std::shared_ptr<BufferProcessor>& processor,
        const std::shared_ptr<Buffer>& buffer, ProcessingToken token = ProcessingToken::GRAPHICS_BACKEND);

    /**
     * @brief Removes a processor from the graphics processing chain
     * @param processor Processor to remove
     * @param token Processing domain (default: GRAPHICS_BACKEND)
     */
    void remove_graphics_processor(
        const std::shared_ptr<BufferProcessor>& processor,
        ProcessingToken token = ProcessingToken::GRAPHICS_BACKEND);

    /**
     * @brief Creates a specialized buffer and adds it to the specified token/channel
     * @tparam BufferType Type of buffer to create (must be compatible with token)
     * @tparam Args Constructor argument types
     * @param token Processing domain
     * @param channel Channel index
     * @param args Constructor arguments
     * @return Shared pointer to the created buffer
     */
    template <typename BufferType, typename... Args>
    std::shared_ptr<BufferType> create_buffer(ProcessingToken token, uint32_t channel, Args&&... args)
    {
        auto& unit = ensure_and_get_unit(token, channel);
        auto buffer = std::make_shared<BufferType>(channel, unit.buffer_size, std::forward<Args>(args)...);

        if (auto audio_buffer = std::dynamic_pointer_cast<AudioBuffer>(buffer)) {
            add_audio_buffer(audio_buffer, token, channel);
        }
        return buffer;

        // TODO: Add handling for GRAPHICS_BACKEND when RootVideoBuffer exists
    }

    /**
     * @brief Gets the processing chain for a specific token and channel
     * @param token Processing domain
     * @param channel Channel index
     * @return Shared pointer to the processing chain
     */
    std::shared_ptr<BufferProcessingChain> get_processing_chain(ProcessingToken token, uint32_t channel);

    /**
     * @brief Gets the global processing chain (applied to all tokens)
     * @return Shared pointer to the global processing chain
     */
    inline std::shared_ptr<BufferProcessingChain> get_global_processing_chain() { return m_global_processing_chain; }

    /**
     * @brief Adds a processor to a specific buffer
     * @param processor Processor to add
     * @param buffer Target buffer
     */
    void add_processor(const std::shared_ptr<BufferProcessor>& processor, const std::shared_ptr<AudioBuffer>& buffer);

    /**
     * @brief Adds a processor to a specific token and channel
     * @param processor Processor to add
     * @param token Processing domain
     * @param channel Channel index
     */
    void add_processor_to_channel(const std::shared_ptr<BufferProcessor>& processor, ProcessingToken token, uint32_t channel);

    /**
     * @brief Adds a processor to all channels in a token domain
     * @param processor Processor to add
     * @param token Processing domain
     */
    void add_processor_to_token(const std::shared_ptr<BufferProcessor>& processor, ProcessingToken token);

    /**
     * @brief Removes a processor from a specific buffer
     * @param processor Processor to remove
     * @param buffer Target buffer
     */
    void remove_processor(const std::shared_ptr<BufferProcessor>& processor, const std::shared_ptr<AudioBuffer>& buffer);

    /**
     * @brief Removes a processor from a specific token and channel
     * @param processor Processor to remove
     * @param token Processing domain
     * @param channel Channel index
     */
    void remove_processor_from_channel(const std::shared_ptr<BufferProcessor>& processor, ProcessingToken token, uint32_t channel);

    /**
     * @brief Removes a processor from all channels in a token domain
     * @param processor Processor to remove
     * @param token Processing domain
     */
    void remove_processor_from_token(const std::shared_ptr<BufferProcessor>& processor, ProcessingToken token);

    /**
     * @brief Sets a final processor for all root buffers in a token domain
     * @param processor Final processor to apply
     * @param token Processing domain
     */
    void set_final_processor(const std::shared_ptr<BufferProcessor>& processor, ProcessingToken token);

    /**
     * @brief Creates and attaches a quick processing function to a buffer
     * @param processor Processing function
     * @param buffer Target buffer
     * @return Shared pointer to the created processor
     */
    std::shared_ptr<BufferProcessor> attach_quick_process(AudioProcessingFunction processor, const std::shared_ptr<AudioBuffer>& buffer);

    /**
     * @brief Creates and attaches a quick processing function to a token/channel
     * @param processor Processing function
     * @param token Processing domain
     * @param channel Channel index
     * @return Shared pointer to the created processor
     */
    std::shared_ptr<BufferProcessor> attach_quick_process_to_channel(AudioProcessingFunction processor, ProcessingToken token, uint32_t channel);

    /**
     * @brief Creates and attaches a quick processing function to all channels in a token
     * @param processor Processing function
     * @param token Processing domain
     * @return Shared pointer to the created processor
     */
    std::shared_ptr<BufferProcessor> attach_quick_process_to_token(AudioProcessingFunction processor, ProcessingToken token);

    /**
     * @brief Connects a node to a specific token and channel
     * @param node Node to connect
     * @param token Processing domain
     * @param channel Channel index
     * @param mix Mix level (default: 0.5)
     * @param clear_before Whether to clear buffer before adding node output (default: false)
     */
    void connect_node_to_channel(const std::shared_ptr<Nodes::Node>& node, ProcessingToken token, uint32_t channel, float mix = 0.5F, bool clear_before = false);

    /**
     * @brief Connects a node directly to a specific buffer
     * @param node Node to connect
     * @param buffer Target buffer
     * @param mix Mix level (default: 0.5)
     * @param clear_before Whether to clear buffer before adding node output (default: true)
     */
    void connect_node_to_buffer(const std::shared_ptr<Nodes::Node>& node, const std::shared_ptr<AudioBuffer>& buffer, float mix = 0.5F, bool clear_before = true);

    /**
     * @brief Fills token channels from interleaved data
     * @param interleaved_data Source interleaved data
     * @param num_frames Number of frames to process
     * @param token Processing domain (default: AUDIO_BACKEND)
     * @param num_channels Number of channels in the interleaved data
     */
    void fill_from_interleaved(const double* interleaved_data, uint32_t num_frames,
        ProcessingToken token,
        uint32_t num_channels);

    /**
     * @brief Fills interleaved data from token channels
     * @param interleaved_data Target interleaved data buffer
     * @param num_frames Number of frames to process
     * @param token Processing domain (default: AUDIO_BACKEND)
     * @param num_channels Number of channels to interleave
     */
    void fill_interleaved(double* interleaved_data, uint32_t num_frames,
        ProcessingToken token, uint32_t num_channels) const;

    /**
     * @brief Clones the buffer for each channel in the specified vector
     * @param buffer Buffer to clone
     * @param channels Vector of channel indices to clone for
     * @param token Processing domain (default: AUDIO_BACKEND)
     *
     * This method is a wrapper around AudioBuffer::clone_to() that creates a new buffer
     * but can accomodate multiple channels at once
     */
    void clone_buffer_for_channels(const std::shared_ptr<AudioBuffer>& buffer, const std::vector<uint32_t>& channels,
        ProcessingToken token = ProcessingToken::AUDIO_BACKEND);

    /**
     * @brief Gets the default processing token used by the manager
     */
    inline ProcessingToken get_default_processing_token() const { return m_default_token; }

    /**
     * @brief Validates the number of channels and resizes buffers if necessary
     * @param token Processing domain
     * @param num_channels Number of channels to validate
     * @param buffer_size New buffer size to set
     *
     * This method ensures that the specified number of channels exists for the given token,
     * resizing the root audio buffers accordingly.
     */
    inline void validate_num_channels(ProcessingToken token, uint32_t num_channels, uint32_t buffer_size)
    {
        ensure_channels(token, num_channels);
        resize_root_audio_buffers(token, buffer_size);
    }

    /**
     * @brief Processes input data for a specific token and number of channels
     * @param input_data Pointer to the input data buffer
     * @param num_channels Number of channels in the input data
     * @param num_frames Number of frames to process
     */
    void process_input(double* input_data, uint32_t num_channels, uint32_t num_frames);

    /**
     * @brief Registers a listener buffer for input channel
     * @param buffer Buffer to receive input data
     * @param input_channel Input channel to listen to
     */
    void register_input_listener(const std::shared_ptr<AudioBuffer>& buffer, uint32_t channel);

    /**
     * @brief Unregisters a listener buffer from input channel
     * @param buffer Buffer to stop receiving input data
     * @param input_channel Input channel to stop listening to
     */
    void unregister_input_listener(const std::shared_ptr<AudioBuffer>& buffer, uint32_t input_channel);

    /**
     * @brief Supplies a buffer to a specific token and channel
     * @param buffer Buffer to supply
     * @param token Processing domain
     * @param channel Channel index within that domain
     * @param mix Mix level (default: 1.0)
     * @return True if the buffer was successfully supplied, false otherwise
     *
     * This method allows external buffers to be supplied to the manager for processing.
     * The buffer data is added, mixed and normalized at the end of the processing chain of channel's root
     * but before final processing.
     * This method is useful when one AudioBuffer needs to be supplied to multiple channels.
     * This is the only continuous way to have same buffer data in multiple channels.
     * Else look into ::clone_buffer_for_channels() for cloning a buffer to multiple channels.
     */
    bool supply_buffer_to(const std::shared_ptr<AudioBuffer>& buffer, ProcessingToken token, uint32_t channel, double mix = 1.);

    /**
     * @brief Removes a supplied buffer from a specific token and channel
     * @param buffer Buffer to remove
     * @param token Processing domain
     * @param channel Channel index within that domain
     * @return True if the buffer was successfully removed, false otherwise
     *
     * This method removes a previously supplied buffer from the specified token and channel.
     * It is useful for cleaning up buffers that are no longer needed.
     */
    bool remove_supplied_buffer(const std::shared_ptr<AudioBuffer>& buffer, ProcessingToken token, uint32_t channel);

    /**
     * @brief Unregisters the buffer init/cleanup callbacks for a specific processing token
     * @param token Processing domain
     */
    void unregister_graphics_context(ProcessingToken token);

    /**
     * @brief Gets the Vulkan processing context for graphics buffers
     * @return Shared pointer to the VKProcessingContext
     */
    inline std::shared_ptr<VKProcessingContext> get_graphics_processing_context(ProcessingToken token)
    {
        return m_graphics_processing_contexts[token];
    }

    /**
     * @brief Sets the Vulkan processing context for graphics buffers
     * @param context Shared pointer to the VKProcessingContext
     */
    inline void set_graphics_processing_context(const std::shared_ptr<VKProcessingContext>& context, ProcessingToken token)
    {
        m_graphics_processing_contexts[token] = context;
    }

private:
    /**
     * @brief Gets or creates the root audio unit for a specific processing token
     * @param token Processing domain
     * @return Reference to the root audio unit for that token
     */
    RootAudioUnit& get_or_create_unit(ProcessingToken token);

    /**
     * @brief Gets or creates the graphics unit for a specific token
     * @param token Processing domain
     * @return Reference to the graphics unit
     */
    RootGraphicsUnit& get_or_create_graphics_unit(ProcessingToken token);

    /**
     * @brief Gets the graphics unit for a specific token (const version)
     * @param token Processing domain
     * @return Const reference to the graphics unit
     * @throws std::out_of_range if token not found
     */
    const RootGraphicsUnit& get_graphics_unit(ProcessingToken token) const;

    /**
     * @brief Ensures a token/channel combination exists
     * @param token Processing domain
     * @param channel Channel index
     */
    void ensure_channels(ProcessingToken token, uint32_t channel_count);

    /**
     * @brief Ensures a root audio unit exists for a specific token and channel
     * @param token Processing domain
     * @param channel Channel index
     * @return Reference to the root audio unit for that token/channel
     */
    RootAudioUnit& ensure_and_get_unit(ProcessingToken token, uint32_t channel);

    /**
     * @brief Sets up input audio buffers
     * @param default_in_channels Number of input channels
     * @param default_buffer_size Buffer size for input channels
     */
    void setup_input_buffers(uint32_t default_in_channels, uint32_t default_buffer_size);

    /**
     * @brief Default processing token for legacy compatibility
     */
    ProcessingToken m_default_token;

    /**
     * @brief Token-based map of root buffer units
     * Maps processing domain -> channel -> {root_buffers, processing chains}
     */
    std::unordered_map<ProcessingToken, RootAudioUnit> m_audio_units;

    /**
     * @brief Token-based map of root graphics units
     */
    std::unordered_map<ProcessingToken, RootGraphicsUnit> m_graphics_units;

    /**
     * @brief Input buffers for capturing audio input data
     */
    std::vector<std::shared_ptr<InputAudioBuffer>> m_input_buffers;

    /**
     * @brief Global processing chain applied to all tokens
     */
    std::shared_ptr<BufferProcessingChain> m_global_processing_chain;

    /**
     * @brief Mutex for thread-safe access
     */
    mutable std::mutex m_manager_mutex;

    /**
     * @brief Vulkan processing context for graphics buffers
     */
    std::unordered_map<ProcessingToken, std::shared_ptr<VKProcessingContext>> m_graphics_processing_contexts;

    /**
     * @brief Registers graphics processor with relevant context
     * @param processor Processor to set up
     * @param token Processing domain
     */
    void setup_graphics_processor(const std::shared_ptr<BufferProcessor>& processor, ProcessingToken token);
};

} // namespace MayaFlux::Buffers
