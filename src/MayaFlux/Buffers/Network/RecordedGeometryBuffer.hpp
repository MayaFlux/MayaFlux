#pragma once

#include "MayaFlux/Buffers/State/StateFieldBuffer.hpp"
#include "MayaFlux/Kakshya/NDData/SpatialData.hpp"
#include "MayaFlux/Kakshya/NDData/VertexAccess.hpp"
#include "MayaFlux/Kinesis/VertexSampler.hpp"
#include "MayaFlux/Transitive/Memory/SnapshotCell.hpp"

namespace MayaFlux::Nodes::Network {
class GpuFieldOperator;
}

namespace MayaFlux::Buffers {

class RecordedGeometryProcessor;
class RenderProcessor;

/**
 * @class RecordedGeometryBuffer
 * @brief Draws recorded spatial frames and runs them through the same spatial stages a
 *        NetworkGeometryBuffer runs: hash, density colour, claims and the field pass.
 *
 * The geometry is not simulated here. Each frame handed to write_frame replaces the
 * vertices: whole 68-byte records with every column the frame carries, the rest from
 * the access config, and the frame's cluster ids. Frames may differ in size. The buffer
 * holds capacity records, the stages dispatch over all of them, and a record past the
 * current frame is marked dead so none of them sees it. A frame with more vertices than
 * capacity keeps its first capacity vertices.
 *
 * A stream has one topology, POINT_LIST, LINE_STRIP or LINE_LIST. A frame of another
 * topology is not drawn. Curves of a strip are drawn as separate spans.
 *
 * With a GpuFieldOperator the buffer wires what its SpatialFieldConfig asks for, as a
 * NetworkGeometryBuffer does. There is no network to derive anything from, so the
 * config must name its cell_size and the buffer takes the bounds the grid covers. The
 * operator's layout must be the 68-byte vertex record. While the operator has fields
 * bound, the frame is written again every cycle, since the field pass displaces the
 * records in place. Cluster ids and liveness are written once per frame.
 *
 * A claim that relabels hash_cluster_id (transfer_on_claim) lasts until the next frame, which
 * writes the frame's recorded cluster ids again.
 *
 * write_frame may be called from any thread and never blocks. Only the newest frame
 * is kept, and it is written on the next graphics cycle.
 */
class MAYAFLUX_API RecordedGeometryBuffer : public StateFieldBuffer {
public:
    /**
     * @param capacity Most vertices of one frame the buffer draws.
     * @param topology Topology of the stream: POINT_LIST, LINE_STRIP or LINE_LIST.
     * @param bounds Box the spatial hash covers. Unused without stages to wire.
     * @param field_op Operator whose bindings and SpatialFieldConfig select the stages,
     *        or null for a buffer that only draws.
     * @param access Values for every column a frame does not carry.
     */
    RecordedGeometryBuffer(
        uint32_t capacity,
        Portal::Graphics::PrimitiveTopology topology,
        Kinesis::SamplerBounds bounds,
        std::shared_ptr<Nodes::Network::GpuFieldOperator> field_op = nullptr,
        Kakshya::VertexAccessConfig access = {});

    ~RecordedGeometryBuffer() override = default;

    /**
     * @brief Create the supplier and wire the spatial stages.
     *
     * The first frame is written after this, never before: wiring can resize the vertex
     * storage, which does not keep what it held.
     */
    void setup_processors(ProcessingToken token) override;

    /**
     * @brief Draw the buffer.
     * @param config Render config. The stream's topology stands in for a point topology,
     *        as for a NetworkGeometryBuffer, and shaders left empty take the defaults.
     *
     * Registers after everything setup_processors wired, so the draw sees the stages'
     * work. Call setup_processors first.
     */
    void setup_rendering(const RenderConfig& config);

    /**
     * @brief Replace the geometry with a frame.
     * @param frame Frame to show from the next graphics cycle. Null shows nothing.
     */
    void write_frame(std::shared_ptr<const Kakshya::SpatialData> frame);

    /** @brief The newest frame written, or null before the first. */
    [[nodiscard]] std::shared_ptr<const Kakshya::SpatialData> current_frame() const;

    /**
     * @brief Tell the draw what the current frame occupies.
     * @param vertex_count Vertices to draw. Zero hides the buffer.
     * @param runs Spans of the vertices, one per curve of a strip, empty otherwise.
     *
     * Called by the supplier each time a new frame is loaded. Applied at once when the
     * buffer is already drawn, and when setup_rendering runs otherwise.
     */
    void update_draw(uint32_t vertex_count, std::vector<Portal::Graphics::DrawRun> runs);

    /**
     * @brief Place what the buffer draws, inside whatever transform its renderers already have.
     *
     * A renderer's own transform, fixed or from a source, stays and is applied outside the
     * placement, and one set on it afterwards through a source is layered the same way.
     * Every caller writes the one placement, so it is applied once however many there are.
     * A renderer made after a call is reached by the next one. For the graphics thread.
     */
    void set_placement(const glm::mat4& placement);

    [[nodiscard]] uint32_t capacity() const noexcept { return m_capacity; }

    [[nodiscard]] Portal::Graphics::PrimitiveTopology topology() const noexcept { return m_topology; }

    /** @brief The supplier, once setup_processors has run. */
    [[nodiscard]] const std::shared_ptr<RecordedGeometryProcessor>& get_processor() const { return m_processor; }

private:
    uint32_t m_capacity;
    Portal::Graphics::PrimitiveTopology m_topology;
    Kinesis::SamplerBounds m_bounds;
    std::shared_ptr<Nodes::Network::GpuFieldOperator> m_field_op;
    Kakshya::VertexAccessConfig m_access;
    std::shared_ptr<RecordedGeometryProcessor> m_processor;

    Memory::SnapshotCell<std::shared_ptr<const Kakshya::SpatialData>> m_pending;

    std::shared_ptr<Memory::SnapshotCell<glm::mat4>> m_placement;
    std::vector<std::weak_ptr<RenderProcessor>> m_placed;

    uint32_t m_draw_count {};
    std::vector<Portal::Graphics::DrawRun> m_draw_runs;
    std::optional<uint32_t> m_layout_count;
    std::optional<uint32_t> m_applied_count;
    std::vector<Portal::Graphics::DrawRun> m_applied_runs;

    /** @brief Wire the stages the field operator asks for. */
    void wire_field();

    /** @brief Hand the render processor the current extent and spans. */
    void apply_draw();
};

} // namespace MayaFlux::Buffers
