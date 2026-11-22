#pragma once

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Kakshya/NDData/SpatialData.hpp"
#include "MayaFlux/Kakshya/NDData/VertexAccess.hpp"

namespace MayaFlux::Nodes::Network {
class GpuFieldOperator;
}

namespace MayaFlux::Buffers {

class RecordedGeometryBuffer;

/**
 * @class RecordedGeometryProcessor
 * @brief Writes the newest frame of a RecordedGeometryBuffer into its storage each cycle.
 *
 * A new frame is packed into whole vertex records, clamped to the buffer's capacity, and
 * written together with its cluster ids and the liveness of every record, and the buffer
 * is told what to draw. The same frame is written again on later cycles only while the
 * buffer's field operator has fields bound, because the field pass displaces the records
 * in place and the reference vertices must be restored first.
 *
 * Runs on the graphics cycle as the buffer's default processor. Frames reach it through
 * RecordedGeometryBuffer::write_frame, so any thread may supply them.
 */
class MAYAFLUX_API RecordedGeometryProcessor : public VKBufferProcessor {
public:
    /**
     * @param access Values for every column a frame does not carry.
     * @param field_op The buffer's field operator, or null. Fields bound on it make the
     *        frame be written every cycle.
     */
    RecordedGeometryProcessor(
        Kakshya::VertexAccessConfig access,
        std::shared_ptr<Nodes::Network::GpuFieldOperator> field_op);

    void processing_function(const std::shared_ptr<Buffer>& buffer) override;

private:
    Kakshya::VertexAccessConfig m_access;
    std::shared_ptr<Nodes::Network::GpuFieldOperator> m_field_op;

    std::shared_ptr<const Kakshya::SpatialData> m_frame;
    std::vector<std::byte> m_bytes;
    std::vector<uint32_t> m_clusters;
    std::vector<Portal::Graphics::DrawRun> m_runs;
    uint32_t m_count {};
    std::optional<uint32_t> m_alive_count;
    std::shared_ptr<VKBuffer> m_staging;

    bool m_clamp_reported {};
    bool m_topology_reported {};
    bool m_unreadable_reported {};

    /** @brief Pack a new frame and tell the buffer what it occupies. */
    void load(const std::shared_ptr<RecordedGeometryBuffer>& target);

    /** @brief Mark the first m_count records live and the rest dead. */
    void write_liveness(const std::shared_ptr<RecordedGeometryBuffer>& target);
};

} // namespace MayaFlux::Buffers
