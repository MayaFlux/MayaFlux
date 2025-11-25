#include "Graph.hpp"

#include "Core.hpp"

#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Core/Engine.hpp"
#include "MayaFlux/Nodes/Network/NodeNetwork.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux {

//-------------------------------------------------------------------------
// Node Graph Management
//-------------------------------------------------------------------------

std::shared_ptr<Nodes::NodeGraphManager> get_node_graph_manager()
{
    return get_context().get_node_graph_manager();
}

void register_audio_node(const std::shared_ptr<Nodes::Node>& node, uint32_t channel)
{
    auto manager = get_node_graph_manager();
    if (channel >= manager->get_channel_count(Nodes::ProcessingToken::AUDIO_RATE)) {
        std::out_of_range err("Channel index out of range for audio node registration");
        std::cerr << err.what() << '\n';
    }
    manager->add_to_root(node, Nodes::ProcessingToken::AUDIO_RATE, channel);
}

void register_audio_node(const std::shared_ptr<Nodes::Node>& node, const std::vector<uint32_t>& channels)
{
    for (const auto& channel : channels) {
        register_audio_node(node, channel);
    }
}

void unregister_audio_node(const std::shared_ptr<Nodes::Node>& node, uint32_t channel)
{
    auto manager = get_node_graph_manager();
    if (channel >= manager->get_channel_count(Nodes::ProcessingToken::AUDIO_RATE)) {
        MF_ERROR(Journal::Component::API, Journal::Context::NodeProcessing,
            "Channel index out of range for audio node registration");
    }
    manager->remove_from_root(node, Nodes::ProcessingToken::AUDIO_RATE, channel);
}

void unregister_audio_node(const std::shared_ptr<Nodes::Node>& node, const std::vector<uint32_t>& channels)
{
    for (const auto& channel : channels) {
        unregister_audio_node(node, channel);
    }
}

void unregister_node(const std::shared_ptr<Nodes::Node>& node, const Nodes::ProcessingToken& token, uint32_t channel)
{
    auto manager = get_node_graph_manager();
    if (channel >= manager->get_channel_count(token)) {
        MF_ERROR(Journal::Component::API, Journal::Context::NodeProcessing,
            "Channel index out of range for audio node registration");
    }
    manager->remove_from_root(node, token, channel);
}

Nodes::RootNode& get_audio_channel_root(uint32_t channel)
{
    return *get_context().get_node_graph_manager()->get_all_root_nodes(Nodes::ProcessingToken::AUDIO_RATE)[channel];
}

void register_node(const std::shared_ptr<Nodes::Node>& node, const Nodes::ProcessingToken& token, uint32_t channel)
{
    get_context().get_node_graph_manager()->add_to_root(node, token, channel);
}

//-------------------------------------------------------------------------
//  Buffer Management
//-------------------------------------------------------------------------

std::shared_ptr<Buffers::BufferManager> get_buffer_manager()
{
    return get_context().get_buffer_manager();
}

void add_processor(const std::shared_ptr<Buffers::BufferProcessor>& processor, const std::shared_ptr<Buffers::Buffer>& buffer, Buffers::ProcessingToken token)
{
    get_buffer_manager()->add_processor(processor, buffer, token);
}

void add_processor(const std::shared_ptr<Buffers::BufferProcessor>& processor, Buffers::ProcessingToken token, uint32_t channel)
{
    get_buffer_manager()->add_processor(processor, token, channel);
}

void add_processor(const std::shared_ptr<Buffers::BufferProcessor>& processor, Buffers::ProcessingToken token)
{
    get_buffer_manager()->add_processor(processor, token);
}

std::shared_ptr<Buffers::BufferProcessingChain> create_processing_chain()
{
    return std::make_shared<Buffers::BufferProcessingChain>();
}

Buffers::RootAudioBuffer& get_root_audio_buffer(uint32_t channel)
{
    return *get_buffer_manager()->get_root_audio_buffer(Buffers::ProcessingToken::AUDIO_BACKEND, channel);
}

void connect_node_to_channel(const std::shared_ptr<Nodes::Node>& node, uint32_t channel_index, float mix, bool clear_before)
{
    auto token = get_buffer_manager()->get_default_audio_token();
    get_buffer_manager()->connect_node_to_channel(node, token, channel_index, mix, clear_before);
}

void connect_node_to_buffer(const std::shared_ptr<Nodes::Node>& node, const std::shared_ptr<Buffers::AudioBuffer>& buffer, float mix, bool clear_before)
{
    get_buffer_manager()->connect_node_to_buffer(node, buffer, mix, clear_before);
}

//-------------------------------------------------------------------------
// Node Network Management
//-------------------------------------------------------------------------

void register_node_network(const std::shared_ptr<Nodes::Network::NodeNetwork>& network, const Nodes::ProcessingToken& token)
{
    get_context().get_node_graph_manager()->add_network(network, token);
}

void unregister_node_network(const std::shared_ptr<Nodes::Network::NodeNetwork>& network, const Nodes::ProcessingToken& token)
{
    get_context().get_node_graph_manager()->remove_network(network, token);
}

//-------------------------------------------------------------------------
// Audio Processing
//-------------------------------------------------------------------------

std::shared_ptr<Buffers::BufferProcessor> attach_quick_process(Buffers::BufferProcessingFunction processor, const std::shared_ptr<Buffers::AudioBuffer>& buffer)
{
    return get_buffer_manager()->attach_quick_process(std::move(processor), buffer, Buffers::ProcessingToken::AUDIO_BACKEND);
}

std::shared_ptr<Buffers::BufferProcessor> attach_quick_process(Buffers::BufferProcessingFunction processor, unsigned int channel_id)
{
    return get_buffer_manager()->attach_quick_process(std::move(processor), Buffers::ProcessingToken::AUDIO_BACKEND, channel_id);
}

void register_audio_buffer(const std::shared_ptr<Buffers::AudioBuffer>& buffer, uint32_t channel)
{
    get_buffer_manager()->add_buffer(buffer, Buffers::ProcessingToken::AUDIO_BACKEND, channel);
}

void unregister_audio_buffer(const std::shared_ptr<Buffers::AudioBuffer>& buffer, uint32_t channel)
{
    get_buffer_manager()->remove_buffer(buffer, Buffers::ProcessingToken::AUDIO_BACKEND, channel);
}

void register_graphics_buffer(const std::shared_ptr<Buffers::VKBuffer>& buffer, Buffers::ProcessingToken token)
{
    get_buffer_manager()->add_buffer(buffer, token);
}

void unregister_graphics_buffer(const std::shared_ptr<Buffers::VKBuffer>& buffer)
{
    get_buffer_manager()->remove_buffer(buffer, Buffers::ProcessingToken::GRAPHICS_BACKEND);
}

void read_from_audio_input(const std::shared_ptr<Buffers::AudioBuffer>& buffer, uint32_t channel)
{
    get_buffer_manager()->register_input_listener(buffer, channel);
}

void detach_from_audio_input(const std::shared_ptr<Buffers::AudioBuffer>& buffer, uint32_t channel)
{
    get_buffer_manager()->unregister_input_listener(buffer, channel);
}

std::shared_ptr<Buffers::AudioBuffer> create_input_listener_buffer(uint32_t channel, bool add_to_output)
{
    std::shared_ptr<Buffers::AudioBuffer> buffer = std::make_shared<Buffers::AudioBuffer>(channel);

    if (add_to_output) {
        register_audio_buffer(buffer, channel);
    }
    read_from_audio_input(buffer, channel);

    return buffer;
}

std::vector<std::shared_ptr<Buffers::AudioBuffer>> clone_buffer_to_channels(const std::shared_ptr<Buffers::AudioBuffer>& buffer,
    const std::vector<uint32_t>& channels)
{
    return get_buffer_manager()->clone_buffer_for_channels(buffer, channels, Buffers::ProcessingToken::AUDIO_BACKEND);
}

std::vector<std::shared_ptr<Buffers::AudioBuffer>> clone_buffer_to_channels(const std::shared_ptr<Buffers::AudioBuffer>& buffer, const std::vector<uint32_t>& channels, const Buffers::ProcessingToken& token)
{
    return get_buffer_manager()->clone_buffer_for_channels(buffer, channels, token);
}

void supply_buffer_to_channel(const std::shared_ptr<Buffers::AudioBuffer>& buffer,
    uint32_t channel, double mix)
{
    auto manager = get_buffer_manager();
    if (channel < manager->get_num_channels(Buffers::ProcessingToken::AUDIO_BACKEND)) {
        manager->supply_buffer_to(buffer, Buffers::ProcessingToken::AUDIO_BACKEND, channel, mix);
    }
}

void supply_buffer_to_channels(const std::shared_ptr<Buffers::AudioBuffer>& buffer,
    const std::vector<uint32_t>& channels,
    double mix)
{
    for (const auto& channel : channels) {
        supply_buffer_to_channel(buffer, channel, mix);
    }
}

void remove_supplied_buffer_from_channel(const std::shared_ptr<Buffers::AudioBuffer>& buffer,
    const uint32_t channel)
{
    auto manager = get_buffer_manager();

    if (channel < manager->get_num_channels(Buffers::ProcessingToken::AUDIO_BACKEND)) {
        manager->remove_supplied_buffer(buffer, Buffers::ProcessingToken::AUDIO_BACKEND, channel);
    }
}

void remove_supplied_buffer_from_channels(const std::shared_ptr<Buffers::AudioBuffer>& buffer,
    const std::vector<uint32_t>& channels)
{
    for (const auto& channel : channels) {
        remove_supplied_buffer_from_channel(buffer, channel);
    }
}

}
