#include "Mint.hpp"

#include "MayaFlux/API/Graph.hpp"
#include "MayaFlux/Buffers/Geometry/ComputeMeshBuffer.hpp"
#include "MayaFlux/Buffers/Geometry/MeshBuffer.hpp"
#include "MayaFlux/Buffers/Network/InstanceNetworkBuffer.hpp"
#include "MayaFlux/Buffers/Network/MeshNetworkBuffer.hpp"
#include "MayaFlux/Buffers/Network/NetworkGeometryBuffer.hpp"
#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Nodes/Graphics/MeshWriterNode.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"
#include "MayaFlux/Nodes/Network/AssemblyNetwork.hpp"
#include "MayaFlux/Nodes/Network/InstanceNetwork.hpp"
#include "MayaFlux/Nodes/Network/MeshNetwork.hpp"

namespace MayaFlux {

namespace {

[[nodiscard]] bool has_target(const StructureConfig::RenderConfig& render)
{
    if (render.target_window) {
        return true;
    }

    MF_ERROR(Journal::Component::API, Journal::Context::Init,
        "mint: RenderConfig::target_window is required, there is no default window");
    return false;
}

[[nodiscard]] std::shared_ptr<Nodes::GpuSync::MeshWriterNode> resolve_writer(
    const std::optional<Kakshya::MeshData>& mesh,
    const StructureConfig::MeshExpression& expression,
    const std::shared_ptr<Nodes::GpuSync::MeshWriterNode>& writer,
    std::string_view context)
{
    const unsigned int source_count = static_cast<unsigned int>(mesh.has_value())
        + static_cast<unsigned int>(static_cast<bool>(expression))
        + static_cast<unsigned int>(writer != nullptr);

    if (source_count != 1) {
        MF_ERROR(Journal::Component::API, Journal::Context::Init,
            "mint: {} requires exactly one mesh, expression or writer source", context);
        return nullptr;
    }

    if (writer) {
        return writer;
    }

    Kakshya::MeshData data = mesh ? *mesh : expression();
    if (!data.is_valid()) {
        MF_ERROR(Journal::Component::API, Journal::Context::Init,
            "mint: {} produced invalid MeshData", context);
        return nullptr;
    }

    auto resolved = std::make_shared<Nodes::GpuSync::MeshWriterNode>(data.vertex_count());
    resolved->set_mesh(data);
    return resolved;
}

void add_extra_textures(
    StructureConfig::RenderConfig& render,
    const std::vector<std::shared_ptr<Core::VKImage>>& textures,
    size_t first)
{
    for (size_t index = first; index < textures.size(); ++index) {
        if (textures[index]) {
            render.additional_textures.emplace_back(
                "texture" + std::to_string(index), textures[index]);
        }
    }
}

void add_all_textures(
    StructureConfig::RenderConfig& render,
    const std::vector<std::shared_ptr<Core::VKImage>>& textures)
{
    if (!textures.empty() && textures.front()) {
        render.additional_textures.emplace_back("diffuseTex", textures.front());
    }
    add_extra_textures(render, textures, 1);
}

void register_network_if_needed(
    const std::shared_ptr<Nodes::Network::NodeNetwork>& network)
{
    if (!get_node_graph_manager()->is_network_registered(
            network, Nodes::ProcessingToken::VISUAL_RATE)) {
        register_node_network(network, Nodes::ProcessingToken::VISUAL_RATE);
    }
}

template <typename Buffer>
void bind_mesh_textures(
    const std::shared_ptr<Buffer>& buffer,
    StructureConfig::RenderConfig& render,
    const std::vector<std::shared_ptr<Core::VKImage>>& textures)
{
    if (!textures.empty() && textures.front()) {
        const std::string binding = render.default_texture_binding.empty()
            ? "diffuseTex"
            : render.default_texture_binding;
        buffer->bind_diffuse_texture(textures.front(), binding);
        render.default_texture_binding = binding;
    }
    add_extra_textures(render, textures, 1);
}

} // namespace

/** @brief Wiring sequences behind Mint. Holds no state. */
struct Mint::Impl {
    [[nodiscard]] std::shared_ptr<Buffers::MeshBuffer> build(
        const StructureConfig::Object& config) const
    {
        if (!has_target(config.render)) {
            return nullptr;
        }

        const unsigned int source_count = static_cast<unsigned int>(config.mesh.has_value())
            + static_cast<unsigned int>(static_cast<bool>(config.expression))
            + static_cast<unsigned int>(config.writer != nullptr);
        if (source_count != 1) {
            MF_ERROR(Journal::Component::API, Journal::Context::Init,
                "mint: Object requires exactly one mesh, expression or writer source");
            return nullptr;
        }

        auto writer = resolve_writer(
            config.mesh, config.expression, config.writer, "Object");
        if (!writer) {
            return nullptr;
        }

        auto buffer = std::make_shared<Buffers::MeshBuffer>(writer);

        StructureConfig::RenderConfig render = config.render;
        bind_mesh_textures(buffer, render, config.textures);
        register_graphics_buffer(buffer);
        buffer->setup_rendering(render);
        return buffer;
    }

    [[nodiscard]] std::shared_ptr<Buffers::MeshNetworkBuffer> build(
        const StructureConfig::Model& config) const
    {
        if (!has_target(config.render)) {
            return nullptr;
        }

        auto network = config.network
            ? config.network
            : std::make_shared<Nodes::Network::MeshNetwork>();

        for (size_t index = 0; index < config.components.size(); ++index) {
            const auto& component = config.components[index];
            auto writer = resolve_writer(
                component.mesh, component.expression, component.writer, "Model component");
            if (!writer) {
                return nullptr;
            }

            const uint32_t slot_index = network->add_slot(
                "component_" + std::to_string(index), writer, component.parent);
            network->get_slot(slot_index).local_transform = component.local_transform;
        }

        if (network->slot_count() == 0) {
            MF_ERROR(Journal::Component::API, Journal::Context::Init,
                "mint: Model requires at least one mesh component");
            return nullptr;
        }

        register_network_if_needed(network);
        auto buffer = std::make_shared<Buffers::MeshNetworkBuffer>(
            network, config.over_allocate_factor);
        StructureConfig::RenderConfig render = config.render;
        bind_mesh_textures(buffer, render, config.textures);
        register_graphics_buffer(buffer);
        buffer->setup_rendering(render);
        return buffer;
    }

    [[nodiscard]] std::shared_ptr<Buffers::InstanceNetworkBuffer> build(
        const StructureConfig::Instances& config) const
    {
        if (!has_target(config.render)) {
            return nullptr;
        }

        auto network = config.network
            ? config.network
            : std::make_shared<Nodes::Network::InstanceNetwork>();

        if (!config.transforms.empty()) {
            auto writer = resolve_writer(
                config.prototype_mesh,
                config.prototype_expression,
                config.prototype_writer,
                "Instances prototype");
            if (!writer) {
                return nullptr;
            }

            for (size_t index = 0; index < config.transforms.size(); ++index) {
                const uint32_t slot_index = network->add_slot(
                    "instance_" + std::to_string(index), writer);
                network->get_slot(slot_index).transform = config.transforms[index];
            }
        }

        if (network->slot_count() == 0) {
            MF_ERROR(Journal::Component::API, Journal::Context::Init,
                "mint: Instances requires an existing populated network or transforms");
            return nullptr;
        }

        register_network_if_needed(network);
        auto buffer = std::make_shared<Buffers::InstanceNetworkBuffer>(
            network, config.over_allocate_factor);
        StructureConfig::RenderConfig render = config.render;
        add_all_textures(render, config.textures);
        register_graphics_buffer(buffer);
        buffer->setup_rendering(render);
        return buffer;
    }

    [[nodiscard]] std::shared_ptr<Buffers::NetworkGeometryBuffer> build(
        const StructureConfig::Assembly& config) const
    {
        if (!has_target(config.render)) {
            return nullptr;
        }

        auto network = config.network
            ? config.network
            : std::make_shared<Nodes::Network::AssemblyNetwork>();

        for (const auto& source : config.sources) {
            network->add_geometry(source);
        }

        if (network->get_node_count() == 0) {
            MF_ERROR(Journal::Component::API, Journal::Context::Init,
                "mint: Assembly requires at least one geometry source");
            return nullptr;
        }

        register_network_if_needed(network);
        auto buffer = std::make_shared<Buffers::NetworkGeometryBuffer>(network);
        StructureConfig::RenderConfig render = config.render;
        render.triangulate = true;
        add_all_textures(render, config.textures);
        register_graphics_buffer(buffer);
        buffer->setup_rendering(render);
        buffer->get_render_processor()->set_mill_spec(config.mill);
        return buffer;
    }

    [[nodiscard]] std::shared_ptr<Buffers::ComputeMeshBuffer> build(
        const StructureConfig::Isosurface& config) const
    {
        if (!has_target(config.render)) {
            return nullptr;
        }

        const bool has_buffer = config.buffer != nullptr;
        const bool has_config = config.config.has_value();
        if (static_cast<unsigned int>(has_buffer)
                + static_cast<unsigned int>(has_config)
            != 1) {
            MF_ERROR(Journal::Component::API, Journal::Context::Init,
                "mint: Isosurface requires exactly one buffer or config");
            return nullptr;
        }

        auto buffer = has_buffer
            ? config.buffer
            : std::make_shared<Buffers::ComputeMeshBuffer>(*config.config);

        StructureConfig::RenderConfig render = config.render;
        if (!config.textures.empty() && config.textures.front()) {
            const std::string binding = render.default_texture_binding.empty()
                ? "diffuseTex"
                : render.default_texture_binding;
            buffer->set_texture(config.textures.front(), binding);
            render.default_texture_binding = binding;
        }
        add_extra_textures(render, config.textures, 1);
        register_graphics_buffer(buffer);
        buffer->setup_rendering(render);
        return buffer;
    }
};

Mint::Mint()
    : m_impl(std::make_unique<Impl>())
{
}

Mint::~Mint() = default;

Mint::Mint(Mint&&) noexcept = default;

Mint& Mint::operator=(Mint&&) noexcept = default;

std::shared_ptr<Buffers::MeshBuffer> Mint::build(
    const StructureConfig::Object& config)
{
    return m_impl->build(config);
}

std::shared_ptr<Buffers::MeshNetworkBuffer> Mint::build(
    const StructureConfig::Model& config)
{
    return m_impl->build(config);
}

std::shared_ptr<Buffers::InstanceNetworkBuffer> Mint::build(
    const StructureConfig::Instances& config)
{
    return m_impl->build(config);
}

std::shared_ptr<Buffers::NetworkGeometryBuffer> Mint::build(
    const StructureConfig::Assembly& config)
{
    return m_impl->build(config);
}

std::shared_ptr<Buffers::ComputeMeshBuffer> Mint::build(
    const StructureConfig::Isosurface& config)
{
    return m_impl->build(config);
}

} // namespace MayaFlux
