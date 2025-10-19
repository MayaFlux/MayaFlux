#pragma once

#include "MayaFlux/Core/ProcessingTokens.hpp"

namespace MayaFlux {

namespace Nodes {
    class Node;
    class NodeGraphManager;
    class RootNode;
}

namespace Buffers {
    class AudioBuffer;
    class RootAudioBuffer;
    class BufferManager;
    class BufferProcessor;
    class BufferProcessingChain;
}

/**
 * @typedef AudioProcessingFunction
 * @brief Function type for audio buffer processing callbacks
 */
using AudioProcessingFunction = std::function<void(std::shared_ptr<Buffers::AudioBuffer>)>;

//-------------------------------------------------------------------------
// Node Graph Management
//-------------------------------------------------------------------------

/**
 * @brief Gets the node graph manager from the default engine
 * @return Shared pointer to the centrally managed NodeGraphManager
 *
 * Returns the node graph manager that's managed by the default engine instance.
 * All node operations using the convenience functions will use this manager.
 */
MAYAFLUX_API std::shared_ptr<Nodes::NodeGraphManager> get_node_graph_manager();

/**
 * @brief Adds a node to the root node of a specific channel
 * @param node Node to add
 * @param channel Channel index
 *
 * Adds the node as a child of the root node for the specified channel.
 * Uses the default engine's node graph manager.
 */
MAYAFLUX_API void register_audio_node(std::shared_ptr<Nodes::Node> node, uint32_t channel = 0);

/**
 * @brief Adds a node to the root node of specified channels
 * @param node Node to add
 * @param channels Vector of channel indices
 *
 * Adds the node as a child of the root node for the specified channels.
 * Uses the default engine's node graph manager.
 */
MAYAFLUX_API void register_audio_node(std::shared_ptr<Nodes::Node> node, std::vector<uint32_t> channels);

MAYAFLUX_API void register_node(
    const std::shared_ptr<Nodes::Node>& node,
    const Nodes::ProcessingToken& token,
    uint32_t channel = 0);

/**
 * @brief Removes a node from the root node of a specific channel
 * @param node Node to remove
 * @param channel Channel index
 *
 * Removes the node from being a child of the root node for the specified channel.
 * Uses the default engine's node graph manager.
 */
MAYAFLUX_API void unregister_audio_node(std::shared_ptr<Nodes::Node> node, uint32_t channel = 0);

MAYAFLUX_API void unregister_node(
    const std::shared_ptr<Nodes::Node>& node,
    const Nodes::ProcessingToken& token,
    uint32_t channel = 0);

/**
 * @brief Removes a node from the root node from list of channels
 * @param node Node to remove
 * @param channels Channel indices
 *
 * Removes the node from being a child of the root node for the list of channels
 * Uses the default engine's node graph manager.
 */
MAYAFLUX_API void unregister_audio_node(std::shared_ptr<Nodes::Node> node, std::vector<uint32_t> channels);

/**
 * @brief Gets the root node for a specific channel
 * @param channel Channel index
 * @return The root node for the specified channel
 *
 * The root node is the top-level node in the processing hierarchy for a channel.
 * Uses the default engine's node graph manager.
 */
MAYAFLUX_API Nodes::RootNode& get_audio_channel_root(uint32_t channel = 0);

template <typename NodeType, typename... Args>
    requires std::derived_from<NodeType, Nodes::Node>
auto create_node(Args&&... args) -> std::shared_ptr<NodeType>
{
    auto node = std::make_shared<NodeType>(std::forward<Args>(args)...);
    register_audio_node(node);
    return node;
}

//-------------------------------------------------------------------------
// Buffer Management
//-------------------------------------------------------------------------

/**
 * @brief Gets the buffer manager from the default engine
 * @return Shared pointer to the centrally managed BufferManager
 *
 * Returns the buffer manager that's managed by the default engine instance.
 * All buffer operations using the convenience functions will use this manager.
 */
MAYAFLUX_API std::shared_ptr<Buffers::BufferManager> get_buffer_manager();

/**
 * @brief Adds a processor to a specific buffer
 * @param processor Processor to add
 * @param buffer Buffer to add the processor to
 *
 * Adds the processor to the specified buffer's processing chain.
 * Uses the default engine's buffer manager.
 */
MAYAFLUX_API void add_processor_to_buffer(std::shared_ptr<Buffers::BufferProcessor> processor, std::shared_ptr<Buffers::AudioBuffer> buffer);

/**
 * @brief Registers an AudioBuffer with the default engine's buffer manager
 * @param buffer AudioBuffer to register
 * @param channel Channel index to associate with the buffer (default: 0)
 *
 * Adds the buffer to the default engine's buffer management system, enabling
 * it to participate in the audio processing pipeline. The buffer will be
 * processed during each audio cycle according to its configuration.
 * Multiple buffers can be registered to the same channel for layered processing.
 */
MAYAFLUX_API void register_audio_buffer(std::shared_ptr<Buffers::AudioBuffer> buffer, uint32_t channel = 0);

/**
 * @brief Unregisters an AudioBuffer from the default engine's buffer manager
 * @param buffer AudioBuffer to unregister
 * @param channel Channel index the buffer was associated with (default: 0)
 *
 * Removes the buffer from the default engine's buffer management system.
 * The buffer will no longer participate in audio processing cycles.
 * This is essential for clean shutdown and preventing processing of
 * destroyed or invalid buffers.
 */
MAYAFLUX_API void unregister_audio_buffer(std::shared_ptr<Buffers::AudioBuffer> buffer, uint32_t channel = 0);

/**
 * @brief creates a new buffer of the specified type and registers it
 * @tparam BufferType Type of buffer to create (must be derived from AudioBuffer)
 * @tparam Args Constructor argument types
 * @param channel Channel index to create the buffer for
 * @param buffer_size Size of the buffer in processing units
 */
template <typename BufferType, typename... Args>
    requires std::derived_from<BufferType, Buffers::AudioBuffer>
auto create_buffer(uint32_t channel, uint32_t buffer_size, Args&&... args) -> std::shared_ptr<BufferType>
{
    auto buffer = std::make_shared<BufferType>(channel, buffer_size, std::forward<Args>(args)...);
    register_audio_buffer(buffer, channel);
    return buffer;
}

/**
 *  @brief Creates a new processor and adds it to a buffer
 * @tparam ProcessorType Type of processor to create (must be derived from BufferProcessor)
 * @tparam Args Constructor argument types
 * @param buffer Buffer to add the processor to
 * @param args Constructor arguments for the processor
 * @return Shared pointer to the created processor
 *
 * This function creates a new processor of the specified type, initializes it with the provided arguments,
 * and adds it to the specified buffer's processing chain.
 */
template <typename ProcessorType, typename... Args>
    requires std::derived_from<ProcessorType, Buffers::BufferProcessor>
auto create_processor(std::shared_ptr<Buffers::AudioBuffer> buffer, Args&&... args) -> std::shared_ptr<ProcessorType>
{
    auto processor = std::make_shared<ProcessorType>(std::forward<Args>(args)...);
    add_processor_to_buffer(processor, buffer);
    return processor;
}

/**
 * @brief Creates a new processing chain for the default engine
 * @return Shared pointer to the created BufferProcessingChain
 *
 * This function creates a new processing chain that can be used to manage
 * audio processing for buffers. The chain can be customized with various
 * processors and is managed by the default engine's buffer manager.
 */
MAYAFLUX_API std::shared_ptr<Buffers::BufferProcessingChain> create_processing_chain();

/**
 * @brief Gets the audio buffer for a specific channel
 * @param channel Channel index
 * @return Reference to the channel's AudioBuffer
 *
 * Returns the buffer from the default engine's buffer manager.
 */
MAYAFLUX_API Buffers::RootAudioBuffer& get_root_audio_buffer(uint32_t channel);

/**
 * @brief Connects a node to a specific output channel
 * @param node Node to connect
 * @param channel_index Channel index to connect to
 * @param mix Mix level (0.0 to 1.0) for blending node output with existing channel content
 * @param clear_before Whether to clear the channel buffer before adding node output
 *
 * Uses the default engine's buffer manager and node graph manager.
 */
MAYAFLUX_API void connect_node_to_channel(std::shared_ptr<Nodes::Node> node, uint32_t channel_index = 0, float mix = 0.5F, bool clear_before = false);

/**
 * @brief Connects a node to a specific buffer
 * @param node Node to connect
 * @param buffer Buffer to connect to
 * @param mix Mix level (0.0 to 1.0) for blending node output with existing buffer content
 * @param clear_before Whether to clear the buffer before adding node output
 *
 * Uses the default engine's node graph manager.
 */
MAYAFLUX_API void connect_node_to_buffer(std::shared_ptr<Nodes::Node> node, std::shared_ptr<Buffers::AudioBuffer> buffer, float mix = 0.5F, bool clear_before = true);

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
MAYAFLUX_API std::shared_ptr<Buffers::BufferProcessor> attach_quick_process(AudioProcessingFunction processor, std::shared_ptr<Buffers::AudioBuffer> buffer);

/**
 * @brief Attaches a processing function to a specific channel
 * @param processor Function to process the buffer
 * @param channel_id Channel index to process
 *
 * The processor will be called during the default engine's audio processing cycle
 * and will operate on the specified output channel buffer.
 */
MAYAFLUX_API std::shared_ptr<Buffers::BufferProcessor> attach_quick_process_to_audio_channel(AudioProcessingFunction processor, unsigned int channel_id = 0);

/**
 * @brief Attaches a processing function to multiple channels
 * @param processor Function to process the buffer
 * @param channels Vector of channel indices to process
 *
 * The processor will be called during the default engine's audio processing cycle
 * for each of the specified channel buffers.
 */
MAYAFLUX_API std::shared_ptr<Buffers::BufferProcessor> attach_quick_process_to_audio_channels(AudioProcessingFunction processor, const std::vector<unsigned int> channels);

/**
 * @brief Reads audio data from the default input source into a buffer
 * @param buffer Buffer to read audio data into
 * @param channel Channel index to read from (default: 0)
 *
 * Reads audio data from the default input source (e.g., microphone)
 * and fills the provided AudioBuffer with the captured audio samples.
 * This function is typically used to capture live audio input for processing.
 */
MAYAFLUX_API void read_from_audio_input(std::shared_ptr<Buffers::AudioBuffer> buffer, uint32_t channel = 0);

/**
 * @brief Stops reading audio data from the default input source
 * @param buffer Buffer to stop reading audio data from
 * @param channel Channel index to stop reading from (default: 0)
 */
MAYAFLUX_API void detach_from_audio_input(std::shared_ptr<Buffers::AudioBuffer> buffer, uint32_t channel = 0);

/**
 * @brief Creates a new AudioBuffer for input listening
 * @param channel Channel index to create the buffer for (default: 0)
 * @param add_to_output Whether to add this buffer to the output channel (default: false)
 * @return Shared pointer to the created AudioBuffer
 *
 * This function creates a new AudioBuffer that can be used to listen to audio input.
 * If `add_to_output` is true, the buffer will be automatically added to the output channel
 * for processing.
 */
MAYAFLUX_API std::shared_ptr<Buffers::AudioBuffer> create_input_listener_buffer(uint32_t channel = 0, bool add_to_output = false);

/**
 * @brief Clones a buffer to multiple channels
 * @param buffer Buffer to clone
 * @param channels Vector of channel indices to clone to
 *
 * Creates independent copies of the buffer for each specified channel.
 * Each clone maintains the same data, processors, and processing chain,
 * but operates independently on its assigned channel.
 * Uses the default engine's buffer manager.
 */
MAYAFLUX_API void clone_buffer_to_channels(std::shared_ptr<Buffers::AudioBuffer> buffer,
    const std::vector<uint32_t>& channels);

MAYAFLUX_API void clone_buffer_to_channels(
    std::shared_ptr<Buffers::AudioBuffer> buffer,
    const std::vector<uint32_t>& channels,
    const Buffers::ProcessingToken& token);
/**
 * @brief Supplies a buffer to a single channel with mixing
 * @param buffer Source buffer to supply
 * @param channel Target channel index
 * @param mix Mix level (default: 1.0)
 *
 * Convenience wrapper for single-channel buffer supply operations.
 */
MAYAFLUX_API void supply_buffer_to_channel(std::shared_ptr<Buffers::AudioBuffer> buffer,
    uint32_t channel,
    double mix = 1.0);

/**
 * @brief Supplies a buffer to multiple channels with mixing
 * @param buffer Source buffer to supply
 * @param channels Vector of channel indices to supply to
 * @param mix Mix level for each channel (default: 1.0)
 *
 * Efficiently routes a single buffer's output to multiple channels using
 * the MixProcessor system. This is ideal for sending the same signal to
 * multiple outputs without duplicating processing.
 */
MAYAFLUX_API void supply_buffer_to_channels(std::shared_ptr<Buffers::AudioBuffer> buffer,
    const std::vector<uint32_t>& channels,
    double mix = 1.0);

/**
 * @brief Removes a supplied buffer from multiple channels
 * @param buffer Buffer to remove from supply chains
 * @param channels channel index to remove from
 *
 * Efficiently removes a buffer from channel mix processor.
 */
MAYAFLUX_API void remove_supplied_buffer_from_channel(std::shared_ptr<Buffers::AudioBuffer> buffer,
    const uint32_t channel);

/**
 * @brief Removes a supplied buffer from multiple channels
 * @param buffer Buffer to remove from supply chains
 * @param channels Vector of channel indices to remove from
 *
 * Efficiently removes a buffer from multiple channel mix processors.
 */
MAYAFLUX_API void remove_supplied_buffer_from_channels(std::shared_ptr<Buffers::AudioBuffer> buffer,
    const std::vector<uint32_t>& channels);

}
