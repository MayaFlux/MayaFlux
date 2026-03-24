#pragma once

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Kakshya/NDData/VertexAccess.hpp"

namespace MayaFlux::Buffers {

/**
 * @enum GeometryWriteMode
 * @brief Specifies how vertex data should be interpreted for upload
 */
enum class GeometryWriteMode : uint8_t {
    POINT, ///< Interpret vertex data as Nodes::PointVertex
    LINE ///< Interpret vertex data as Nodes::LineVertex
};

/**
 * @class GeometryWriteProcessor
 * @brief Accepts externally-supplied DataVariant and uploads it as vertex
 *        data to a VKBuffer each cycle.
 *
 * Converts the supplied DataVariant to a vertex representation via
 * Kakshya::as_vertex_access() and uploads the result to the attached
 * VKBuffer. The VertexLayout derived from the conversion is set on the
 * buffer so RenderProcessor has full stride and attribute information.
 *
 * Dirty gating: upload occurs only when new data has been supplied via
 * set_data() since the last cycle. No stale re-upload.
 *
 * Thread safety: set_data() and the graphics thread may run concurrently.
 * Lock-free double-buffer swap via atomic_flag ensures the graphics thread
 * never blocks on the supplier thread.
 *
 * Staging: if the attached VKBuffer is device-local a staging buffer is
 * created automatically on on_attach() and reused every cycle.
 */
class MAYAFLUX_API GeometryWriteProcessor : public VKBufferProcessor {
public:
    GeometryWriteProcessor();
    ~GeometryWriteProcessor() override = default;

    /**
     * @brief Supply vertex data for the next cycle.
     * @param variant Any DataVariant type accepted by as_vertex_access().
     *                Conversion to vertex representation is deferred to
     *                processing_function() on the graphics thread.
     */
    void set_data(Kakshya::DataVariant variant);

    /**
     * @brief Returns true if a snapshot has been set and not yet consumed.
     */
    [[nodiscard]] bool has_pending() const noexcept;

    void set_mode(GeometryWriteMode mode) { m_mode = mode; }
    void set_config(const Kakshya::VertexAccessConfig& config) { m_config = config; }

protected:
    void on_attach(const std::shared_ptr<Buffer>& buffer) override;
    void on_detach(const std::shared_ptr<Buffer>& buffer) override;
    void processing_function(const std::shared_ptr<Buffer>& buffer) override;

private:
    std::optional<Kakshya::DataVariant> m_pending;
    std::optional<Kakshya::DataVariant> m_active;
    Kakshya::VertexAccessConfig m_config;
    GeometryWriteMode m_mode { GeometryWriteMode::POINT };

    std::atomic_flag m_dirty;

    std::shared_ptr<VKBuffer> m_staging;
};

} // namespace MayaFlux::Buffers
