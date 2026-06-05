#include "InstanceSSBOProcessor.hpp"

#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"

#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
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
    m_transform_ssbo = std::make_shared<VKBuffer>(
        ssbo_bytes, VKBuffer::Usage::COMPUTE, Kakshya::DataModality::UNKNOWN);
    ensure_initialized(m_transform_ssbo);

    m_transform_staging = create_staging_buffer(ssbo_bytes);

    upload_template(vk_buf);
    upload_transforms(vk_buf);
    push_ssbo_binding(vk_buf);

    MF_INFO(Journal::Component::Buffers, Journal::Context::Init,
        "InstanceSSBOProcessor: {} slots, {} bytes template, {} bytes SSBO",
        slot_count, vk_buf->get_size_bytes(), ssbo_bytes);
}

void InstanceSSBOProcessor::on_detach(const std::shared_ptr<Buffer>&)
{
    m_vertex_staging.reset();
    m_transform_ssbo.reset();
    m_transform_staging.reset();
}

void InstanceSSBOProcessor::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    auto vk_buf = std::dynamic_pointer_cast<VKBuffer>(buffer);
    if (!vk_buf || m_network->slot_count() == 0)
        return;

    const auto& slots = m_network->slots();

    if (slots[0].node && slots[0].node->needs_gpu_update())
        upload_template(vk_buf);

    if (any_slot_dirty()) {
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
}

void InstanceSSBOProcessor::upload_transforms(const std::shared_ptr<VKBuffer>& vertex_buf)
{
    const auto& slots = m_network->slots();
    m_transform_scratch.resize(slots.size());

    for (const auto& slot : slots)
        m_transform_scratch[slot.index] = slot.transform;

    upload_resizing(
        m_transform_scratch.data(),
        m_transform_scratch.size() * sizeof(glm::mat4),
        m_transform_ssbo,
        m_transform_staging);

    if (auto rp = vertex_buf->get_render_processor())
        rp->set_instance_count(static_cast<uint32_t>(slots.size()));
}

void InstanceSSBOProcessor::push_ssbo_binding(const std::shared_ptr<VKBuffer>& vertex_buf)
{
    if (!m_transform_ssbo)
        return;

    auto& bindings = vertex_buf->get_engine_context().ssbo_bindings;

    Portal::Graphics::DescriptorBindingInfo info;
    info.set = 0;
    info.binding = k_transform_ssbo_binding;
    info.type = vk::DescriptorType::eStorageBuffer;
    info.buffer_info.buffer = m_transform_ssbo->get_buffer();
    info.buffer_info.offset = 0;
    info.buffer_info.range = m_transform_ssbo->get_size_bytes();

    auto it = std::ranges::find_if(bindings, [](const auto& b) {
        return b.set == 0 && b.binding == k_transform_ssbo_binding;
    });
    if (it != bindings.end()) {
        *it = info;
    } else {
        bindings.push_back(info);
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
