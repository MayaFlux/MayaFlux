#include "Creator.hpp"

#include "MayaFlux/API/Depot.hpp"
#include "MayaFlux/API/Graph.hpp"
#include "MayaFlux/API/Input.hpp"

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Kakshya/Source/SoundFileContainer.hpp"
#include "MayaFlux/Nodes/Network/NodeNetwork.hpp"
#include "MayaFlux/Nodes/Node.hpp"

#include "MayaFlux/Nodes/Input/HIDNode.hpp"
#include "MayaFlux/Nodes/Input/MIDINode.hpp"

namespace MayaFlux {

Creator vega {};

void register_node(const std::shared_ptr<Nodes::Node>& node, const CreationContext& ctx)
{
    auto token = get_node_token(ctx.domain.value());

    if (ctx.channel.has_value()) {
        register_node(node, token, ctx.channel.value());
    } else if (ctx.channels.has_value()) {
        for (uint32_t ch : ctx.channels.value()) {
            register_node(node, token, ch);
        }
    } else if (node->get_channel_mask() != 0) {
        for (uint32_t ch = 0; ch < 32; ++ch) {
            if (node->get_channel_mask() & (1 << ch)) {
                register_node(node, token, ch);
            }
        }
    } else {
        register_node(node, token, 0);
    }
}

void register_network(const std::shared_ptr<Nodes::Network::NodeNetwork>& network, const CreationContext& ctx)
{
    auto token = get_node_token(ctx.domain.value());

    if (token == Nodes::ProcessingToken::AUDIO_RATE) {
        if (
            network->get_output_mode() != Nodes::Network::OutputMode::AUDIO_SINK
            && network->get_output_mode() != Nodes::Network::OutputMode::AUDIO_COMPUTE) {
            MF_WARN(Journal::Component::API,
                Journal::Context::Init,
                "Registering audio network in AUDIO_RATE domain without AUDIO_SINK  or AUDIO_COMPUTE mode. Forcing AUDIO_SINK mode.");
            network->set_output_mode(Nodes::Network::OutputMode::AUDIO_SINK);
        }
        if (ctx.channel.has_value()) {
            network->add_channel_usage(ctx.channel.value());
        } else if (ctx.channels.has_value()) {
            for (uint32_t ch : ctx.channels.value()) {
                network->add_channel_usage(ch);
            }
        }
    } else if (token == Nodes::ProcessingToken::VISUAL_RATE && network->get_output_mode() != Nodes::Network::OutputMode::GRAPHICS_BIND) {
        MF_WARN(Journal::Component::API,
            Journal::Context::Init,
            "Registering visual network in VISUAL_RATE domain without GRAPHICS_BIND output mode. Forcing GRAPHICS_BIND mode.");
        network->set_output_mode(Nodes::Network::OutputMode::GRAPHICS_BIND);
    }

    register_node_network(network, token);
}

void register_buffer(const std::shared_ptr<Buffers::Buffer>& buffer, const CreationContext& ctx)
{
    auto token = get_buffer_token(ctx.domain.value());

    if (auto audio_buffer = std::dynamic_pointer_cast<Buffers::AudioBuffer>(buffer)) {
        if (ctx.channel.has_value()) {
            register_audio_buffer(audio_buffer, ctx.channel.value());
        } else if (ctx.channels.has_value()) {
            clone_buffer_to_channels(audio_buffer, ctx.channels.value(), token);
        }
        return;
    }

    if (auto vk_buffer = std::dynamic_pointer_cast<Buffers::VKBuffer>(buffer)) {
        register_graphics_buffer(vk_buffer, token);
        return;
    }
}

void register_container(const std::shared_ptr<Kakshya::SoundFileContainer>& container, const Domain& domain)
{
    if (auto sound_container = std::dynamic_pointer_cast<Kakshya::SoundFileContainer>(container)) {
        if (domain == Domain::AUDIO) {
            s_last_created_container_buffers.clear();
            s_last_created_container_buffers = hook_sound_container_to_buffers(sound_container);
        }
    }
}

std::shared_ptr<Kakshya::SoundFileContainer> Creator::load_container(const std::string& filepath)
{
    return load_audio_file(filepath);
}

std::vector<std::shared_ptr<Buffers::SoundContainerBuffer>> get_last_created_container_buffers()
{
    return s_last_created_container_buffers;
}

std::shared_ptr<Nodes::Node> operator|(const std::shared_ptr<Nodes::Node>& node, Domain d)
{
    CreationContext ctx(d);
    register_node(node, ctx);
    return node;
}

std::shared_ptr<Nodes::Network::NodeNetwork> operator|(const std::shared_ptr<Nodes::Network::NodeNetwork>& network, Domain d)
{
    CreationContext ctx(d);
    register_network(network, ctx);
    return network;
}

std::shared_ptr<Buffers::Buffer> operator|(const std::shared_ptr<Buffers::Buffer>& buffer, Domain d)
{
    CreationContext ctx(d);
    register_buffer(buffer, ctx);
    return buffer;
}

std::shared_ptr<Buffers::TextureBuffer> Creator::load_buffer(const std::string& filepath)
{
    return load_image_file(filepath);
}

std::shared_ptr<Nodes::Input::HIDNode> Creator::read_hid(
    const Nodes::Input::HIDConfig& config,
    const Core::InputBinding& binding)
{
    auto node = std::make_shared<Nodes::Input::HIDNode>(config);
    register_input_node(node, binding);
    return node;
}

std::shared_ptr<Nodes::Input::MIDINode> Creator::read_midi(
    const Nodes::Input::MIDIConfig& config,
    const Core::InputBinding& binding)
{
    auto node = std::make_shared<Nodes::Input::MIDINode>(config);
    register_input_node(node, binding);
    return node;
}

std::shared_ptr<Nodes::Input::InputNode> Creator::read_input(
    const Nodes::Input::InputConfig& config,
    const Core::InputBinding& binding)
{
    switch (binding.backend) {
    case Core::InputType::HID:
        return read_hid(static_cast<const Nodes::Input::HIDConfig&>(config), binding);
    case Core::InputType::MIDI:
        return read_midi(static_cast<const Nodes::Input::MIDIConfig&>(config), binding);
    default:
        MF_ERROR(Journal::Component::API, Journal::Context::Init,
            "Input type {} not yet implemented",
            static_cast<int>(binding.backend));
        return nullptr;
    }
}

} // namespace MayaFlux
