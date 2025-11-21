#include "GeometryWriterNode.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nodes::GpuSync {

GeometryWriterNode::GeometryWriterNode(uint32_t initial_capacity)
{
    if (initial_capacity > 0 && m_vertex_stride == 0) {
        m_vertex_stride = sizeof(glm::vec3);
    }

    if (initial_capacity > 0) {
        m_vertex_buffer.resize(initial_capacity * m_vertex_stride / sizeof(float));
    }
}

void GeometryWriterNode::resize_vertex_buffer(uint32_t vertex_count, bool preserve_data)
{
    if (m_vertex_stride == 0) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Cannot resize vertex buffer: stride is 0. Call set_vertex_stride() first");
        return;
    }

    size_t new_size_floats = static_cast<size_t>(vertex_count) * m_vertex_stride / sizeof(float);

    if (!preserve_data) {
        m_vertex_buffer.clear();
        m_vertex_buffer.resize(new_size_floats, 0.0F);
    } else if (m_vertex_buffer.size() < new_size_floats) {
        m_vertex_buffer.resize(new_size_floats, 0.0F);
    } else if (m_vertex_buffer.size() > new_size_floats) {
        m_vertex_buffer.resize(new_size_floats);
    }

    m_vertex_count = vertex_count;
    m_needs_layout_update = true;
    m_vertex_data_dirty = true;

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "GeometryWriterNode: Resized vertex buffer to {} vertices ({} bytes total)",
        vertex_count, new_size_floats * sizeof(float));
}

void GeometryWriterNode::set_vertex_data(const void* data, size_t size_bytes)
{
    if (!data || size_bytes == 0) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Cannot set vertex data: null data or zero size");
        return;
    }

    if (m_vertex_stride == 0) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Cannot set vertex data: stride is 0");
        return;
    }

    auto expected_vertex_count = static_cast<uint32_t>(size_bytes / m_vertex_stride);

    if (size_bytes % m_vertex_stride != 0) {
        MF_WARN(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Vertex data size {} is not multiple of stride {}",
            size_bytes, m_vertex_stride);
    }

    size_t required_floats = size_bytes / sizeof(float);
    if (m_vertex_buffer.size() < required_floats) {
        m_vertex_buffer.resize(required_floats);
    }

    std::memcpy(m_vertex_buffer.data(), data, size_bytes);
    m_vertex_count = expected_vertex_count;
    m_needs_layout_update = true;
    m_vertex_data_dirty = true;

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "GeometryWriterNode: Set vertex data ({} vertices, {} bytes)",
        m_vertex_count, size_bytes);
}

void GeometryWriterNode::set_vertex(uint32_t vertex_index, const void* data, size_t size_bytes)
{
    if (!data || size_bytes == 0) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Cannot set vertex: null data or zero size");
        return;
    }

    if (m_vertex_stride == 0) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Cannot set vertex: stride is 0");
        return;
    }

    if (vertex_index >= m_vertex_count) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Vertex index {} out of range (count: {})",
            vertex_index, m_vertex_count);
        return;
    }

    if (size_bytes > m_vertex_stride) {
        MF_WARN(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Vertex data size {} exceeds stride {}; truncating",
            size_bytes, m_vertex_stride);
        size_bytes = m_vertex_stride;
    }

    size_t offset_floats = (static_cast<size_t>(vertex_index) * m_vertex_stride) / sizeof(float);

    std::memcpy(
        reinterpret_cast<uint8_t*>(m_vertex_buffer.data()) + (offset_floats * sizeof(float)),
        data,
        size_bytes);

    MF_TRACE(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "GeometryWriterNode: Set vertex {} ({} bytes)", vertex_index, size_bytes);

    m_vertex_data_dirty = true;
}

size_t GeometryWriterNode::get_vertex_buffer_size_bytes() const
{
    return m_vertex_buffer.size() * sizeof(float);
}

void GeometryWriterNode::set_vertex_stride(size_t stride)
{
    m_vertex_stride = stride;
    m_needs_layout_update = true;
}

std::span<const uint8_t> GeometryWriterNode::get_vertex(uint32_t vertex_index) const
{
    if (m_vertex_stride == 0 || m_vertex_count == 0) {
        return {};
    }

    if (vertex_index >= m_vertex_count) {
        MF_WARN(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Vertex index {} out of range (count: {})",
            vertex_index, m_vertex_count);
        return {};
    }

    size_t offset_floats = (static_cast<size_t>(vertex_index) * m_vertex_stride) / sizeof(float);
    size_t stride_floats = m_vertex_stride / sizeof(float);

    if (offset_floats + stride_floats > m_vertex_buffer.size()) {
        return {};
    }

    return { m_vertex_buffer.data() + offset_floats, stride_floats };
}

void GeometryWriterNode::clear()
{
    std::ranges::fill(m_vertex_buffer, 0);

    m_vertex_data_dirty = true;
}

void GeometryWriterNode::clear_and_resize(uint32_t vertex_count)
{
    resize_vertex_buffer(vertex_count, false);
}

std::vector<double> GeometryWriterNode::process_batch(unsigned int num_samples)
{
    compute_frame();

    return std::vector<double>(num_samples, 0.0);
}

void GeometryWriterNode::save_state()
{
    GeometryState state;
    state.vertex_buffer = m_vertex_buffer;
    state.vertex_count = m_vertex_count;
    state.vertex_stride = m_vertex_stride;
    state.vertex_layout = m_vertex_layout;

    m_saved_state = std::move(state);

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "GeometryWriterNode: Saved state ({} vertices, {} bytes)",
        m_vertex_count, m_vertex_buffer.size() * sizeof(uint8_t));
}

void GeometryWriterNode::restore_state()
{
    if (!m_saved_state.has_value()) {
        MF_WARN(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "GeometryWriterNode: No saved state to restore");
        return;
    }

    m_vertex_buffer = m_saved_state->vertex_buffer;
    m_vertex_count = m_saved_state->vertex_count;
    m_vertex_stride = m_saved_state->vertex_stride;
    m_vertex_layout = m_saved_state->vertex_layout;

    m_needs_layout_update = true;

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "GeometryWriterNode: Restored state ({} vertices, {} bytes)",
        m_vertex_count, m_vertex_buffer.size() * sizeof(uint8_t));
}

} // namespace MayaFlux::Nodes
