#include "CompositeGeometryProcessor.hpp"

#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Nodes/Graphics/GeometryWriterNode.hpp"

#include "CompositeGeometryBuffer.hpp"

namespace MayaFlux::Buffers {

CompositeGeometryProcessor::CompositeGeometryProcessor()
{
    m_processing_token = ProcessingToken::GRAPHICS_BACKEND;
    initialize_buffer_service();
}

void CompositeGeometryProcessor::add_geometry(
    const std::string& name,
    const std::shared_ptr<Nodes::GpuSync::GeometryWriterNode>& node,
    Portal::Graphics::PrimitiveTopology topology)
{
    if (!node) {
        error<std::invalid_argument>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "Cannot add null geometry node '{}'", name);
    }

    auto it = std::ranges::find_if(m_collections,
        [&name](const auto& c) { return c.name == name; });

    if (it != m_collections.end()) {
        MF_WARN(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Collection '{}' already exists, replacing", name);
        it->node = node;
        it->topology = topology;
        it->vertex_layout.reset();
        return;
    }

    m_collections.push_back(GeometryCollection {
        .name = name,
        .node = node,
        .topology = topology,
        .vertex_offset = 0,
        .vertex_count = 0 });

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Added geometry collection '{}' with topology {}",
        name, static_cast<int>(topology));
}

void CompositeGeometryProcessor::remove_geometry(const std::string& name)
{
    auto it = std::ranges::find_if(m_collections,
        [&name](const auto& c) { return c.name == name; });

    if (it == m_collections.end()) {
        MF_WARN(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Attempted to remove non-existent collection '{}'", name);
        return;
    }

    m_collections.erase(it);

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Removed geometry collection '{}'", name);
}

std::optional<CompositeGeometryProcessor::GeometryCollection>
CompositeGeometryProcessor::get_collection(const std::string& name) const
{
    auto it = std::ranges::find_if(m_collections,
        [&name](const auto& c) { return c.name == name; });

    if (it == m_collections.end())
        return std::nullopt;
    return *it;
}

void CompositeGeometryProcessor::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    if (m_collections.empty())
        return;

    auto vk_buffer = std::dynamic_pointer_cast<VKBuffer>(buffer);
    if (!vk_buffer) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "CompositeGeometryProcessor requires VKBuffer");
        return;
    }

    size_t total_bytes = 0;
    for (const auto& col : m_collections) {
        total_bytes += col.node->get_vertex_buffer_size_bytes();
    }

    if (total_bytes == 0) {
        if (vk_buffer->is_host_visible())
            vk_buffer->clear();
        return;
    }

    size_t available_size = vk_buffer->get_size_bytes();
    if (total_bytes > available_size) {
        auto new_size = static_cast<size_t>(static_cast<float>(total_bytes) * 1.5F);
        vk_buffer->resize(new_size, false);
        available_size = new_size;
    }

    m_staging_aggregate.resize(total_bytes);
    size_t current_byte_offset = 0;

    for (auto& col : m_collections) {
        auto vertex_data = col.node->get_vertex_data();

        if (vertex_data.empty()) {
            col.vertex_offset = 0;
            col.vertex_count = 0;
            continue;
        }

        std::memcpy(
            m_staging_aggregate.data() + current_byte_offset,
            vertex_data.data(),
            vertex_data.size_bytes());

        size_t stride = col.node->get_vertex_stride();
        if (stride == 0) {
            MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "Collection '{}' has zero vertex stride", col.name);
            continue;
        }

        col.vertex_offset = static_cast<uint32_t>(current_byte_offset / stride);
        col.vertex_count = col.node->get_vertex_count();
        // col.vertex_count = static_cast<uint32_t>(vertex_data.size_bytes() / stride);

        if (auto layout = col.node->get_vertex_layout()) {
            layout->vertex_count = col.vertex_count;
            layout->stride_bytes = static_cast<uint32_t>(stride);
            col.vertex_layout = std::move(layout);
        }

        MF_RT_TRACE(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Collection '{}': {} vertices at offset {} (stride: {})",
            col.name, col.vertex_count, col.vertex_offset, stride);

        current_byte_offset += vertex_data.size_bytes();
    }

    auto composite_buffer = std::dynamic_pointer_cast<CompositeGeometryBuffer>(buffer);
    if (composite_buffer) {
        for (const auto& col : m_collections) {
            composite_buffer->update_collection_render_range(
                col.name, col.vertex_offset, col.vertex_count);

            if (col.vertex_layout) {
                composite_buffer->update_collection_vertex_layout(
                    col.name, *col.vertex_layout);
            }
        }
    }

    upload_to_gpu(
        m_staging_aggregate.data(),
        std::min(total_bytes, available_size),
        vk_buffer,
        nullptr);
}

} // namespace MayaFlux::Buffers
