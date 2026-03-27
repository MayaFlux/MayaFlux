#pragma once

#include "MayaFlux/Buffers/VKBuffer.hpp"

namespace MayaFlux::Buffers {

class MeshBuffer;

/**
 * @class MeshProcessor
 * @brief Default processor for MeshBuffer: handles CPU-to-GPU upload of
 *        vertex and index data with selective dirty-flag re-upload.
 *
 * On on_attach: allocates both GPU buffers (VERTEX + INDEX), creates staging
 * buffers, performs the initial upload of both streams, and calls
 * set_index_resources() on the vertex buffer to link the two Vulkan handles
 * in VKBufferResources. This mirrors GeometryBindingsProcessor::bind_geometry_node()
 * but is driven directly from MeshBuffer's MeshData rather than from a node.
 *
 * On processing_function: checks MeshBuffer::m_vertices_dirty and
 * MeshBuffer::m_indices_dirty independently. Re-uploads whichever stream
 * has changed and clears the flag. No-op when both are clean, so the
 * per-frame cost when a mesh is static is a single atomic load per flag.
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

    std::shared_ptr<VKBuffer> m_gpu_index_buffer;
    std::shared_ptr<VKBuffer> m_vertex_staging;
    std::shared_ptr<VKBuffer> m_index_staging;

    void allocate_gpu_buffers(const std::shared_ptr<MeshBuffer>& buf);
    void upload_vertices(const std::shared_ptr<MeshBuffer>& buf);
    void upload_indices(const std::shared_ptr<MeshBuffer>& buf);
    void link_index_resources();
};

} // namespace MayaFlux::Buffers
