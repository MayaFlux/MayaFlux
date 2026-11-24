#pragma once

#include "MayaFlux/Buffers/VKBuffer.hpp"

namespace MayaFlux::Kakshya {
struct MeshData;
}

namespace MayaFlux::Buffers {

class MeshBuffer;

/**
 * @class MeshProcessor
 * @brief Default processor for MeshBuffer: handles CPU-to-GPU upload of
 *        vertex and index data with selective dirty-flag re-upload.
 *
 * The GPU buffers (VERTEX + INDEX) and staging are allocated on the first
 * cycle that has valid geometry, which is on_attach for a buffer made from a
 * mesh and a later cycle for one that waits for write_frame. Both streams are
 * uploaded then, and set_index_resources() links the index buffer to the
 * vertex buffer in VKBufferResources with the live byte size of the indices,
 * so a smaller mesh never draws the tail of a larger one. This mirrors
 * GeometryBindingsProcessor::bind_geometry_node() but is driven directly from
 * MeshBuffer's MeshData rather than from a node.
 *
 * On processing_function: adopts the newest write_frame frame first, uploading
 * the indices only when they differ from the ones held. Then checks
 * MeshBuffer::m_vertices_dirty and MeshBuffer::m_indices_dirty independently,
 * re-uploads whichever stream has changed and clears the flag. No-op when both
 * are clean, so the per-frame cost when a mesh is static is a single atomic
 * load per flag. While a field pass is wired the vertices are restored every
 * cycle.
 *
 * Users never instantiate this directly. MeshBuffer creates and sets it
 * as its default processor inside setup_processors().
 *
 * Deformation path: call MeshBuffer::set_vertex_data() or
 * MeshBuffer::set_index_data(), which set the respective dirty flag.
 * MeshProcessor picks up the change on the next graphics cycle.
 */
class MAYAFLUX_API MeshProcessor : public VKBufferProcessor {
public:
    MeshProcessor();
    ~MeshProcessor() override = default;

protected:
    void on_attach(const std::shared_ptr<Buffer>& buffer) override;
    void on_detach(const std::shared_ptr<Buffer>& buffer) override;
    void processing_function(const std::shared_ptr<Buffer>& buffer) override;

private:
    std::shared_ptr<MeshBuffer> m_mesh_buffer;
    std::shared_ptr<const Kakshya::MeshData> m_frame;
    std::optional<uint32_t> m_layout_count;

    std::shared_ptr<VKBuffer> m_gpu_index_buffer;
    std::shared_ptr<VKBuffer> m_vertex_staging;
    std::shared_ptr<VKBuffer> m_index_staging;

    void adopt_frame(const std::shared_ptr<const Kakshya::MeshData>& frame);
    void upload_pending(const std::shared_ptr<MeshBuffer>& buf);
    void sync_layout(const std::shared_ptr<MeshBuffer>& buf);
    void allocate_gpu_buffers(const std::shared_ptr<MeshBuffer>& buf);
    void upload_vertices(const std::shared_ptr<MeshBuffer>& buf);
    void upload_indices(const std::shared_ptr<MeshBuffer>& buf);
    void link_index_resources();
};

} // namespace MayaFlux::Buffers
