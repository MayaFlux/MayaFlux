#include "Graph.hpp"
#include "Core.hpp"

#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Core/Engine.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"

#include "Proxy/Creator.hpp"

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
    auto manager = get_node_graph_manager();
    if (channel >= manager->get_channel_count(Nodes::ProcessingToken::AUDIO_RATE)) {
        std::out_of_range err("Channel index out of range for audio node registration");
        std::cerr << err.what() << std::endl;
    }
    manager->add_to_root(node, Nodes::ProcessingToken::AUDIO_RATE, channel);
}

void unregister_audio_node(std::shared_ptr<Nodes::Node> node, unsigned int channel)
{
    for (const auto& channel : channels) {
        register_audio_node(node, channel);
    }
}

void unregister_audio_node(std::shared_ptr<Nodes::Node> node, u_int32_t channel)
{
    auto manager = get_node_graph_manager();
    if (channel >= manager->get_channel_count(Nodes::ProcessingToken::AUDIO_RATE)) {
        std::out_of_range err("Channel index out of range for audio node registration");
        std::cerr << err.what() << std::endl;
    }
    manager->remove_from_root(node, Nodes::ProcessingToken::AUDIO_RATE, channel);
}

void unregister_audio_node(std::shared_ptr<Nodes::Node> node, std::vector<u_int32_t> channels)
{
    for (const auto& channel : channels) {
        unregister_audio_node(node, channel);
    }
}

Nodes::RootNode& get_audio_channel_root(u_int32_t channel)
{
    return *get_context().get_node_graph_manager()->get_all_root_nodes(Nodes::ProcessingToken::AUDIO_RATE)[channel];
}

void register_all_nodes()
{
    ContextAppliers::set_node_context_applier(
        [](std::shared_ptr<Nodes::Node> node, const CreationContext& context) {
            if (context.domain && context.channel) {
                auto token = get_node_token(*context.domain);
                get_node_graph_manager()->add_to_root(node, token, context.channel.value());
            }
        });
}

//-------------------------------------------------------------------------
//  Buffer Management
//-------------------------------------------------------------------------

std::shared_ptr<Buffers::BufferManager> get_buffer_manager()
{
    return get_context().get_buffer_manager();
}

Buffers::RootAudioBuffer& get_root_audio_buffer(u_int32_t channel)
{
    return *get_buffer_manager()->get_root_audio_buffer(Buffers::ProcessingToken::AUDIO_BACKEND, channel);
}

void connect_node_to_channel(std::shared_ptr<Nodes::Node> node, u_int32_t channel_index, float mix, bool clear_before)
{
    auto token = get_buffer_manager()->get_default_processing_token();
    get_buffer_manager()->connect_node_to_channel(node, token, 0, mix, clear_before);
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

std::shared_ptr<Buffers::BufferProcessor> attach_quick_process_to_audio_channel(AudioProcessingFunction processor, unsigned int channel_id)
{
    return get_buffer_manager()->attach_quick_process_to_channel(processor, Buffers::ProcessingToken::AUDIO_BACKEND, channel_id);
}

std::shared_ptr<Buffers::BufferProcessor> attach_quick_process_to_audio_channels(AudioProcessingFunction processor, const std::vector<unsigned int> channels)
{
    std::shared_ptr<Buffers::BufferProcessor> quick_processor = nullptr;
    quick_processor = get_buffer_manager()->attach_quick_process_to_token(processor, Buffers::ProcessingToken::AUDIO_BACKEND);
    return quick_processor;
}

void register_audio_buffer(std::shared_ptr<Buffers::AudioBuffer> buffer, u_int32_t channel)
{
    get_buffer_manager()->add_audio_buffer(buffer, Buffers::ProcessingToken::AUDIO_BACKEND, channel);
}

void unregister_audio_buffer(std::shared_ptr<Buffers::AudioBuffer> buffer, u_int32_t channel)
{
    get_buffer_manager()->remove_audio_buffer(buffer, Buffers::ProcessingToken::AUDIO_BACKEND, channel);
}

void read_from_audio_input(std::shared_ptr<Buffers::AudioBuffer> buffer, u_int32_t channel)
{
    get_buffer_manager()->register_input_listener(buffer, channel);
}

void detach_from_audio_input(std::shared_ptr<Buffers::AudioBuffer> buffer, u_int32_t channel)
{
    get_buffer_manager()->unregister_input_listener(buffer, channel);
}

std::shared_ptr<Buffers::AudioBuffer> create_input_listener_buffer(u_int32_t channel, bool add_to_output)
{
    std::shared_ptr<Buffers::AudioBuffer> buffer = std::make_shared<Buffers::AudioBuffer>(channel);

    if (add_to_output) {
        register_audio_buffer(buffer, channel);
    }
    read_from_audio_input(buffer, channel);

    return buffer;
}

void clone_buffer_to_channels(std::shared_ptr<Buffers::AudioBuffer> buffer,
    const std::vector<u_int32_t>& channels)
{
    get_buffer_manager()->clone_buffer_for_channels(buffer, channels, Buffers::ProcessingToken::AUDIO_BACKEND);
}

void supply_buffer_to_channel(std::shared_ptr<Buffers::AudioBuffer> buffer,
    u_int32_t channel, double mix)
{
    auto manager = get_buffer_manager();
    if (channel < manager->get_num_channels(Buffers::ProcessingToken::AUDIO_BACKEND)) {
        manager->supply_buffer_to(buffer, Buffers::ProcessingToken::AUDIO_BACKEND, channel, mix);
    }
}

void supply_buffer_to_channels(std::shared_ptr<Buffers::AudioBuffer> buffer,
    const std::vector<u_int32_t>& channels,
    double mix)
{
    for (const auto& channel : channels) {
        supply_buffer_to_channel(buffer, channel, mix);
    }
}

void remove_supplied_buffer_from_channel(std::shared_ptr<Buffers::AudioBuffer> buffer,
    const u_int32_t channel)
{
    auto manager = get_buffer_manager();

    if (channel < manager->get_num_channels(Buffers::ProcessingToken::AUDIO_BACKEND)) {
        manager->remove_supplied_buffer(buffer, Buffers::ProcessingToken::AUDIO_BACKEND, channel);
    }
}

void remove_supplied_buffer_from_channels(std::shared_ptr<Buffers::AudioBuffer> buffer,
    const std::vector<u_int32_t>& channels)
{
    for (const auto& channel : channels) {
        remove_supplied_buffer_from_channel(buffer, channel);
    }
}

void register_all_buffers()
{
    ContextAppliers::set_buffer_context_applier(
        [](std::shared_ptr<Buffers::Buffer> buffer, const CreationContext& context) {
            if (context.domain && context.channel) {
                auto token = get_buffer_token(*context.domain);
                if (auto t_buffer = dynamic_pointer_cast<Buffers::AudioBuffer>(buffer)) {
                    get_buffer_manager()->add_audio_buffer(t_buffer, token, context.channel.value());
                }
            }
        });
}

}
