#pragma once

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Portal/Graphics/GraphicsUtils.hpp"

namespace MayaFlux::Buffers {

/**
 * @class FormaProcessor
 * @brief Default processor for FormaBuffer.
 *
 * Owns the CPU-to-GPU upload cycle for Forma geometry. Accepts raw
 * interleaved vertex bytes via set_bytes(), stores them behind an
 * atomic dirty flag, and uploads to the device-local buffer each
 * graphics tick via a persistent staging buffer. Updates vertex_count
 * in the buffer's VertexLayout after every upload so RenderProcessor
 * draws the correct number of vertices.
 *
 * Mirrors GeometryWriteProcessor in structure. The topology is fixed
 * at construction to derive the correct per-vertex stride for the
 * vertex_count calculation.
 */
class MAYAFLUX_API FormaProcessor : public VKBufferProcessor {
public:
    explicit FormaProcessor(Portal::Graphics::PrimitiveTopology topology);
    ~FormaProcessor() override = default;

    /**
     * @brief Supply new vertex bytes for the next graphics tick.
     *
     * Thread-safe via atomic_flag. Called from Mapped::sync() on the
     * scheduler tick. The bytes are swapped into the active slot on the
     * next processing_function call.
     *
     * @param bytes Raw interleaved vertex bytes matching the topology's
     *              vertex type (PointVertex, LineVertex, or MeshVertex).
     */
    void set_bytes(std::vector<uint8_t> bytes);

    [[nodiscard]] bool has_pending() const noexcept;

protected:
    void on_attach(const std::shared_ptr<Buffer>& buffer) override;
    void on_detach(const std::shared_ptr<Buffer>& buffer) override;
    void processing_function(const std::shared_ptr<Buffer>& buffer) override;

private:
    Portal::Graphics::PrimitiveTopology m_topology;
    uint32_t m_stride { 0 };

    std::vector<uint8_t> m_pending;
    std::vector<uint8_t> m_active;
    std::atomic_flag m_dirty;

    std::shared_ptr<VKBuffer> m_staging;

    [[nodiscard]] uint32_t vertex_count(size_t byte_count) const noexcept;
};

} // namespace MayaFlux::Buffers
