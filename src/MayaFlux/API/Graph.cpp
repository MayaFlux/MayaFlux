#include "Graph.hpp"
#include "Core.hpp"

#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Core/Engine.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"

namespace MayaFlux {

//-------------------------------------------------------------------------
// Node Graph Management
//-------------------------------------------------------------------------

std::shared_ptr<Nodes::NodeGraphManager> get_node_graph_manager()
{
    return get_context().get_node_graph_manager();
}

void register_audio_node(std::shared_ptr<Nodes::Node> node, unsigned int channel)
{
    get_context().get_node_graph_manager()->add_to_root(node, Nodes::ProcessingToken::AUDIO_RATE, channel);
}

void unregister_audio_node(std::shared_ptr<Nodes::Node> node, unsigned int channel)
{
    get_context().get_node_graph_manager()->get_token_roots(Nodes::ProcessingToken::AUDIO_RATE)[channel]->unregister_node(node);
}

Nodes::RootNode& get_audio_channel_root(u_int32_t channel)
{
    return *get_context().get_node_graph_manager()->get_token_roots(Nodes::ProcessingToken::AUDIO_RATE)[channel];
}

//-------------------------------------------------------------------------
// Buffer Management
//-------------------------------------------------------------------------

std::shared_ptr<Buffers::BufferManager> get_buffer_manager()
{
    return get_context().get_buffer_manager();
}

Buffers::RootAudioBuffer& get_root_audio_buffer(u_int32_t channel)
{
    return *get_buffer_manager()->get_root_buffer(Buffers::ProcessingToken::AUDIO_BACKEND, channel);
}

void connect_node_to_channel(std::shared_ptr<Nodes::Node> node, u_int32_t channel_index, float mix, bool clear_before)
{
    auto token = get_buffer_manager()->get_default_processing_token();
    get_buffer_manager()->connect_node_to_token_channel(node, token, 0, mix, clear_before);
}

void connect_node_to_buffer(std::shared_ptr<Nodes::Node> node, std::shared_ptr<Buffers::AudioBuffer> buffer, float mix, bool clear_before)
{
    get_buffer_manager()->connect_node_to_buffer(node, buffer, mix, clear_before);
}

//-------------------------------------------------------------------------
// Audio Processing
//-------------------------------------------------------------------------

std::shared_ptr<Buffers::BufferProcessor> attach_quick_process(AudioProcessingFunction processor, std::shared_ptr<Buffers::AudioBuffer> buffer)
{
    return get_buffer_manager()->attach_quick_process(processor, buffer);
}

std::shared_ptr<Buffers::BufferProcessor> attach_quick_process_to_channel(AudioProcessingFunction processor, unsigned int channel_id)
{
    return get_buffer_manager()->attach_quick_process_to_channel(processor, channel_id);
}

std::shared_ptr<Buffers::BufferProcessor> attach_quick_process_to_channels(AudioProcessingFunction processor, const std::vector<unsigned int> channels)
{
    std::shared_ptr<Buffers::BufferProcessor> quick_processor = nullptr;
    for (unsigned int channel : channels) {
        quick_processor = get_buffer_manager()->attach_quick_process_to_channel(processor, channel);
    }
    return quick_processor;
}

}
