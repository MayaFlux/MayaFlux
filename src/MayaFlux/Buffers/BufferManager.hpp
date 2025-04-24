#pragma once

#include "MayaFlux/Buffers/Root/RootAudioBuffer.hpp"

namespace MayaFlux::Nodes {
class Node;
}

namespace MayaFlux::Buffers {

/**
 * @typedef AudioProcessingFunction
 * @brief Function type for data transformation operations
 *
 * A function that takes a shared pointer to an AudioBuffer and performs
 * some transformation operation on it. Used for creating simple processors
 * without defining full BufferProcessor classes.
 */
using AudioProcessingFunction = std::function<void(std::shared_ptr<AudioBuffer>)>;

/**
 * @class BufferManager
 * @brief Central coordinator for data buffers and transformation chains
 *
 * BufferManager is a core component of the Maya Flux engine, responsible for
 * coordinating all buffer-related operations across multiple data channels. It serves
 * as the bridge between hardware interfaces (via Engine and Device classes) and the
 * buffer processing system.
 *
 * Key responsibilities:
 * - Managing root buffers for each data channel
 * - Coordinating transformation chains at channel and global levels
 * - Connecting computational nodes to buffers for data generation
 * - Converting between interleaved and per-channel data formats
 * - Ensuring thread-safe data processing for real-time operations
 *
 * While users can create their own instances of most buffer classes, BufferManager
 * is typically used through the Engine's centrally managed instance. This ensures
 * proper synchronization with hardware interfaces and the node graph system.
 *
 * The manager provides a hierarchical approach to data transformation:
 * 1. Individual buffers can have their own processors
 * 2. Each channel has a processing chain applied to all buffers in that channel
 * 3. A global processing chain applies to all buffers across all channels
 *
 * This hierarchy allows for efficient application of transformations at different levels
 * of granularity, from individual data streams to the final aggregated output.
 */
class BufferManager {
public:
    /**
     * @brief Creates a new buffer manager
     * @param num_channels Number of data channels to manage
     * @param num_frames Buffer capacity per channel
     *
     * Initializes a buffer manager with the specified number of channels and frame capacity.
     * Each channel gets its own RootAudioBuffer and processing chain.
     */
    BufferManager(u_int32_t num_channels, u_int32_t num_frames);

    /**
     * @brief Default destructor
     */
    ~BufferManager() = default;

    /**
     * @brief Gets the number of data channels
     * @return Number of channels
     */
    inline u_int32_t get_num_channels() const { return m_num_channels; }

    /**
     * @brief Gets the buffer capacity per channel
     * @return Buffer capacity in frames
     */
    inline u_int32_t get_num_frames() const { return m_num_frames; }

    /**
     * @brief Gets the root buffer for a channel
     * @param channel_index Channel index (0-based)
     * @return Shared pointer to the channel's root buffer
     *
     * This provides access to the root buffer for a specific channel,
     * which is the final aggregation point before data reaches hardware interfaces.
     */
    std::shared_ptr<AudioBuffer> get_channel(u_int32_t channel_index);

    /**
     * @brief Gets the root buffer for a channel (const version)
     * @param channel_index Channel index (0-based)
     * @return Const shared pointer to the channel's root buffer
     */
    const std::shared_ptr<AudioBuffer> get_channel(u_int32_t channel_index) const;

    /**
     * @brief Adds a buffer to a channel
     * @param channel_index Channel index (0-based)
     * @param buffer Buffer to add
     *
     * Adds the buffer as a tributary to the channel's root buffer,
     * making it contribute to the channel's output.
     */
    void add_buffer_to_channel(u_int32_t channel_index, std::shared_ptr<AudioBuffer> buffer);

    /**
     * @brief Removes a buffer from a channel
     * @param channel_index Channel index (0-based)
     * @param buffer Buffer to remove
     */
    void remove_buffer_from_channel(u_int32_t channel_index, std::shared_ptr<AudioBuffer> buffer);

    /**
     * @brief Gets all buffers in a channel
     * @param channel_index Channel index (0-based)
     * @return Const reference to the vector of buffers in the channel
     */
    const std::vector<std::shared_ptr<AudioBuffer>>& get_channel_buffers(u_int32_t channel_index) const;

    /**
     * @brief Gets the data for a channel
     * @param channel_index Channel index (0-based)
     * @return Reference to the vector of data values for the channel
     *
     * This provides direct access to the data in the channel's root buffer.
     */
    std::vector<double>& get_channel_data(u_int32_t channel_index);

    /**
     * @brief Gets the data for a channel (const version)
     * @param channel_index Channel index (0-based)
     * @return Const reference to the vector of data values for the channel
     */
    const std::vector<double>& get_channel_data(u_int32_t channel_index) const;

    /**
     * @brief Processes all buffers in a channel
     * @param channel_index Channel index (0-based)
     *
     * This triggers processing of the channel's root buffer, which in turn
     * processes all tributary buffers and applies the channel's transformation chain.
     */
    void process_channel(u_int32_t channel_index);

    /**
     * @brief Processes all channels
     *
     * This processes all channels in sequence, preparing the final
     * output for delivery to hardware interfaces.
     */
    void process_all_channels();

    /**
     * @brief Gets the transformation chain for a channel
     * @param channel_index Channel index (0-based)
     * @return Shared pointer to the channel's processing chain
     *
     * This provides access to the transformation chain that applies to
     * all buffers in the specified channel.
     */
    std::shared_ptr<BufferProcessingChain> get_channel_processing_chain(u_int32_t channel_index);

    /**
     * @brief Gets the global transformation chain
     * @return Shared pointer to the global processing chain
     *
     * This provides access to the transformation chain that applies to
     * all buffers across all channels.
     */
    inline std::shared_ptr<BufferProcessingChain> get_global_processing_chain() { return m_global_processing_chain; }

    /**
     * @brief Adds a processor to a buffer
     * @param processor Processor to add
     * @param buffer Buffer to process
     *
     * This adds the processor to the buffer's transformation chain.
     */
    void add_processor(std::shared_ptr<BufferProcessor> processor, std::shared_ptr<AudioBuffer> buffer);

    /**
     * @brief Adds a processor to all buffers in a channel
     * @param processor Processor to add
     * @param channel_index Channel index (0-based)
     *
     * This adds the processor to the channel's transformation chain,
     * affecting all buffers in that channel.
     */
    void add_processor_to_channel(std::shared_ptr<BufferProcessor> processor, u_int32_t channel_index);

    /**
     * @brief Adds a processor to all buffers in all channels
     * @param processor Processor to add
     *
     * This adds the processor to the global transformation chain,
     * affecting all buffers across all channels.
     */
    void add_processor_to_all(std::shared_ptr<BufferProcessor> processor);

    /**
     * @brief Removes a processor from a buffer
     * @param processor Processor to remove
     * @param buffer Buffer to remove the processor from
     */
    void remove_processor(std::shared_ptr<BufferProcessor> processor, std::shared_ptr<AudioBuffer> buffer);

    /**
     * @brief Removes a processor from all buffers in a channel
     * @param processor Processor to remove
     * @param channel_index Channel index (0-based)
     */
    void remove_processor_from_channel(std::shared_ptr<BufferProcessor> processor, u_int32_t channel_index);

    /**
     * @brief Removes a processor from all buffers in all channels
     * @param processor Processor to remove
     */
    void remove_processor_from_all(std::shared_ptr<BufferProcessor> processor);

    /**
     * @brief Creates and attaches a simple processor from a function
     * @param processor Function that transforms a buffer
     * @param buffer Buffer to process
     * @return Shared pointer to the created processor
     *
     * This is a convenience method for creating simple processors
     * without defining full BufferProcessor classes.
     */
    std::shared_ptr<BufferProcessor> attach_quick_process(AudioProcessingFunction processor, std::shared_ptr<AudioBuffer> buffer);

    /**
     * @brief Creates and attaches a simple processor to all buffers in a channel
     * @param processor Function that transforms a buffer
     * @param channel_index Channel index (0-based)
     * @return Shared pointer to the created processor
     */
    std::shared_ptr<BufferProcessor> attach_quick_process_to_channel(AudioProcessingFunction processor, u_int32_t channel_index);

    /**
     * @brief Creates and attaches a simple processor to all buffers in all channels
     * @param processor Function that transforms a buffer
     * @return Shared pointer to the created processor
     */
    std::shared_ptr<BufferProcessor> attach_quick_process_to_all(AudioProcessingFunction processor);

    /**
     * @brief Sets a final processor for all root buffers
     * @param processor Processor to set as final
     *
     * The final processor is applied after all other transformations,
     * making it ideal for boundary enforcement and normalization operations.
     */
    void set_final_processor_for_root_buffers(std::shared_ptr<BufferProcessor> processor);

    /**
     * @brief Connects a computational node to a channel
     * @param node Node to connect
     * @param channel_index Channel index (0-based)
     * @param mix Mix level (0.0-1.0)
     * @param clear_before Whether to clear the buffer before adding node output
     *
     * This creates a NodeSourceProcessor that captures output from the node
     * and adds it to the channel's root buffer.
     */
    void connect_node_to_channel(std::shared_ptr<Nodes::Node> node, u_int32_t channel_index, float mix = 0.5, bool clear_before = false);

    /**
     * @brief Connects a computational node to a buffer
     * @param node Node to connect
     * @param buffer Buffer to connect to
     * @param mix Mix level (0.0-1.0)
     * @param clear_before Whether to clear the buffer before adding node output
     *
     * This creates a NodeSourceProcessor that captures output from the node
     * and adds it to the specified buffer.
     */
    void connect_node_to_buffer(std::shared_ptr<Nodes::Node> node, std::shared_ptr<AudioBuffer>, float mix = 0.5, bool clear_before = true);

    /**
     * @brief Creates a specialized buffer and adds it to a channel
     * @tparam BufferType Type of buffer to create
     * @tparam Args Types of additional constructor arguments
     * @param channel_index Channel index (0-based)
     * @param args Additional arguments to pass to the buffer constructor
     * @return Shared pointer to the created buffer
     *
     * This is a template method that creates a buffer of the specified type,
     * adds it to the specified channel, and returns a pointer to it.
     *
     * Example usage:
     * ```
     * auto feedback_buffer = buffer_manager->create_specialized_buffer<FeedbackBuffer>(0, 0.7f);
     * ```
     */
    template <typename BufferType, typename... Args>
    std::shared_ptr<BufferType> create_specialized_buffer(u_int32_t channel_index, Args&&... args)
    {
        if (channel_index >= m_num_channels) {
            throw std::out_of_range("Channel index out of range");
        }

        auto buffer = std::make_shared<BufferType>(channel_index, m_num_frames, std::forward<Args>(args)...);
        buffer->set_processing_chain(m_channel_processing_chains[channel_index]);
        m_root_buffers[channel_index]->add_child_buffer(buffer);

        return buffer;
    }

    void fill_from_interleaved(const double* interleaved_data, u_int32_t num_frames);
    void fill_interleaved(double* interleaved_data, u_int32_t num_frames) const;

    void resize(u_int32_t num_frames);

private:
    /**
     * @brief Number of data channels managed
     */
    u_int32_t m_num_channels;

    /**
     * @brief Number of frames per buffer
     */
    u_int32_t m_num_frames;

    /**
     * @brief Root buffers for each channel
     *
     * These are the final aggregation points for each channel before
     * data is sent to hardware interfaces.
     */
    std::vector<std::shared_ptr<RootAudioBuffer>> m_root_buffers;

    /**
     * @brief Processing chains for each channel
     *
     * Each channel has its own processing chain that applies to
     * all buffers in that channel.
     */
    std::vector<std::shared_ptr<BufferProcessingChain>> m_channel_processing_chains;

    /**
     * @brief Global processing chain
     *
     * This processing chain applies to all buffers across all channels.
     * It's typically used for boundary enforcement and normalization operations.
     */
    std::shared_ptr<BufferProcessingChain> m_global_processing_chain;
};

}
