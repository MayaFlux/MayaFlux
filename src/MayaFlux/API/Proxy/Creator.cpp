#include "Creator.hpp"

#include "MayaFlux/API/Depot.hpp"
#include "MayaFlux/API/Graph.hpp"
#include "MayaFlux/API/Input.hpp"

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Buffers/Geometry/MeshBuffer.hpp"
#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/IO/IOManager.hpp"
#include "MayaFlux/Kakshya/Source/SoundFileContainer.hpp"
#include "MayaFlux/Nodes/Network/NodeNetwork.hpp"

#include "MayaFlux/Nodes/Input/HIDNode.hpp"
#include "MayaFlux/Nodes/Input/MIDINode.hpp"
#include "MayaFlux/Nodes/Input/OSCNode.hpp"

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
            register_audio_buffer(audio_buffer, ctx.channels.value()[0]);
            for (size_t i = 1; i < ctx.channels.value().size(); ++i) {
                clone_buffer_to_channels(audio_buffer, { static_cast<uint32_t>(i) }, token);
            }
        } else {
            register_audio_buffer(audio_buffer, audio_buffer->get_channel_id());
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
            (void)get_io_manager()->hook_audio_container_to_buffers(sound_container);
        }
    }
}

std::shared_ptr<Kakshya::SoundFileContainer> Creator::load_sound_container(const std::string& filepath)
{
    return get_io_manager()->load_audio(filepath);
}

MeshGroupHandle::MeshGroupHandle(
    std::vector<std::shared_ptr<Buffers::MeshBuffer>> buffers)
    : m_buffers(std::move(buffers))
{
}

MeshGroupHandle::~MeshGroupHandle() = default;

MeshGroupHandle& MeshGroupHandle::operator|(Domain d)
{
    CreationContext ctx(d);
    for (const auto& buf : m_buffers) {
        register_buffer(
            std::static_pointer_cast<Buffers::Buffer>(buf), ctx);
    }
    return *this;
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

std::shared_ptr<Buffers::TextureBuffer> Creator::load_image_buffer(const std::string& filepath)
{
    return get_io_manager()->load_image(filepath);
}

std::vector<std::shared_ptr<Buffers::MeshBuffer>> Creator::load_mesh_buffers(const std::string& filepath)
{
    return get_io_manager()->load_mesh(filepath);
}

std::shared_ptr<Nodes::Network::MeshNetwork>
Creator::load_mesh_network(const std::string& filepath, IO::TextureResolver resolver)
{
    return get_io_manager()->load_mesh_network(filepath, std::move(resolver));
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

std::shared_ptr<Nodes::Input::OSCNode> Creator::read_osc(
    const Nodes::Input::OSCConfig& config,
    const Core::InputBinding& binding)
{
    auto node = std::make_shared<Nodes::Input::OSCNode>(config);
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
    case Core::InputType::OSC:
        return read_osc(static_cast<const Nodes::Input::OSCConfig&>(config), binding);
    default:
        MF_ERROR(Journal::Component::API, Journal::Context::Init,
            "Input type {} not yet implemented",
            static_cast<int>(binding.backend));
        return nullptr;
    }
}

} // namespace MayaFlux
