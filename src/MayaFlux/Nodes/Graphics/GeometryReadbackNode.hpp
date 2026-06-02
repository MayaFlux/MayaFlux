#pragma once

#include "GeometryWriterNode.hpp"

#include "MayaFlux/Kakshya/NDData/VertexFormats.hpp"

namespace MayaFlux::Buffers {
class VKBuffer;
} // namespace MayaFlux::Buffers

namespace MayaFlux::Nodes::GpuSync {

/**
 * @class GeometryReadbackNode
 * @brief GeometryWriterNode that downloads vertex data from a GPU buffer each frame.
 *
 * Symmetric to ComputeOutNode but for vertex buffers. Each compute_frame()
 * derives the current vertex count from the buffer's resident byte size,
 * issues a synchronous GPU-to-CPU transfer, and re-emits the result into
 * the inherited vertex buffer so the node can be placed directly in a
 * GeometryBuffer for a second render target.
 *
 * Intended for closing visual-to-audio and visual-to-visual feedback loops:
 * positions produced by a GPU geometry pass (SDF mesh, mesh deformation,
 * particle simulation) become inputs to the node graph or a second window
 * on the next cycle.
 *
 * Vertex count is derived fresh each frame from get_size_bytes() so dynamic
 * geometry (SDF, marching cubes) that changes triangle count per frame is
 * handled without stale sizing.
 *
 * The download is synchronous (execute_immediate_commands). Use at
 * VISUAL_RATE. For high-frequency readback a double-buffered async path
 * would be required.
 *
 * Usage - second window from SDF mesh:
 * @code
 * auto readback = std::make_shared<GeometryReadbackNode>(mesh_buf) | Graphics;
 * auto buf2 = vega.GeometryBuffer(readback) | Graphics;
 * buf2->setup_rendering({ .target_window = window2 });
 * @endcode
 *
 * Usage - positions into audio:
 * @code
 * auto readback = std::make_shared<GeometryReadbackNode>(mesh_buf);
 * schedule_metro(1.0 / 60.0, [readback]() {
 *     readback->compute_frame();
 *     glm::vec3 p = readback->get_position(0);
 * }, "readback", Vruta::ProcessingToken::FRAME_ACCURATE);
 * @endcode
 */
class MAYAFLUX_API GeometryReadbackNode : public GeometryWriterNode {
public:
    /**
     * @brief Construct from a GPU vertex buffer.
     *
     * @param buffer       GPU vertex buffer to download from. Must remain
     *                     valid for the lifetime of this node.
     * @param vertex_count Expected number of MeshVertex elements. Used to
     *                     size the readback vector. If 0, the count is
     *                     derived from buffer->get_size_bytes().
     */
    explicit GeometryReadbackNode(
        std::shared_ptr<Buffers::VKBuffer> buffer,
        size_t vertex_count = 0);

    /**
     * @brief Suppress automatic compute_frame() invocation by the node graph.
     *
     * GeometryReadbackNode::compute_frame() issues a synchronous GPU-to-CPU
     * transfer via download_from_gpu_async. Calling it from the graphics thread
     * (which is what GpuSync::process_sample does when the node is registered
     * at VISUAL_RATE) produces undefined behaviour due to re-entrant queue
     * submission. The node must be driven manually from a FRAME_ACCURATE metro
     * that runs on the scheduler thread.
     *
     * @return Always 0.0.
     */
    double process_sample(double /*input*/) override { return 0.0; }

    /**
     * @brief Download vertex data from GPU to CPU.
     *
     * Issues a synchronous GPU-to-CPU transfer. Safe to call at VISUAL_RATE
     * from a FRAME_ACCURATE metro callback.
     */
    void compute_frame() override;

    /**
     * @brief Downloaded vertex array from the last compute_frame() call.
     */
    [[nodiscard]] const std::vector<Kakshya::MeshVertex>& get_vertices() const
    {
        return m_vertices;
    }

    /**
     * @brief Position of vertex at index, or zero if out of range.
     */
    [[nodiscard]] glm::vec3 get_position(size_t index) const;

    [[nodiscard]] Portal::Graphics::PrimitiveTopology get_primitive_topology() const override
    {
        return Portal::Graphics::PrimitiveTopology::TRIANGLE_LIST;
    }

private:
    std::shared_ptr<Buffers::VKBuffer> m_gpu_buffer;
    std::shared_ptr<Buffers::VKBuffer> m_staging_buffer;
    std::vector<Kakshya::MeshVertex> m_vertices;
};

} // namespace MayaFlux::Nodes::GpuSync
