#pragma once

#include "MayaFlux/Buffers/Root/RootAudioBuffer.hpp"

namespace MayaFlux::Nodes {
class Node;
}

namespace MayaFlux::Buffers {

class InputAudioBuffer;

using AudioProcessingFunction = std::function<void(std::shared_ptr<AudioBuffer>)>;
using RootProcessingFunction = std::function<void(std::vector<std::shared_ptr<RootAudioBuffer>>&, u_int32_t)>;

struct RootAudioUnit {
    std::vector<std::shared_ptr<RootAudioBuffer>> root_buffers;
    std::vector<std::shared_ptr<BufferProcessingChain>> processing_chains;
    RootProcessingFunction custom_processor;
    u_int32_t channel_count = 0;
    u_int32_t buffer_size = 512;

    std::shared_ptr<RootAudioBuffer> get_buffer(u_int32_t channel) const
    {
        return root_buffers[channel];
    }

    std::shared_ptr<BufferProcessingChain> get_chain(u_int32_t channel) const
    {
        return processing_chains[channel];
    }

    void resize_channels(u_int32_t new_count, u_int32_t new_buffer_size, ProcessingToken token);

    void resize_buffers(u_int32_t new_buffer_size);
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
class BufferManager {
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
        u_int32_t default_out_channels = 2,
        u_int32_t default_in_channels = 0,
        u_int32_t default_buffer_size = 512,
        ProcessingToken default_processing_token = ProcessingToken::AUDIO_BACKEND);

    ~BufferManager() = default;

    /**
     * @brief Processes all buffers for a specific processing token
     * @param token Processing domain to process
     * @param processing_units Number of processing units (samples for audio, frames for video)
     */
    void process_token(ProcessingToken token, u_int32_t processing_units);

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
        u_int32_t channel,
        u_int32_t processing_units,
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
    std::shared_ptr<RootAudioBuffer> get_root_audio_buffer(ProcessingToken token = ProcessingToken::AUDIO_BACKEND, u_int32_t channel = 0);

    /**
     * @brief Gets data from a specific token and channel
     * @param token Processing domain
     * @param channel Channel index
     * @return Reference to the channel's data vector
     */
    std::vector<double>& get_buffer_data(ProcessingToken token, u_int32_t channel);
    const std::vector<double>& get_buffer_data(ProcessingToken token, u_int32_t channel) const;

    /**
     * @brief Gets the number of channels for a specific processing token
     * @param token Processing domain
     * @return Number of channels in that domain
     */
    u_int32_t get_num_channels(ProcessingToken token) const;

    /**
     * @brief Gets the buffer size for a specific processing token
     * @param token Processing domain
     * @return Buffer size in processing units for that domain
     */
    u_int32_t get_root_audio_buffer_size(ProcessingToken token) const;

    /**
     * @brief Resizes all buffers in a specific token domain
     * @param token Processing domain
     * @param buffer_size New buffer size
     */
    void resize_root_audio_buffers(ProcessingToken token, u_int32_t buffer_size);

    /**
     * @brief Adds a buffer to a specific token and channel
     * @param buffer Buffer to add (must be compatible with the token's root buffer type)
     * @param token Processing domain
     * @param channel Channel index within that domain
     */
    void add_audio_buffer(std::shared_ptr<AudioBuffer> buffer, ProcessingToken token, u_int32_t channel);

    /**
     * @brief Removes a buffer from a specific token and channel
     * @param buffer Buffer to remove
     * @param token Processing domain
     * @param channel Channel index within that domain
     */
    void remove_audio_buffer(std::shared_ptr<AudioBuffer> buffer, ProcessingToken token, u_int32_t channel);

    /**
     * @brief Gets all buffers for a specific token and channel
     * @param token Processing domain
     * @param channel Channel index
     * @return Vector of buffers in that token/channel
     */
    const std::vector<std::shared_ptr<AudioBuffer>>& get_audio_buffers(ProcessingToken token, u_int32_t channel) const;

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
    std::shared_ptr<BufferType> create_buffer(ProcessingToken token, u_int32_t channel, Args&&... args)
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
    std::shared_ptr<BufferProcessingChain> get_processing_chain(ProcessingToken token, u_int32_t channel);

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
    void add_processor(std::shared_ptr<BufferProcessor> processor, std::shared_ptr<AudioBuffer> buffer);

    /**
     * @brief Adds a processor to a specific token and channel
     * @param processor Processor to add
     * @param token Processing domain
     * @param channel Channel index
     */
    void add_processor_to_channel(std::shared_ptr<BufferProcessor> processor, ProcessingToken token, u_int32_t channel);

    /**
     * @brief Adds a processor to all channels in a token domain
     * @param processor Processor to add
     * @param token Processing domain
     */
    void add_processor_to_token(std::shared_ptr<BufferProcessor> processor, ProcessingToken token);

    /**
     * @brief Removes a processor from a specific buffer
     * @param processor Processor to remove
     * @param buffer Target buffer
     */
    void remove_processor(std::shared_ptr<BufferProcessor> processor, std::shared_ptr<AudioBuffer> buffer);

    /**
     * @brief Removes a processor from a specific token and channel
     * @param processor Processor to remove
     * @param token Processing domain
     * @param channel Channel index
     */
    void remove_processor_from_channel(std::shared_ptr<BufferProcessor> processor, ProcessingToken token, u_int32_t channel);

    /**
     * @brief Removes a processor from all channels in a token domain
     * @param processor Processor to remove
     * @param token Processing domain
     */
    void remove_processor_from_token(std::shared_ptr<BufferProcessor> processor, ProcessingToken token);

    /**
     * @brief Sets a final processor for all root buffers in a token domain
     * @param processor Final processor to apply
     * @param token Processing domain
     */
    void set_final_processor(std::shared_ptr<BufferProcessor> processor, ProcessingToken token);

    /**
     * @brief Creates and attaches a quick processing function to a buffer
     * @param processor Processing function
     * @param buffer Target buffer
     * @return Shared pointer to the created processor
     */
    std::shared_ptr<BufferProcessor> attach_quick_process(AudioProcessingFunction processor, std::shared_ptr<AudioBuffer> buffer);

    /**
     * @brief Creates and attaches a quick processing function to a token/channel
     * @param processor Processing function
     * @param token Processing domain
     * @param channel Channel index
     * @return Shared pointer to the created processor
     */
    std::shared_ptr<BufferProcessor> attach_quick_process_to_channel(AudioProcessingFunction processor, ProcessingToken token, u_int32_t channel);

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
    void connect_node_to_channel(std::shared_ptr<Nodes::Node> node, ProcessingToken token, u_int32_t channel, float mix = 0.5f, bool clear_before = false);

    /**
     * @brief Connects a node directly to a specific buffer
     * @param node Node to connect
     * @param buffer Target buffer
     * @param mix Mix level (default: 0.5)
     * @param clear_before Whether to clear buffer before adding node output (default: true)
     */
    void connect_node_to_buffer(std::shared_ptr<Nodes::Node> node, std::shared_ptr<AudioBuffer> buffer, float mix = 0.5f, bool clear_before = true);

    /**
     * @brief Fills token channels from interleaved data
     * @param interleaved_data Source interleaved data
     * @param num_frames Number of frames to process
     * @param token Processing domain (default: AUDIO_BACKEND)
     * @param num_channels Number of channels in the interleaved data
     */
    void fill_from_interleaved(const double* interleaved_data, u_int32_t num_frames,
        ProcessingToken token,
        u_int32_t num_channels);

    /**
     * @brief Fills interleaved data from token channels
     * @param interleaved_data Target interleaved data buffer
     * @param num_frames Number of frames to process
     * @param token Processing domain (default: AUDIO_BACKEND)
     * @param num_channels Number of channels to interleave
     */
    void fill_interleaved(double* interleaved_data, u_int32_t num_frames,
        ProcessingToken token, u_int32_t num_channels) const;

    /**
     * @brief Gets the default processing token used by the manager
     */
    inline ProcessingToken get_default_processing_token() const { return m_default_token; }

    inline void validate_num_channels(ProcessingToken token, u_int32_t num_channels, u_int32_t buffer_size)
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
    void process_input(double* input_data, u_int32_t num_channels, u_int32_t num_frames);

    /**
     * @brief Registers a listener buffer for input channel
     * @param buffer Buffer to receive input data
     * @param input_channel Input channel to listen to
     */
    void register_input_listener(std::shared_ptr<AudioBuffer> buffer, u_int32_t channel);

    /**
     * @brief Unregisters a listener buffer from input channel
     * @param buffer Buffer to stop receiving input data
     * @param input_channel Input channel to stop listening to
     */
    void unregister_input_listener(std::shared_ptr<AudioBuffer> buffer, u_int32_t input_channel);

private:
    /**
     * @brief Gets or creates the root audio unit for a specific processing token
     * @param token Processing domain
     * @return Reference to the root audio unit for that token
     */
    RootAudioUnit& get_or_create_unit(ProcessingToken token);

    /**
     * @brief Ensures a token/channel combination exists
     * @param token Processing domain
     * @param channel Channel index
     */
    void ensure_channels(ProcessingToken token, u_int32_t channel_count);

    /**
     * @brief Ensures a root audio unit exists for a specific token and channel
     * @param token Processing domain
     * @param channel Channel index
     * @return Reference to the root audio unit for that token/channel
     */
    RootAudioUnit& ensure_and_get_unit(ProcessingToken token, u_int32_t channel);

    void setup_input_buffers(u_int32_t default_in_channels, u_int32_t default_buffer_size);

    /**
     * @brief Default processing token for legacy compatibility
     */
    ProcessingToken m_default_token;

    /**
     * @brief Token-based map of root buffer units
     * Maps processing domain -> channel -> {root_buffers, processing chains}
     */
    std::unordered_map<ProcessingToken, RootAudioUnit> m_audio_units;

    std::vector<std::shared_ptr<InputAudioBuffer>> m_input_buffers;

    /**
     * @brief Global processing chain applied to all tokens
     */
    std::shared_ptr<BufferProcessingChain> m_global_processing_chain;

    /**
     * @brief Mutex for thread-safe access
     */
    mutable std::mutex m_manager_mutex;
};

} // namespace MayaFlux::Buffers
