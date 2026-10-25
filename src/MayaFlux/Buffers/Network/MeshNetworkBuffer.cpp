#include "MeshNetworkBuffer.hpp"

#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Nodes/Network/MeshNetwork.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Portal/Graphics/GraphicsUtils.hpp"

namespace MayaFlux::Buffers {

// =============================================================================
// Construction
// =============================================================================

MeshNetworkBuffer::MeshNetworkBuffer(
    std::shared_ptr<Nodes::Network::MeshNetwork> network,
    float over_allocate_factor)
    : VKBuffer(
          estimate_vertex_bytes(network, over_allocate_factor),
          Usage::VERTEX,
          Kakshya::DataModality::VERTEX_POSITIONS_3D)
    , m_network(std::move(network))
{
    if (!m_network) {
        error<std::invalid_argument>(
            Journal::Component::Buffers, Journal::Context::Init,
            std::source_location::current(),
            "MeshNetworkBuffer: null MeshNetwork");
    }

    RenderConfig defaults;
    defaults.topology = Portal::Graphics::PrimitiveTopology::TRIANGLE_LIST;
    set_default_render_config(defaults);
    set_needs_depth_attachment(true);

    m_render_config = defaults;

    MF_INFO(Journal::Component::Buffers, Journal::Context::Init,
        "MeshNetworkBuffer: {} slots, {} bytes estimated",
        m_network->slot_count(), get_size_bytes());
}

// =============================================================================
// setup_processors
// =============================================================================

void MeshNetworkBuffer::setup_processors(ProcessingToken token)
{
    m_processor = std::make_shared<MeshNetworkProcessor>(m_network);
    m_processor->set_processing_token(token);
    set_default_processor(m_processor);

    auto chain = get_processing_chain();
    if (!chain) {
        chain = std::make_shared<BufferProcessingChain>();
        set_processing_chain(chain);
    }
    chain->set_preferred_token(token);

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::Init,
        "MeshNetworkBuffer::setup_processors with token {}",
        static_cast<int>(token));
}

// =============================================================================
// setup_rendering
// =============================================================================

void MeshNetworkBuffer::setup_rendering(const RenderConfig& config)
{
    if (!config.vertex_shader.empty())
        m_render_config.vertex_shader = config.vertex_shader;
    if (!config.fragment_shader.empty())
        m_render_config.fragment_shader = config.fragment_shader;
    if (!config.default_texture_binding.empty()) {
        m_render_config.default_texture_binding = config.default_texture_binding;
        m_diffuse_binding = config.default_texture_binding;
    }

    m_render_config.target_window = config.target_window;
    m_render_config.topology = Portal::Graphics::PrimitiveTopology::TRIANGLE_LIST;
    m_render_config.polygon_mode = config.polygon_mode;
    m_render_config.cull_mode = config.cull_mode;
    m_render_config.triangulate = config.triangulate;

    m_render_config.triangulate |= Portal::Graphics::needs_triangulation(
        m_render_config.topology, m_render_config.polygon_mode);
    const bool milled = m_render_config.triangulate;
    m_processor->set_milling_slot_attribute(milled);
    set_prefer_semantic_layout(milled);

    auto vertex_layout = Kakshya::VertexLayout::for_meshes(
        sizeof(Kakshya::MeshVertex) + (milled ? 2 * sizeof(float) : 0));
    if (milled) {
        vertex_layout.attributes.push_back({
            .component_modality = Kakshya::DataModality::SCALAR_F32,
            .offset_in_vertex = static_cast<uint32_t>(sizeof(Kakshya::MeshVertex)),
            .name = "slot_index_low"
        });
        vertex_layout.attributes.push_back({
            .component_modality = Kakshya::DataModality::SCALAR_F32,
            .offset_in_vertex = static_cast<uint32_t>(sizeof(Kakshya::MeshVertex) + sizeof(float)),
            .name = "slot_index_high"
        });
    }
    for (const auto& slot : m_network->slots()) {
        if (slot.node)
            vertex_layout.vertex_count += static_cast<uint32_t>(slot.node->get_mesh_vertex_count());
    }
    set_vertex_layout(vertex_layout);

    for (const auto& [name, tex] : config.additional_textures)
        m_render_config.additional_textures.emplace_back(name, tex);

    const auto slot_count = static_cast<uint32_t>(m_network->slot_count());

    const bool has_slot_textures = std::ranges::any_of(
        m_network->slots(),
        [](const auto& slot) { return slot.diffuse_texture != nullptr; });

    const bool textured = m_diffuse_texture != nullptr
        || !m_render_config.default_texture_binding.empty()
        || has_slot_textures;

    if (textured && m_render_config.default_texture_binding.empty())
        m_render_config.default_texture_binding = m_diffuse_binding;

    if (m_render_config.vertex_shader.empty())
        m_render_config.vertex_shader = "mesh_network.vert.spv";

    if (m_render_config.fragment_shader.empty()) {
        m_render_config.fragment_shader = textured
            ? "mesh_network_textured.frag.spv" : "mesh_network.frag.spv";
    }

    ShaderConfig sc { m_render_config.vertex_shader };

    sc.bindings["modelMatrices"] = ShaderBinding(0, 1, vk::DescriptorType::eStorageBuffer);
    sc.bindings["slotIndices"] = ShaderBinding(0, 2, vk::DescriptorType::eStorageBuffer);
    if (has_slot_textures) {
        sc.bindings["diffuseTex"] = ShaderBinding(
            0, 3, vk::DescriptorType::eCombinedImageSampler, slot_count);
    } else if (textured && !m_render_config.default_texture_binding.empty()) {
        sc.bindings[m_render_config.default_texture_binding] = ShaderBinding(
            0, 3, vk::DescriptorType::eCombinedImageSampler);
    }

    uint32_t binding_idx = 4;
    for (const auto& [name, _] : m_render_config.additional_textures)
        sc.bindings[name] = ShaderBinding(0, binding_idx++, vk::DescriptorType::eCombinedImageSampler);

    apply_render_config(m_render_config, sc);

    auto self = std::dynamic_pointer_cast<MeshNetworkBuffer>(shared_from_this());
    m_render_processor->set_buffer_vertex_layout(self, vertex_layout);

    if (m_render_config.triangulate) {
        auto mill_spec = m_render_processor->get_mill_spec();
        mill_spec.use_vertex_extent = false;
        m_render_processor->set_mill_spec(mill_spec);
    }

    if (has_slot_textures) {
        const auto& slots = m_network->slots();
        const auto& order = m_network->sorted_indices();
        for (uint32_t i = 0; i < static_cast<uint32_t>(order.size()); ++i) {
            const auto& slot = slots[order[i]];
            if (slot.diffuse_texture)
                m_render_processor->bind_texture(3 + i, slot.diffuse_texture);
        }
    } else if (m_diffuse_texture && !m_render_config.default_texture_binding.empty()) {
        m_render_processor->bind_texture(m_render_config.default_texture_binding, m_diffuse_texture);
    }

    for (const auto& [name, tex] : m_render_config.additional_textures)
        m_render_processor->bind_texture(name, tex);

    get_processing_chain()->add_final_processor(
        m_render_processor,
        shared_from_this());

    MF_INFO(Journal::Component::Buffers, Journal::Context::Init,
        "MeshNetworkBuffer::setup_rendering: vert={} frag={} textured={}",
        m_render_config.vertex_shader,
        m_render_config.fragment_shader,
        textured);
}

// =============================================================================
// bind_diffuse_texture
// =============================================================================

void MeshNetworkBuffer::bind_diffuse_texture(
    std::shared_ptr<Core::VKImage> image,
    std::string_view binding_name)
{
    m_diffuse_texture = std::move(image);
    m_diffuse_binding = std::string(binding_name);

    if (m_render_processor) {
        if (m_render_config.default_texture_binding == m_diffuse_binding) {
            m_render_processor->bind_texture(m_diffuse_binding, m_diffuse_texture);
        } else {
            MF_WARN(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "MeshNetworkBuffer::bind_diffuse_texture: pipeline compiled without "
                "binding '{}' — call before setup_rendering() for correct results",
                m_diffuse_binding);
        }
    }
}

// =============================================================================
// Private helpers
// =============================================================================

size_t MeshNetworkBuffer::estimate_vertex_bytes(
    const std::shared_ptr<Nodes::Network::MeshNetwork>& network,
    float over_allocate_factor)
{
    constexpr size_t k_min = 4096;
    if (!network || network->slot_count() == 0)
        return k_min;

    size_t total = 0;
    for (const auto& slot : network->slots()) {
        if (slot.node)
            total += slot.node->get_mesh_vertex_count() * sizeof(Kakshya::MeshVertex);
    }

    if (total == 0)
        return k_min;

    return static_cast<size_t>(static_cast<float>(total) * over_allocate_factor);
}

} // namespace MayaFlux::Buffers
