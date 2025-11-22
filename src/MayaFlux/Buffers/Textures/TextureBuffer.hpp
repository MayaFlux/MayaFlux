#pragma once

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/IO/ImageReader.hpp"
#include "MayaFlux/Portal/Graphics/TextureLoom.hpp"

namespace MayaFlux::Buffers {

class TextureProcessor;

/**
 * @class TextureBuffer
 * @brief Specialized buffer for image/texture data
 *
 * Stores pixel data in CPU-accessible VKBuffer and manages corresponding
 * GPU VKImage. Designed for both static loaded images and dynamic procedural
 * textures that update per-frame.
 *
 * Philosophy:
 * - Images are just pixel buffers - process like audio samples
 * - Automatic CPU→GPU synchronization via TextureProcessor
 * - Static images (loaded once) and dynamic images (update per frame) use same API
 *
 * Usage (Static Image):
 *   auto img_reader = IO::ImageReader();
 *   img_reader.open("texture.png");
 *   auto tex_buffer = img_reader->create_buffer();
 *   register_graphics_buffer(tex_buffer, ProcessingToken::GRAPHICS_BACKEND);
 *
 *   // Bind to shader
 *   render_processor->bind_texture(0, tex_buffer->get_texture());
 *
 * Usage (Dynamic/Procedural):
 *   auto tex = std::make_shared<TextureBuffer>(512, 512, ImageFormat::RGBA8);
 *
 *   // Modify pixels every frame
 *   schedule_metro(0.016, [tex]() {
 *       auto accessor = tex->get_pixel_accessor();
 *       for (int i = 0; i < pixels; i++) {
 *           accessor[i] = compute_procedural_pixel(i);
 *       }
 *       tex->mark_dirty(); // TextureProcessor will re-upload
 *   });
 */
class MAYAFLUX_API TextureBuffer : public VKBuffer {
public:
    /**
     * @brief Create texture buffer with dimensions
     * @param width Texture width in pixels
     * @param height Texture height in pixels
     * @param format Pixel format
     * @param initial_pixel_data Optional initial pixel data (nullptr = uninitialized)
     *
     * The VKBuffer itself contains fullscreen quad vertices.
     * The texture pixels are stored separately and uploaded to VKImage.
     */
    TextureBuffer(
        uint32_t width,
        uint32_t height,
        Portal::Graphics::ImageFormat format,
        const void* initial_pixel_data = nullptr);

    ~TextureBuffer() override = default;

    void setup_processors(ProcessingToken token) override;

    // === Metadata Access ===
    [[nodiscard]] uint32_t get_width() const { return m_width; }
    [[nodiscard]] uint32_t get_height() const { return m_height; }
    [[nodiscard]] Portal::Graphics::ImageFormat get_format() const { return m_format; }

    // === GPU Texture Access ===
    [[nodiscard]] std::shared_ptr<Core::VKImage> get_texture() const { return m_gpu_texture; }
    [[nodiscard]] bool has_texture() const { return m_gpu_texture != nullptr; }

    // === Processor Access ===
    [[nodiscard]] std::shared_ptr<TextureProcessor> get_texture_processor() const
    {
        return m_texture_processor;
    }

    // === Pixel Data Access ===
    [[nodiscard]] const std::vector<uint8_t>& get_pixel_data() const { return m_pixel_data; }

    void mark_texture_dirty() { m_texture_dirty = true; }
    [[nodiscard]] bool is_texture_dirty() const { return m_texture_dirty; }
    void clear_dirty_flag() { m_texture_dirty = false; }

private:
    uint32_t m_width;
    uint32_t m_height;
    Portal::Graphics::ImageFormat m_format;

    // Texture pixel data (separate from VKBuffer vertex data)
    std::vector<uint8_t> m_pixel_data;

    // GPU texture (managed by TextureProcessor)
    std::shared_ptr<Core::VKImage> m_gpu_texture;
    bool m_texture_dirty = true;

    std::shared_ptr<TextureProcessor> m_texture_processor;

    static size_t calculate_buffer_size(uint32_t width, uint32_t height,
        Portal::Graphics::ImageFormat format);

    /**
     * @brief Calculate size for fullscreen quad vertices
     */
    static size_t calculate_quad_size();

    /**
     * @brief Initialize quad vertex data
     */
    void initialize_quad_vertices();

    friend class TextureProcessor;
};

} // namespace MayaFlux::Buffers
