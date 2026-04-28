#pragma once

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Portal/Graphics/GraphicsUtils.hpp"

namespace MayaFlux::Core {
class Window;
}

namespace MayaFlux::Buffers {
class RenderProcessor;

/**
 * @class FormaBuffer
 * @brief VKBuffer for 2D screen-space Forma geometry.
 *
 * Encodes the invariants that hold for all Forma geometry:
 *   - No depth test, no depth write, no view transform.
 *   - Alpha blending enabled on the RenderProcessor.
 *   - Topology fixed at construction (determines default shaders).
 *
 * Provides write_bytes() as the clean write interface for Mapped::sync(),
 * replacing the raw set_data path on plain VKBuffer.
 *
 * Usage:
 * @code
 * auto buf = std::make_shared<FormaBuffer>(
 *     initial_capacity_bytes,
 *     Portal::Graphics::PrimitiveTopology::TRIANGLE_STRIP);
 * buf->setup_processors(ProcessingToken::GRAPHICS_BACKEND);
 * buf->setup_rendering({ .target_window = window });
 * @endcode
 */
class MAYAFLUX_API FormaBuffer : public Buffers::VKBuffer {
public:
    /**
     * @brief Construct a FormaBuffer.
     * @param capacity_bytes  Initial buffer capacity. Size the buffer for the
     *                        maximum expected vertex count from the geometry
     *                        function. write_bytes() will warn if exceeded.
     * @param topology        Primitive topology. Determines default shaders
     *                        selected in setup_rendering().
     */
    FormaBuffer(
        size_t capacity_bytes,
        Portal::Graphics::PrimitiveTopology topology);

    ~FormaBuffer() override = default;

    FormaBuffer(const FormaBuffer&) = delete;
    FormaBuffer& operator=(const FormaBuffer&) = delete;
    FormaBuffer(FormaBuffer&&) = delete;
    FormaBuffer& operator=(FormaBuffer&&) = delete;

    /**
     * @brief Register with the graphics subsystem.
     *        Creates a BufferUploadProcessor as default processor.
     */
    void setup_processors(ProcessingToken token) override;

    /**
     * @brief Attach a RenderProcessor targeting the given window.
     *
     * Selects default shaders based on topology set at construction.
     * Always enables alpha blending and disables depth testing.
     * Caller may override shaders via config fields.
     */
    void setup_rendering(const RenderConfig& config);

    /**
     * @brief Write vertex bytes into the buffer.
     *
     * Called by Mapped::sync() each time the geometry function produces
     * new vertex data. Copies bytes into the mapped host-visible region
     * and marks the dirty range for flush on the next graphics tick.
     *
     * @param bytes Raw interleaved vertex bytes. Must not exceed the
     *              capacity set at construction.
     */
    void write_bytes(const std::vector<uint8_t>& bytes);

    [[nodiscard]] std::shared_ptr<Buffers::RenderProcessor> get_render_processor() const override
    {
        return m_render_processor;
    }

    [[nodiscard]] Portal::Graphics::PrimitiveTopology topology() const { return m_topology; }

private:
    Portal::Graphics::PrimitiveTopology m_topology;
    std::shared_ptr<Buffers::RenderProcessor> m_render_processor;
};

} // namespace MayaFlux::Buffers
