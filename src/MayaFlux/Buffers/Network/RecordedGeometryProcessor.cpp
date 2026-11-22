#include "RecordedGeometryProcessor.hpp"

#include "RecordedGeometryBuffer.hpp"

#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Kakshya/NDData/SpatialAccess.hpp"
#include "MayaFlux/Kakshya/NDData/VertexFormats.hpp"
#include "MayaFlux/Nodes/Network/Operators/GpuFieldOperator.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

RecordedGeometryProcessor::RecordedGeometryProcessor(
    Kakshya::VertexAccessConfig access,
    std::shared_ptr<Nodes::Network::GpuFieldOperator> field_op)
    : m_access(access)
    , m_field_op(std::move(field_op))
{
}

void RecordedGeometryProcessor::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    auto target = std::dynamic_pointer_cast<RecordedGeometryBuffer>(buffer);
    if (!target) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "RecordedGeometryProcessor requires a RecordedGeometryBuffer");
        return;
    }

    auto frame = target->current_frame();
    const bool fresh = frame != m_frame;
    if (fresh) {
        m_frame = std::move(frame);
        load(target);
    }

    write_liveness(target);

    const bool restore = m_field_op && m_field_op->demands_full_vertex_restore();
    if ((fresh || restore) && !m_bytes.empty()) {
        upload_to_gpu(m_bytes.data(), m_bytes.size(), target);
    }
}

void RecordedGeometryProcessor::load(const std::shared_ptr<RecordedGeometryBuffer>& target)
{
    m_bytes.clear();
    m_clusters.clear();
    m_runs.clear();
    m_count = 0;

    if (m_frame && !m_frame->positions.empty()) {
        if (m_frame->topology != target->topology()) {
            if (!m_topology_reported) {
                m_topology_reported = true;
                MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                    "RecordedGeometryBuffer: the frame's topology is not the stream's, nothing is drawn");
            }
        } else if (auto access = Kakshya::as_spatial_vertex_access(*m_frame, m_access)) {
            m_topology_reported = false;
            m_unreadable_reported = false;

            const auto total = static_cast<uint32_t>(access->byte_count / sizeof(Kakshya::Vertex));
            m_count = std::min(total, target->capacity());

            if (total > m_count) {
                if (!m_clamp_reported) {
                    m_clamp_reported = true;
                    MF_RT_WARN(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                        "RecordedGeometryBuffer: a frame of {} vertices exceeds the capacity of {}, "
                        "its first {} are drawn",
                        total, target->capacity(), m_count);
                }
            } else {
                m_clamp_reported = false;
            }

            m_bytes = std::move(access->conversion_buffer);
            m_bytes.resize(static_cast<size_t>(m_count) * sizeof(Kakshya::Vertex));

            m_clusters = Kakshya::spatial_cluster_ids(*m_frame);
            m_clusters.resize(m_count);

            if (target->topology() == Portal::Graphics::PrimitiveTopology::LINE_STRIP) {
                size_t offset = 0;
                for (const int32_t length : m_frame->vertex_counts_per_curve) {
                    if (offset >= m_count) {
                        break;
                    }
                    const auto span = static_cast<size_t>(std::max(length, 0));
                    const auto drawn = static_cast<uint32_t>(std::min<size_t>(span, m_count - offset));
                    if (drawn > 0) {
                        m_runs.push_back({ .topology = target->topology(),
                            .vertex_offset = static_cast<uint32_t>(offset),
                            .vertex_count = drawn });
                    }
                    offset += span;
                }
            }
        } else if (!m_unreadable_reported) {
            m_unreadable_reported = true;
            MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "RecordedGeometryBuffer: the frame could not be packed into vertices, nothing is drawn");
        }
    }

    if (m_count > 0 && target->has_state("hash_cluster_id")) {
        upload_back_buffer(target->write_state_slot("hash_cluster_id"), m_clusters.data(),
            static_cast<size_t>(m_count) * sizeof(uint32_t), m_staging);
    }

    target->update_draw(m_count, m_runs);
}

void RecordedGeometryProcessor::write_liveness(const std::shared_ptr<RecordedGeometryBuffer>& target)
{
    if (m_alive_count == m_count) {
        return;
    }

    if (target->has_state("mutation_alive")) {
        std::vector<uint32_t> alive(target->get_state_bytes("mutation_alive") / sizeof(uint32_t), 0U);
        std::fill_n(alive.begin(), std::min<size_t>(m_count, alive.size()), 1U);
        upload_back_buffer(target->write_state_slot("mutation_alive"), alive.data(),
            alive.size() * sizeof(uint32_t), m_staging);
    }

    m_alive_count = m_count;
}

} // namespace MayaFlux::Buffers
