#include "MeshNetworkBuffer.hpp"

#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Buffers/Shaders/SkinProcessor.hpp"
#include "MayaFlux/Kinesis/Polyhedral/AffineBlend.hpp"
#include "MayaFlux/Nodes/Network/MeshNetwork.hpp"
#include "MayaFlux/Nodes/Network/Operators/GpuFieldOperator.hpp"
#include "MayaFlux/Nodes/Network/Operators/OperatorChain.hpp"

#include "SpatialFieldWiring.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Portal/Graphics/GraphicsUtils.hpp"
#include "MayaFlux/Portal/Graphics/TextureLoom.hpp"

namespace MayaFlux::Buffers {

// =============================================================================
// Construction
// =============================================================================

MeshNetworkBuffer::MeshNetworkBuffer(
    std::shared_ptr<Nodes::Network::MeshNetwork> network,
    float over_allocate_factor)
    : StateFieldBuffer(
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
    m_render_config.target_images = config.target_images;
    m_render_config.depth_test = config.depth_test;
    m_render_config.depth_write = config.depth_write;
    m_render_config.view_pinned = config.view_pinned;
    m_render_config.draw_band = config.draw_band;
    m_render_config.scissor = config.scissor;
    m_render_config.draw_priority = config.draw_priority;
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

    wire_skin_passes(vertex_layout.stride_bytes);
    wire_field_pass(vertex_layout.stride_bytes);

    for (const auto& entry : config.additional_textures)
        m_render_config.additional_textures.push_back(entry);

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
    for (const auto& entry : m_render_config.additional_textures)
        sc.bindings[entry.name] = ShaderBinding(0, binding_idx++, vk::DescriptorType::eCombinedImageSampler);

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
                m_render_processor->bind_texture({ .image = slot.diffuse_texture, .binding = 3 + i });
        }
    } else if (m_diffuse_texture && !m_render_config.default_texture_binding.empty()) {
        m_render_processor->bind_texture({ .name = m_render_config.default_texture_binding, .image = m_diffuse_texture });
    }

    for (const auto& entry : m_render_config.additional_textures)
        m_render_processor->bind_texture(entry);

    get_processing_chain()->add_final_processor(
        m_render_processor,
        shared_from_this());

    MF_INFO(Journal::Component::Buffers, Journal::Context::Init,
        "MeshNetworkBuffer::setup_rendering: vert={} frag={} textured={}",
        m_render_config.vertex_shader,
        m_render_config.fragment_shader,
        textured);
}

bool MeshNetworkBuffer::set_slot_palette(uint32_t slot, std::vector<glm::mat4> palette)
{
    const auto entry = std::ranges::find(m_skins, slot, &SkinEntry::slot);
    return entry != m_skins.end() && entry->processor && entry->processor->set_palette(std::move(palette));
}

std::vector<glm::mat4> MeshNetworkBuffer::get_slot_palette(uint32_t slot) const
{
    const auto entry = std::ranges::find(m_skins, slot, &SkinEntry::slot);
    return entry != m_skins.end() && entry->processor ? entry->processor->get_palette() : std::vector<glm::mat4> {};
}

void MeshNetworkBuffer::wire_skin_passes(size_t record_bytes)
{
    if (record_bytes < sizeof(Kakshya::MeshVertex)) {
        if (std::ranges::any_of(m_network->slots(), [](const auto& slot) { return slot.skin || slot.morphs; })) {
            MF_ERROR(Journal::Component::Buffers, Journal::Context::Init,
                "MeshNetworkBuffer: records of {} bytes are smaller than a MeshVertex, so no skin pass is wired", record_bytes);
        }
        return;
    }

    for (const auto& slot : m_network->slots()) {
        if (!slot.skin && !slot.morphs) {
            continue;
        }
        if (!slot.node) {
            MF_ERROR(Journal::Component::Buffers, Journal::Context::Init,
                "MeshNetworkBuffer: slot {} has no geometry, so it is not skinned", slot.index);
            continue;
        }

        const auto& vertices = slot.node->get_mesh_vertices();
        const std::span<const uint8_t> rest(
            reinterpret_cast<const uint8_t*>(vertices.data()), vertices.size() * sizeof(Kakshya::MeshVertex));

        SkinEntry entry { .slot = slot.index, .processor = nullptr };
        try {
            entry.processor = std::make_shared<SkinProcessor>(slot.skin, rest, "slot" + std::to_string(entry.slot), slot.morphs);
        } catch (const std::invalid_argument& e) {
            MF_ERROR(Journal::Component::Buffers, Journal::Context::Init,
                "MeshNetworkBuffer: the skin or morph targets of slot {} do not fit its mesh, so it is not deformed: {}", entry.slot, e.what());
            continue;
        }

        bool follows_joints = slot.skin && !slot.joints.empty();
        if (follows_joints) {
            const size_t slots = m_network->slot_count();
            if (slot.joints.size() != slot.skin->joint_count()
                || std::ranges::any_of(slot.joints, [slots](uint32_t joint) { return joint >= slots; })) {
                MF_ERROR(Journal::Component::Buffers, Journal::Context::Init,
                    "MeshNetworkBuffer: the joints of slot {} do not match its skin, so its palette is the caller's", entry.slot);
                follows_joints = false;
            }
        }

        if (follows_joints || slot.morphs) {
            entry.processor->set_palette_source(
                [weak = std::weak_ptr<Nodes::Network::MeshNetwork>(m_network), index = entry.slot, follows_joints,
                    processor = entry.processor.get(), weights = std::vector<float> {}]() mutable -> std::vector<glm::mat4> {
                    const auto network = weak.lock();
                    if (!network) {
                        return {};
                    }
                    const auto& all = network->slots();
                    const auto& deformed = all[index];
                    if (deformed.morph_weights != weights && processor->set_morph_weights(deformed.morph_weights)) {
                        weights = deformed.morph_weights;
                    }
                    if (!follows_joints) {
                        return {};
                    }
                    std::vector<glm::mat4> joints;
                    joints.reserve(deformed.joints.size());
                    for (const uint32_t joint : deformed.joints) {
                        joints.push_back(all[joint].world_transform);
                    }
                    return Kinesis::relative_transforms(deformed.world_transform, joints, deformed.skin->inverse_bind);
                });
        }

        entry.processor->set_first_vertex_source(
            [weak = std::weak_ptr<Nodes::Network::MeshNetwork>(m_network), slot = entry.slot]() -> uint32_t {
                const auto network = weak.lock();
                if (!network) {
                    return 0;
                }
                const auto& all = network->slots();
                uint32_t first = 0;
                for (const uint32_t index : network->sorted_indices()) {
                    if (index == slot) {
                        break;
                    }
                    if (all[index].node) {
                        first += static_cast<uint32_t>(all[index].node->get_mesh_vertex_count());
                    }
                }
                return first;
            });

        get_processing_chain()->add_processor(entry.processor, shared_from_this());
        m_skins.push_back(std::move(entry));
    }

    if (!m_skins.empty()) {
        m_processor->m_geometry_observer = [this](uint32_t slot, const std::vector<Kakshya::MeshVertex>& vertices) {
            const auto entry = std::ranges::find(m_skins, slot, &SkinEntry::slot);
            if (entry != m_skins.end()) {
                entry->processor->set_rest({ reinterpret_cast<const uint8_t*>(vertices.data()),
                    vertices.size() * sizeof(Kakshya::MeshVertex) });
            }
        };

        MF_INFO(Journal::Component::Buffers, Journal::Context::Init,
            "MeshNetworkBuffer: {} skin passes wired", m_skins.size());
    }
}

void MeshNetworkBuffer::wire_field_pass(size_t record_bytes)
{
    const auto chain = m_network->get_operator_chain();
    if (!chain)
        return;

    std::shared_ptr<Nodes::Network::GpuFieldOperator> field_op;
    for (const auto& op : chain->operators()) {
        field_op = std::dynamic_pointer_cast<Nodes::Network::GpuFieldOperator>(op);
        if (field_op)
            break;
    }
    if (!field_op)
        return;

    if (field_op->get_layout().stride_bytes != record_bytes) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::Init,
            "MeshNetworkBuffer: the field operator's layout is {} bytes per vertex, the buffer's "
            "records are {}; no field pass is wired",
            field_op->get_layout().stride_bytes, record_bytes);
        return;
    }

    if (wants_spatial_stages(field_op->get_field_config())) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::Init,
            "MeshNetworkBuffer: the hash, claim, density and population stages are not wired for a "
            "MeshNetwork; only the field pass is");
    }

    wire_field_stages(
        std::dynamic_pointer_cast<StateFieldBuffer>(shared_from_this()),
        field_op, std::nullopt, {}, nullptr);

    MF_INFO(Journal::Component::Buffers, Journal::Context::Init,
        "MeshNetworkBuffer: field pass wired, {} bound fields, {} bytes per vertex",
        field_op->binding_count(), record_bytes);
}

MeshNetworkBuffer::~MeshNetworkBuffer()
{
    Portal::Graphics::get_texture_manager().release_image(m_diffuse_texture);
}

// =============================================================================
// bind_diffuse_texture
// =============================================================================

void MeshNetworkBuffer::bind_diffuse_texture(
    std::shared_ptr<Core::VKImage> image,
    std::string_view binding_name)
{
    auto& loom = Portal::Graphics::get_texture_manager();
    loom.retain_image(image);
    loom.release_image(m_diffuse_texture);
    m_diffuse_texture = std::move(image);
    m_diffuse_binding = std::string(binding_name);

    if (m_render_processor) {
        if (m_render_config.default_texture_binding == m_diffuse_binding) {
            m_render_processor->bind_texture({ .name = m_diffuse_binding, .image = m_diffuse_texture });
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
