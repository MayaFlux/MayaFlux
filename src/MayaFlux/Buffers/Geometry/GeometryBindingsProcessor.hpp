#pragma once

#include "MayaFlux/Buffers/VKBuffer.hpp"

namespace MayaFlux::Nodes {
class GeometryWriterNode;
}

namespace MayaFlux::Buffers {

/**
 * @class GeometryBindingsProcessor
 * @brief BufferProcessor that uploads geometry node data to GPU vertex buffers
 *
 * Manages bindings between GeometryWriterNode instances (CPU-side) and GPU
 * vertex buffers. Each frame, reads vertex data from nodes and uploads to
 * corresponding GPU buffers via staging buffers.
 *
 * Behavior:
 * - Uploads ALL bound geometries to their target vertex buffers
 * - If target is device-local: uses staging buffer (auto-created)
 * - If target is host-visible: direct upload (no staging)
 * - If attached buffer is one of the targets: uploads its geometry
 * - If attached buffer is NOT a target: uploads first geometry to it
 *
 * Usage:
 *   auto vertex_buffer = std::make_shared<VKBuffer>(
 *       1000 * sizeof(Vertex),
 *       VKBuffer::Usage::VERTEX_BUFFER);
 *
 *   auto processor = std::make_shared<GeometryBindingsProcessor>();
 *   processor->bind_geometry_node("particles", particle_node, vertex_buffer);
 *
 *   vertex_buffer->set_default_processor(processor);
 *   vertex_buffer->process_default();  // Uploads geometry
 */
class MAYAFLUX_API GeometryBindingsProcessor : public VKBufferProcessor {
public:
    struct GeometryBinding {
        std::shared_ptr<Nodes::GeometryWriterNode> node;
        std::shared_ptr<VKBuffer> gpu_vertex_buffer; // Target vertex buffer
        std::shared_ptr<VKBuffer> staging_buffer; // Staging (only if device-local)
    };

    /**
     * @brief Bind a geometry node to a GPU vertex buffer
     * @param name Logical name for this binding
     * @param node GeometryWriterNode to read vertices from
     * @param vertex_buffer GPU vertex buffer to upload to
     *
     * If vertex_buffer is device-local, a staging buffer is automatically created.
     * If vertex_buffer is host-visible, no staging is needed.
     */
    void bind_geometry_node(
        const std::string& name,
        const std::shared_ptr<Nodes::GeometryWriterNode>& node,
        const std::shared_ptr<VKBuffer>& vertex_buffer);

    /**
     * @brief Remove a geometry binding
     * @param name Name of binding to remove
     */
    void unbind_geometry_node(const std::string& name);

    /**
     * @brief Check if a binding exists
     * @param name Binding name
     * @return True if binding exists
     */
    [[nodiscard]] bool has_binding(const std::string& name) const;

    /**
     * @brief Get all binding names
     * @return Vector of binding names
     */
    [[nodiscard]] std::vector<std::string> get_binding_names() const;

    /**
     * @brief Get number of active bindings
     * @return Binding count
     */
    [[nodiscard]] size_t get_binding_count() const;

    /**
     * @brief Get a specific binding
     * @param name Binding name
     * @return Optional containing binding if exists
     */
    [[nodiscard]] std::optional<GeometryBinding> get_binding(const std::string& name) const;

    /**
     * @brief BufferProcessor interface - uploads all bound geometries
     * @param buffer The buffer this processor is attached to
     *
     * Uploads all geometry nodes to their target vertex buffers.
     * Uses staging buffers for device-local targets.
     */
    void processing_function(std::shared_ptr<Buffer> buffer) override;

private:
    std::unordered_map<std::string, GeometryBinding> m_bindings;
};

} // namespace MayaFlux::Buffers
