#include "AssembledGeometryProcessor.hpp"

#include "AssembledGeometryBuffer.hpp"

#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Kakshya/NDData/VertexFormats.hpp"
#include "MayaFlux/Nodes/Network/AssemblyNetwork.hpp"
#include "MayaFlux/Nodes/Network/Operators/OperatorChain.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

namespace {

    using Portal::Graphics::PrimitiveTopology;

    bool is_triangle(PrimitiveTopology topology)
    {
        return topology == PrimitiveTopology::TRIANGLE_LIST
            || topology == PrimitiveTopology::TRIANGLE_STRIP
            || topology == PrimitiveTopology::TRIANGLE_FAN;
    }

} // namespace

AssembledGeometryProcessor::AssembledGeometryProcessor(
    const std::shared_ptr<Nodes::Network::AssemblyNetwork>& network)
    : m_network(network)
{
    m_processing_token = ProcessingToken::GRAPHICS_BACKEND;
    initialize_buffer_service();
}

void AssembledGeometryProcessor::on_attach(const std::shared_ptr<Buffer>& buffer)
{
    auto vk_buffer = std::dynamic_pointer_cast<VKBuffer>(buffer);
    if (!vk_buffer) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "AssembledGeometryProcessor requires a VKBuffer");
        return;
    }

    ensure_initialized(vk_buffer);
    if (!vk_buffer->is_host_visible()) {
        m_vertex_staging = create_staging_buffer(vk_buffer->get_size_bytes());
    }

    aggregate();
    upload(vk_buffer);
}

void AssembledGeometryProcessor::on_detach(const std::shared_ptr<Buffer>& /*buffer*/)
{
    m_vertex_staging.reset();
    m_index_buffer.reset();
    m_index_staging.reset();
    m_uploaded.clear();
}

void AssembledGeometryProcessor::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    if (!m_network || !m_network->is_enabled()) {
        return;
    }

    auto vk_buffer = std::dynamic_pointer_cast<VKBuffer>(buffer);
    if (!vk_buffer || (!items_changed() && !demands_vertex_restore())) {
        return;
    }

    aggregate();
    upload(vk_buffer);
}

bool AssembledGeometryProcessor::items_changed() const
{
    const auto& items = m_network->assembly()->get_items();
    return items != m_uploaded
        || std::ranges::any_of(items, [](const auto& item) { return item->needs_gpu_update(); });
}

bool AssembledGeometryProcessor::demands_vertex_restore() const
{
    const auto chain = m_network->get_operator_chain();
    return chain && std::ranges::any_of(chain->operators(), [](const auto& op) {
        return op && op->demands_full_vertex_restore();
    });
}

void AssembledGeometryProcessor::aggregate()
{
    auto* assembly = m_network->assembly();
    const auto& items = assembly->get_items();

    m_vertices.clear();
    m_indices.clear();
    m_runs.clear();
    m_uploaded = items;

    const bool indexed = std::ranges::any_of(items, [](const auto& item) {
        return item->has_indices() && is_triangle(item->get_primitive_topology());
    });

    uint32_t first_vertex = 0;
    for (const auto& item : items) {
        const uint32_t count = item->get_vertex_count();
        if (count == 0) {
            continue;
        }

        const auto bytes = item->get_vertex_data();
        if (item->get_vertex_stride() != sizeof(Kakshya::Vertex)
            || bytes.size() < static_cast<size_t>(count) * sizeof(Kakshya::Vertex)) {
            MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "AssembledGeometryProcessor: an item with {} byte vertices is not drawn, "
                "records are {} bytes",
                item->get_vertex_stride(), sizeof(Kakshya::Vertex));
            continue;
        }

        const auto records = bytes.first(static_cast<size_t>(count) * sizeof(Kakshya::Vertex));
        m_vertices.insert(m_vertices.end(), records.begin(), records.end());

        Portal::Graphics::DrawRun run {
            .topology = item->get_primitive_topology(),
            .vertex_offset = first_vertex,
            .vertex_count = count
        };

        if (indexed && is_triangle(run.topology)) {
            run.vertex_offset = static_cast<uint32_t>(m_indices.size());
            if (item->has_indices()) {
                for (const uint32_t index : item->get_index_data()) {
                    m_indices.push_back(index + first_vertex);
                }
            } else {
                for (uint32_t i = 0; i < count; ++i) {
                    m_indices.push_back(first_vertex + i);
                }
            }
            run.vertex_count = static_cast<uint32_t>(m_indices.size()) - run.vertex_offset;
        }

        if (!m_runs.empty() && m_runs.back().topology == run.topology
            && Portal::Graphics::is_concatenable_topology(run.topology)
            && m_runs.back().vertex_offset + m_runs.back().vertex_count == run.vertex_offset) {
            m_runs.back().vertex_count += run.vertex_count;
        } else {
            m_runs.push_back(run);
        }

        first_vertex += count;
    }

    assembly->mark_vertex_data_clean();
}

void AssembledGeometryProcessor::upload(const std::shared_ptr<VKBuffer>& buffer)
{
    if (!m_vertices.empty()) {
        upload_resizing(m_vertices.data(), m_vertices.size(), buffer, m_vertex_staging);
    }

    upload_index_stream(buffer);

    const auto vertex_count = static_cast<uint32_t>(m_vertices.size() / sizeof(Kakshya::Vertex));
    auto layout = Kakshya::VertexLayout::for_points(sizeof(Kakshya::Vertex));
    if (!m_uploaded.empty()) {
        if (const auto first = m_uploaded.front()->get_vertex_layout()) {
            layout = *first;
        }
    }
    layout.vertex_count = vertex_count;
    buffer->set_vertex_layout(layout);

    if (auto target = std::dynamic_pointer_cast<AssembledGeometryBuffer>(buffer)) {
        target->update_draw(m_runs);
    }
}

void AssembledGeometryProcessor::upload_index_stream(const std::shared_ptr<VKBuffer>& buffer)
{
    if (m_indices.empty()) {
        buffer->set_index_resources({}, {}, 0);
        return;
    }

    const size_t bytes = m_indices.size() * sizeof(uint32_t);
    if (!m_index_buffer) {
        m_index_buffer = std::make_shared<VKBuffer>(
            bytes, VKBuffer::Usage::INDEX, Kakshya::DataModality::UNKNOWN);
        ensure_initialized(m_index_buffer);
        m_index_staging = create_staging_buffer(bytes);
    }

    upload_resizing(m_indices.data(), bytes, m_index_buffer, m_index_staging);

    buffer->set_index_resources(
        m_index_buffer->get_buffer(),
        m_index_buffer->get_buffer_resources().memory,
        bytes);
}

} // namespace MayaFlux::Buffers
