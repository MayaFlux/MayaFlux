#pragma once

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
std::shared_ptr<Nodes::NodeGraphManager> get_node_graph_manager();

/**
 * @brief Adds a node to the root node of a specific channel
 * @param node Node to add
 * @param channel Channel index
 *
 * Adds the node as a child of the root node for the specified channel.
 * Uses the default engine's node graph manager.
 */
void register_audio_node(std::shared_ptr<Nodes::Node> node, unsigned int channel = 0);

/**
 * @brief Removes a node from the root node of a specific channel
 * @param node Node to remove
 * @param channel Channel index
 *
 * Removes the node from being a child of the root node for the specified channel.
 * Uses the default engine's node graph manager.
 */
void unregister_audio_node(std::shared_ptr<Nodes::Node> node, unsigned int channel = 0);

/**
 * @brief Gets the root node for a specific channel
 * @param channel Channel index
 * @return The root node for the specified channel
 *
 * The root node is the top-level node in the processing hierarchy for a channel.
 * Uses the default engine's node graph manager.
 */
Nodes::RootNode& get_audio_channel_root(u_int32_t channel = 0);

template <typename NodeType, typename... Args>
auto create_node(NodeType, Args&&... args) -> std::shared_ptr<NodeType>
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
std::shared_ptr<Buffers::BufferManager> get_buffer_manager();

/**
 * @brief Gets the audio buffer for a specific channel
 * @param channel Channel index
 * @return Reference to the channel's AudioBuffer
 *
 * Returns the buffer from the default engine's buffer manager.
 */
Buffers::RootAudioBuffer& get_root_audio_buffer(u_int32_t channel);

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

}
