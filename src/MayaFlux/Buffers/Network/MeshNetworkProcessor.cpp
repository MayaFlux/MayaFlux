#include "MeshNetworkProcessor.hpp"

#include "MayaFlux/Nodes/Network/MeshNetwork.hpp"

#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Registry/Service/BufferService.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

MeshNetworkProcessor::MeshNetworkProcessor(
    std::shared_ptr<Nodes::Network::MeshNetwork> network)
    : m_network(std::move(network))
{
    m_processing_token = ProcessingToken::GRAPHICS_BACKEND;
    initialize_buffer_service();
}

// =============================================================================
// on_attach
// =============================================================================

void MeshNetworkProcessor::on_attach(const std::shared_ptr<Buffer>& buffer)
{
    if (!m_network || m_network->slot_count() == 0) {
        MF_WARN(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "MeshNetworkProcessor attached to empty network");
        return;
    }

    auto vk_buf = std::dynamic_pointer_cast<VKBuffer>(buffer);
    if (!vk_buf) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "MeshNetworkProcessor: attached buffer is not a VKBuffer");
        return;
    }

    ensure_initialized(vk_buf);

    allocate_gpu_buffers(vk_buf);
    upload_combined(vk_buf);
    link_index_resources(vk_buf);
    clear_slot_dirty_flags();

    MF_INFO(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "MeshNetworkProcessor: initialised — {} slots, {} bytes vertex, {} indices",
        m_network->slot_count(),
        m_vertex_aggregate.size(),
        m_index_aggregate.size());
}

void MeshNetworkProcessor::on_detach(const std::shared_ptr<Buffer>& /*buffer*/)
{
    m_gpu_index_buffer.reset();
    m_vertex_staging.reset();
    m_index_staging.reset();
    m_model_ssbo.reset();
    m_slot_index_ssbo.reset();
}

// =============================================================================
// processing_function
// =============================================================================

void MeshNetworkProcessor::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    if (!m_network || !any_slot_dirty())
        return;

    auto vk_buf = std::dynamic_pointer_cast<VKBuffer>(buffer);
    if (!vk_buf)
        return;

    upload_combined(vk_buf);
    link_index_resources(vk_buf);
    clear_slot_dirty_flags();

    if (!m_model_ssbo || !m_slot_index_ssbo)
        allocate_ssbo_buffers();

    upload_ssbos();
    push_ssbo_bindings(vk_buf);

    MF_RT_TRACE(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "MeshNetworkProcessor: re-uploaded {} bytes vertex, {} indices",
        m_vertex_aggregate.size(), m_index_aggregate.size());
}

// =============================================================================
// Private helpers
// =============================================================================

void MeshNetworkProcessor::allocate_gpu_buffers(
    const std::shared_ptr<VKBuffer>& vertex_buf)
{
    const size_t v_bytes = total_vertex_bytes();
    const size_t i_bytes = total_index_count() * sizeof(uint32_t);

    if (v_bytes == 0 || i_bytes == 0) {
        MF_WARN(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "MeshNetworkProcessor::allocate_gpu_buffers: network has no geometry");
        return;
    }

    if (!vertex_buf->is_host_visible())
        m_vertex_staging = create_staging_buffer(v_bytes);

    m_gpu_index_buffer = std::make_shared<VKBuffer>(
        i_bytes, VKBuffer::Usage::INDEX, Kakshya::DataModality::UNKNOWN);
    m_buffer_service->initialize_buffer(m_gpu_index_buffer);
    m_index_staging = create_staging_buffer(i_bytes);

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "MeshNetworkProcessor: allocated vertex ({} bytes) + index ({} bytes)",
        v_bytes, i_bytes);
}

void MeshNetworkProcessor::upload_combined(
    const std::shared_ptr<VKBuffer>& vertex_buf)
{
    const auto& slots = m_network->slots();
    const auto& order = m_network->sorted_indices();

    m_vertex_aggregate.clear();
    m_index_aggregate.clear();
    m_model_aggregate.clear();
    m_slot_index_aggregate.clear();

    uint32_t running_vertex_count = 0;

    for (uint32_t idx : order) {
        const auto& slot = slots[idx];
        if (!slot.node)
            continue;

        const auto& verts = slot.node->get_mesh_vertices();
        const auto& indices = slot.node->get_mesh_indices();

        const size_t v_bytes = verts.size() * sizeof(Nodes::MeshVertex);
        const auto* v_src = reinterpret_cast<const uint8_t*>(verts.data());
        m_vertex_aggregate.insert(m_vertex_aggregate.end(), v_src, v_src + v_bytes);

        for (uint32_t i : indices)
            m_index_aggregate.push_back(i + running_vertex_count);

        m_model_aggregate.push_back(slot.world_transform);

        const uint32_t sorted_slot_pos = static_cast<uint32_t>(m_model_aggregate.size() - 1);
        m_slot_index_aggregate.insert(
            m_slot_index_aggregate.end(), verts.size(), sorted_slot_pos);

        running_vertex_count += static_cast<uint32_t>(verts.size());
    }

    if (m_vertex_aggregate.empty())
        return;

    const size_t v_required = m_vertex_aggregate.size();
    if (v_required > vertex_buf->get_size_bytes()) {
        const auto new_size = static_cast<size_t>(
            static_cast<float>(v_required) * 1.5F);
        vertex_buf->resize(new_size, false);
        if (m_vertex_staging)
            m_vertex_staging->resize(new_size, false);
    }

    upload_to_gpu(
        m_vertex_aggregate.data(),
        v_required,
        vertex_buf,
        vertex_buf->is_host_visible() ? nullptr : m_vertex_staging);

    const size_t i_required = m_index_aggregate.size() * sizeof(uint32_t);
    if (m_gpu_index_buffer && i_required > m_gpu_index_buffer->get_size_bytes()) {
        const auto new_size = static_cast<size_t>(
            static_cast<float>(i_required) * 1.5F);
        m_gpu_index_buffer->resize(new_size, false);
        if (m_index_staging)
            m_index_staging->resize(new_size, false);
    }

    if (m_gpu_index_buffer && !m_index_aggregate.empty()) {
        upload_to_gpu(
            m_index_aggregate.data(),
            i_required,
            m_gpu_index_buffer,
            m_index_staging);
    }

    for (uint32_t idx : order) {
        const auto& slot = slots[idx];
        if (slot.node && slot.node->get_mesh_vertex_count() > 0) {
            if (auto layout = slot.node->get_vertex_layout()) {
                layout->vertex_count = running_vertex_count;
                vertex_buf->set_vertex_layout(*layout);
            }
            break;
        }
    }
}

void MeshNetworkProcessor::link_index_resources(
    const std::shared_ptr<VKBuffer>& vertex_buf)
{
    if (!m_gpu_index_buffer)
        return;

    vertex_buf->set_index_resources(
        m_gpu_index_buffer->get_buffer(),
        m_gpu_index_buffer->get_buffer_resources().memory,
        m_gpu_index_buffer->get_size_bytes());
}

bool MeshNetworkProcessor::any_slot_dirty() const
{
    return std::ranges::any_of(m_network->slots(), [](const auto& slot) {
        return slot.dirty || (slot.node && slot.node->needs_gpu_update());
    });
}

void MeshNetworkProcessor::clear_slot_dirty_flags()
{
    for (auto& slot : m_network->slots()) {
        slot.dirty = false;
        if (slot.node)
            slot.node->clear_gpu_update_flag();
    }
}

size_t MeshNetworkProcessor::total_vertex_bytes() const
{
    size_t total = 0;
    for (const auto& slot : m_network->slots()) {
        if (slot.node)
            total += slot.node->get_mesh_vertex_count() * sizeof(Nodes::MeshVertex);
    }
    return total;
}

size_t MeshNetworkProcessor::total_index_count() const
{
    size_t total = 0;
    for (const auto& slot : m_network->slots()) {
        if (slot.node)
            total += slot.node->get_mesh_indices().size();
    }
    return total;
}

void MeshNetworkProcessor::allocate_ssbo_buffers()
{
    const size_t model_bytes = m_model_aggregate.size() * sizeof(glm::mat4);
    const size_t slot_index_bytes = m_slot_index_aggregate.size() * sizeof(uint32_t);

    if (model_bytes == 0 || slot_index_bytes == 0) {
        MF_WARN(Journal::Component::Buffers, Journal::Context::BufferProcessing, "MeshNetworkProcessor::allocate_ssbo_buffers: aggregates empty at alloc time");
        return;
    }

    m_model_ssbo = std::make_shared<VKBuffer>(
        model_bytes, VKBuffer::Usage::COMPUTE, Kakshya::DataModality::UNKNOWN);
    m_buffer_service->initialize_buffer(m_model_ssbo);

    m_slot_index_ssbo = std::make_shared<VKBuffer>(
        slot_index_bytes, VKBuffer::Usage::COMPUTE, Kakshya::DataModality::UNKNOWN);
    m_buffer_service->initialize_buffer(m_slot_index_ssbo);
}

void MeshNetworkProcessor::upload_ssbos()
{
    if (m_model_aggregate.empty() || !m_model_ssbo || !m_slot_index_ssbo)
        return;

    upload_resizing(
        m_model_aggregate.data(),
        m_model_aggregate.size() * sizeof(glm::mat4),
        m_model_ssbo,
        nullptr);

    upload_resizing(
        m_slot_index_aggregate.data(),
        m_slot_index_aggregate.size() * sizeof(uint32_t),
        m_slot_index_ssbo,
        nullptr);
}

void MeshNetworkProcessor::push_ssbo_bindings(const std::shared_ptr<VKBuffer>& buffer)
{
    if (!m_model_ssbo || !m_slot_index_ssbo)
        return;

    auto& bindings = buffer->get_pipeline_context().descriptor_buffer_bindings;

    auto push = [&](uint32_t binding_idx, const std::shared_ptr<VKBuffer>& ssbo) {
        Portal::Graphics::DescriptorBindingInfo info;
        info.set = 1;
        info.binding = binding_idx;
        info.type = vk::DescriptorType::eStorageBuffer;
        info.buffer_info.buffer = ssbo->get_buffer();
        info.buffer_info.offset = 0;
        info.buffer_info.range = ssbo->get_size_bytes();

        auto it = std::ranges::find_if(bindings, [&](const auto& b) {
            return b.set == info.set && b.binding == info.binding;
        });
        if (it != bindings.end()) {
            *it = info;
        } else {
            bindings.push_back(info);
        }
    };

    push(0, m_model_ssbo);
    push(1, m_slot_index_ssbo);
}

} // namespace MayaFlux::Buffers
