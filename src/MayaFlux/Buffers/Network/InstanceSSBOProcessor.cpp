#include "InstanceSSBOProcessor.hpp"

#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"

#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Buffers/Shaders/SkinProcessor.hpp"
#include "MayaFlux/Nodes/Network/InstanceNetwork.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

InstanceSSBOProcessor::InstanceSSBOProcessor(
    std::shared_ptr<Nodes::Network::InstanceNetwork> network)
    : m_network(std::move(network))
{
}

// =============================================================================
// VKBufferProcessor interface
// =============================================================================

void InstanceSSBOProcessor::on_attach(const std::shared_ptr<Buffer>& buffer)
{
    if (!m_network || m_network->slot_count() == 0) {
        MF_WARN(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "InstanceSSBOProcessor attached to empty network");
        return;
    }

    auto vk_buf = std::dynamic_pointer_cast<VKBuffer>(buffer);
    if (!vk_buf) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "InstanceSSBOProcessor: attached buffer is not a VKBuffer");
        return;
    }

    ensure_initialized(vk_buf);

    const size_t slot_count = m_network->slot_count();

    m_vertex_staging = create_staging_buffer(vk_buf->get_size_bytes());

    const size_t ssbo_bytes = slot_count * sizeof(glm::mat4);
    m_transform_arena = std::make_shared<Portal::Graphics::SlotArena>();
    m_transform_arena->initialize(ssbo_bytes, Portal::Graphics::BufferUsageHint::HOST_STORAGE, 3);

    m_payload_arena = std::make_shared<Portal::Graphics::SlotArena>();
    m_payload_arena->initialize(slot_count * sizeof(InstancePayload), Portal::Graphics::BufferUsageHint::HOST_STORAGE, 3);

    upload_template(vk_buf);
    upload_deformation_template();
    upload_transforms(vk_buf);
    push_ssbo_binding(vk_buf);

    MF_INFO(Journal::Component::Buffers, Journal::Context::Init,
        "InstanceSSBOProcessor: {} slots, {} bytes template, {} bytes SSBO",
        slot_count, vk_buf->get_size_bytes(), ssbo_bytes);
}

void InstanceSSBOProcessor::on_detach(const std::shared_ptr<Buffer>&)
{
    m_vertex_staging.reset();
    if (m_transform_arena) {
        m_transform_arena->release();
    }
    m_transform_arena.reset();
    if (m_payload_arena) {
        m_payload_arena->release();
    }
    m_payload_arena.reset();
    for (auto* stream : { &m_influences, &m_deltas, &m_palettes, &m_weights }) {
        if (stream->arena) {
            stream->arena->release();
        }
        stream->arena.reset();
    }
    m_index_buffer.reset();
    m_index_staging.reset();
}

void InstanceSSBOProcessor::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    auto vk_buf = std::dynamic_pointer_cast<VKBuffer>(buffer);
    if (!vk_buf || m_network->slot_count() == 0)
        return;

    const auto& slots = m_network->slots();

    bool refit = false;
    if (slots[0].node && slots[0].node->needs_gpu_update()) {
        upload_template(vk_buf);
        if (m_deformed_vertices > 0) {
            const bool fits = slots[0].node->get_vertex_data().size() == m_deformed_vertices * sizeof(Kakshya::MeshVertex);
            refit = fits != m_deformation_fits;
            m_deformation_fits = fits;
        }
    }

    if (any_slot_dirty() || refit) {
        upload_transforms(vk_buf);
        clear_dirty_flags();
    }

    push_ssbo_binding(vk_buf);
}

// =============================================================================
// Private
// =============================================================================

void InstanceSSBOProcessor::upload_template(const std::shared_ptr<VKBuffer>& vertex_buf)
{
    const auto& slots = m_network->slots();
    if (slots.empty() || !slots[0].node)
        return;

    const auto span = slots[0].node->get_vertex_data();
    if (span.empty())
        return;

    upload_resizing(span.data(), span.size_bytes(), vertex_buf, m_vertex_staging);

    if (!vertex_buf->has_vertex_layout()) {
        if (auto layout = slots[0].node->get_vertex_layout()) {
            layout->vertex_count = static_cast<uint32_t>(span.size_bytes() / layout->stride_bytes);
            vertex_buf->set_vertex_layout(*layout);
        }
    }

    upload_indices(vertex_buf);
}

void InstanceSSBOProcessor::upload_indices(const std::shared_ptr<VKBuffer>& vertex_buf)
{
    const auto indices = m_network->slots()[0].node->get_index_data();
    if (indices.empty())
        return;

    if (!m_index_buffer) {
        m_index_buffer = std::make_shared<VKBuffer>(
            indices.size_bytes(), VKBuffer::Usage::INDEX, Kakshya::DataModality::UNKNOWN);
        ensure_initialized(m_index_buffer);
        m_index_staging = create_staging_buffer(indices.size_bytes());
    }

    upload_resizing(indices.data(), indices.size_bytes(), m_index_buffer, m_index_staging);

    vertex_buf->set_index_resources(
        m_index_buffer->get_buffer(),
        m_index_buffer->get_buffer_resources().memory,
        indices.size_bytes());
}

void InstanceSSBOProcessor::upload_transforms(const std::shared_ptr<VKBuffer>& vertex_buf)
{
    const auto& slots = m_network->slots();
    m_transform_scratch.resize(slots.size());

    for (const auto& slot : slots)
        m_transform_scratch[slot.index] = slot.transform;

    if (!m_transform_arena)
        return;

    const size_t bytes = m_transform_scratch.size() * sizeof(glm::mat4);
    if (bytes > m_transform_arena->slot_bytes()) {
        m_transform_arena->initialize(bytes * 3 / 2, Portal::Graphics::BufferUsageHint::HOST_STORAGE, 3);
    }

    m_transform_slot = m_transform_arena->acquire();
    m_transform_arena->write(m_transform_slot, m_transform_scratch.data(), bytes);

    upload_payload();
    upload_deformation();

    if (auto rp = vertex_buf->get_render_processor())
        rp->set_instance_count(static_cast<uint32_t>(slots.size()));
}

void InstanceSSBOProcessor::upload_payload()
{
    if (!m_payload_arena || !m_payload_arena->is_initialized())
        return;

    const auto& slots = m_network->slots();
    m_payload_scratch.assign(slots.size(), InstancePayload {});
    for (const auto& slot : slots) {
        const bool skinned = m_deformation_fits && slot.palette.size() == m_joint_count;
        const bool morphed = m_deformation_fits && slot.morph_weights.size() == m_target_count;
        m_payload_scratch[slot.index] = { .tint = slot.tint,
            .scalar = slot.scalar,
            .texture = slot.texture,
            .joint_count = skinned ? m_joint_count : 0,
            .target_count = morphed ? m_target_count : 0,
            .vertex_count = m_deformed_vertices };
    }

    const size_t bytes = m_payload_scratch.size() * sizeof(InstancePayload);
    if (bytes > m_payload_arena->slot_bytes()) {
        m_payload_arena->initialize(bytes * 3 / 2, Portal::Graphics::BufferUsageHint::HOST_STORAGE, 3);
    }

    m_payload_slot = m_payload_arena->acquire();
    m_payload_arena->write(m_payload_slot, m_payload_scratch.data(), bytes);
}

void InstanceSSBOProcessor::write_stream(Stream& stream, const void* data, size_t bytes)
{
    constexpr auto usage = Portal::Graphics::BufferUsageHint::HOST_STORAGE;
    if (!stream.arena) {
        stream.arena = std::make_shared<Portal::Graphics::SlotArena>();
        stream.arena->initialize(bytes, usage, 3);
    } else if (bytes > stream.arena->slot_bytes()) {
        stream.arena->initialize(bytes * 3 / 2, usage, 3);
    }
    stream.slot = stream.arena->acquire();
    stream.arena->write(stream.slot, data, bytes);
}

void InstanceSSBOProcessor::upload_deformation_template()
{
    m_joint_count = 0;
    m_target_count = 0;
    m_deformed_vertices = 0;
    m_deformation_fits = false;

    const auto& first = m_network->slots()[0];
    if ((!first.skin && !first.morphs) || !first.node) {
        return;
    }

    const auto data = first.node->get_vertex_data();
    const size_t vertices = data.size() / sizeof(Kakshya::MeshVertex);
    if (vertices == 0 || data.size() % sizeof(Kakshya::MeshVertex) != 0 || (first.skin && !first.skin->valid(vertices))) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "InstanceSSBOProcessor: the skin does not cover the template's {} vertices, so the instances are not deformed", vertices);
        const std::array<uint32_t, 8> unused {};
        write_stream(m_influences, unused.data(), sizeof(unused));
        write_stream(m_deltas, unused.data(), sizeof(float));
        m_deformed_vertices = 1;
        return;
    }

    struct Influence {
        glm::uvec4 joints { 0 };
        glm::vec4 weights { 0.0F };
    };
    std::vector<Influence> influences(vertices);
    for (size_t v = 0; first.skin && v < vertices; ++v) {
        influences[v] = { first.skin->influence_joints[v], first.skin->influence_weights[v] };
    }
    write_stream(m_influences, influences.data(), influences.size() * sizeof(Influence));

    std::vector<Kakshya::MeshVertex> base(vertices);
    std::memcpy(base.data(), data.data(), data.size());
    const auto deltas = shape_offsets(first.morphs ? *first.morphs : Kakshya::MeshMorphTargets {}, base);
    write_stream(m_deltas, deltas.data(), deltas.size() * sizeof(float));

    m_joint_count = first.skin ? static_cast<uint32_t>(first.skin->joint_count()) : 0;
    m_target_count = first.morphs ? static_cast<uint32_t>(first.morphs->size()) : 0;
    m_deformed_vertices = static_cast<uint32_t>(vertices);
    m_deformation_fits = true;
}

void InstanceSSBOProcessor::upload_deformation()
{
    if (m_deformed_vertices == 0) {
        return;
    }

    const auto& slots = m_network->slots();
    std::vector<glm::mat4> palettes(std::max<size_t>(1, slots.size() * m_joint_count), glm::mat4(1.0F));
    std::vector<float> weights(std::max<size_t>(1, slots.size() * m_target_count), 0.0F);
    for (const auto& slot : slots) {
        if (slot.palette.size() == m_joint_count) {
            std::ranges::copy(slot.palette, palettes.begin() + static_cast<std::ptrdiff_t>(slot.index * m_joint_count));
        }
        if (slot.morph_weights.size() == m_target_count) {
            std::ranges::copy(slot.morph_weights, weights.begin() + static_cast<std::ptrdiff_t>(slot.index * m_target_count));
        }
    }
    write_stream(m_palettes, palettes.data(), palettes.size() * sizeof(glm::mat4));
    write_stream(m_weights, weights.data(), weights.size() * sizeof(float));
}

void InstanceSSBOProcessor::push_ssbo_binding(const std::shared_ptr<VKBuffer>& vertex_buf)
{
    if (!m_transform_arena || !m_transform_arena->is_initialized())
        return;

    auto& bindings = vertex_buf->get_engine_context().ssbo_bindings;

    const auto push = [&bindings](uint32_t binding, const std::shared_ptr<Portal::Graphics::SlotArena>& arena, size_t slot) {
        Portal::Graphics::DescriptorBindingInfo info;
        info.set = 0;
        info.binding = binding;
        info.type = vk::DescriptorType::eStorageBuffer;
        info.buffer_info = arena->info(slot);
        info.arena = arena;
        info.slot = slot;

        auto it = std::ranges::find_if(bindings, [binding](const auto& b) {
            return b.set == 0 && b.binding == binding;
        });
        if (it != bindings.end()) {
            *it = info;
        } else {
            bindings.push_back(info);
        }
    };

    push(k_transform_ssbo_binding, m_transform_arena, m_transform_slot);
    if (m_payload_arena && m_payload_arena->is_initialized()) {
        push(k_payload_ssbo_binding, m_payload_arena, m_payload_slot);
    }
    if (m_deformed_vertices > 0) {
        push(k_influence_ssbo_binding, m_influences.arena, m_influences.slot);
        push(k_delta_ssbo_binding, m_deltas.arena, m_deltas.slot);
        push(k_palette_ssbo_binding, m_palettes.arena, m_palettes.slot);
        push(k_weight_ssbo_binding, m_weights.arena, m_weights.slot);
    }
}

bool InstanceSSBOProcessor::any_slot_dirty() const
{
    return std::ranges::any_of(m_network->slots(), [](const auto& s) { return s.dirty; });
}

void InstanceSSBOProcessor::clear_dirty_flags()
{
    for (auto& slot : m_network->slots())
        slot.dirty = false;
}

} // namespace MayaFlux::Buffers
