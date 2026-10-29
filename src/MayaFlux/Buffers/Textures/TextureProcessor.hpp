#pragma once

#include "MayaFlux/Buffers/VKBuffer.hpp"

namespace MayaFlux::Core {
class VKImage;
}

namespace MayaFlux::Buffers {

class TextureBuffer;

/**
 * @class TextureProcessor
 * @brief Internal processor: handles CPU→GPU transfers for TextureBuffer
 *
 * TextureProcessor is automatically created and attached by TextureBuffer.
 * Users never instantiate or interact with it directly.
 *
 * Responsibilities:
 * - Initialize GPU texture (VKImage)
 * - Upload pixel data to GPU (initial + dirty updates)
 * - Generate and upload quad geometry respecting transform
 * - Detect changes and re-upload as needed
 *
 * All work is invisible to the user. They just modify TextureBuffer
 * (set_pixel_data, set_position, etc.) and it "just works."
 */
class MAYAFLUX_API TextureProcessor : public VKBufferProcessor {
public:
    /** @enum PixelSource
     * @brief Source of pixel data for GPU uploads
     *
     * - BUFFER_STORAGE: Read from m_texture_buffer->m_pixel_data (default)
     * - EXTERNAL_DATA: Call get_variant_source(), expects DataVariant supply
     */
    enum class PixelSource : uint8_t {
        BUFFER_STORAGE,
        EXTERNAL_DATA
    };

    TextureProcessor();
    ~TextureProcessor() override;

protected:
    void on_attach(const std::shared_ptr<Buffer>& buffer) override;
    void on_detach(const std::shared_ptr<Buffer>& buffer) override;
    void processing_function(const std::shared_ptr<Buffer>& buffer) override;

    virtual std::optional<Kakshya::DataVariant> get_variant_source() { return std::nullopt; }

    PixelSource m_pixel_source { PixelSource::BUFFER_STORAGE };
    std::shared_ptr<TextureBuffer> m_texture_buffer;

private:
    // =========================================================================
    // Initialization (on_attach)
    // =========================================================================

    /**
     * Initialize all GPU resources:
     * - VKImage for texture
     * - Vertex buffer with quad geometry
     * - Pixel upload
     */
    void initialize_gpu_resources();

    /**
     * Upload initial quad geometry based on default or custom vertices
     */
    void upload_initial_geometry();

    /**
     * Upload initial pixel data to GPU texture
     */
    void upload_initial_pixels();

    // =========================================================================
    // Per-Frame Updates (processing_function)
    // =========================================================================

    /**
     * Regenerate quad vertices if transform changed, upload if needed
     */
    void update_geometry_if_dirty();

    /**
     * Re-upload pixels to GPU if they changed
     */
    void update_pixels_if_dirty();

    // =========================================================================
    // GPU Resource Creation
    // =========================================================================

    /**
     * Create VKImage for texture storage
     */
    std::shared_ptr<Core::VKImage> create_gpu_texture();

    /**
     * Generate quad vertices respecting current transform
     * Handles both default quad and custom vertices
     */
    void generate_quad_vertices(std::vector<uint8_t>& out_bytes);
};

} // namespace MayaFlux::Buffers
