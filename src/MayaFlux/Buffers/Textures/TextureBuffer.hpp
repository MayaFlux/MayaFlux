#pragma once

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Portal/Graphics/TextureLoom.hpp"

namespace MayaFlux::Buffers {

class TextureProcessor;

/**
 * @class TextureBuffer
 * @brief A hybrid buffer managing both a textured quad geometry and its pixel data.
 *
 * TextureBuffer serves a dual purpose:
 * 1. Geometry: It acts as a VKBuffer containing vertex data for a 2D quad (Position + UVs).
 *    This geometry can be transformed (translated, scaled, rotated) or customized.
 * 2. Texture: It manages raw pixel data in system memory and synchronizes it with a
 *    GPU-resident VKImage via the TextureProcessor.
 *
 * Unlike a raw texture resource, this class represents a "renderable sprite" or "surface".
 * The vertex data is dynamic and updates automatically when transforms change.
 * The pixel data can be static (loaded once) or dynamic (procedural/video), with
 * dirty-flag tracking to minimize bus traffic.
 *
 * Key Features:
 * - Automatic quad generation based on dimensions.
 * - Built-in 2D transform support (Position, Scale, Rotation) affecting vertex positions.
 * - CPU-side pixel storage with automatic upload to GPU VKImage on change.
 * - Support for custom vertex geometry (e.g., for non-rectangular sprites).
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

    // =========================================================================
    // Texture Metadata
    // =========================================================================

    [[nodiscard]] uint32_t get_width() const { return m_width; }
    [[nodiscard]] uint32_t get_height() const { return m_height; }
    [[nodiscard]] Portal::Graphics::ImageFormat get_format() const { return m_format; }

    // =========================================================================
    // GPU Texture Access
    // =========================================================================

    /**
     * @brief Get GPU texture image
     * Suitable for binding to shaders via RenderProcessor::bind_texture()
     */
    [[nodiscard]] std::shared_ptr<Core::VKImage> get_texture() const { return m_gpu_texture; }
    [[nodiscard]] bool has_texture() const { return m_gpu_texture != nullptr; }
    // === Processor Access ===
    [[nodiscard]] std::shared_ptr<TextureProcessor> get_texture_processor() const
    {
        return m_texture_processor;
    }

    /**
     * @brief Replace pixel data
     * @param data Pointer to pixel data (size must match width*height*channels)
     * @param size Size in bytes
     *
     * Marks texture as dirty. TextureProcessor will re-upload on next frame.
     */
    void set_pixel_data(const void* data, size_t size);

    /**
     * @brief Mark pixel data as changed
     * Use this if you modify pixel data in-place without calling set_pixel_data()
     */
    void mark_pixels_dirty();

    // =========================================================================
    // Display Transform
    // =========================================================================

    /**
     * @brief Set screen position (NDC or pixel coords depending on rendering setup)
     * @param x X position
     * @param y Y position
     *
     * Marks geometry as dirty. TextureProcessor will recalculate vertices on next frame.
     */
    void set_position(float x, float y);

    /**
     * @brief Set display size
     * @param width Width in pixels/units
     * @param height Height in pixels/units
     *
     * Marks geometry as dirty.
     */
    void set_scale(float width, float height);

    /**
     * @brief Set rotation around center
     * @param angle_radians Rotation in radians
     *
     * Marks geometry as dirty.
     */
    void set_rotation(float angle_radians);

    [[nodiscard]] glm::vec2 get_position() const { return m_position; }
    [[nodiscard]] glm::vec2 get_scale() const { return m_scale; }
    [[nodiscard]] float get_rotation() const { return m_rotation; }

    // =========================================================================
    // Advanced: Custom Geometry
    // =========================================================================

    /**
     * @brief Use custom vertex geometry instead of default quad
     * @param vertices Custom quad vertices (must be 4 vertices with position + texcoord)
     *
     * For power users who want non-rectangular meshes or different vertex layouts.
     * Marks geometry as dirty.
     */
    struct QuadVertex {
        glm::vec3 position;
        glm::vec2 texcoord;
    };

    void set_custom_vertices(const std::vector<QuadVertex>& vertices);

    /**
     * @brief Reset to default fullscreen quad
     * Uses position and scale to generate quad geometry.
     */
    void use_default_quad();

    [[nodiscard]] const std::vector<uint8_t>& get_pixel_data() const { return m_pixel_data; }

    void mark_texture_dirty() { m_texture_dirty = true; }
    [[nodiscard]] bool is_texture_dirty() const { return m_texture_dirty; }
    void clear_dirty_flag() { m_texture_dirty = false; }

private:
    friend class TextureProcessor;

    // Texture metadata
    uint32_t m_width;
    uint32_t m_height;
    Portal::Graphics::ImageFormat m_format;

    // Pixel data
    std::vector<uint8_t> m_pixel_data;
    bool m_texture_dirty = true;

    // Display transform
    glm::vec2 m_position { 0.0F, 0.0F };
    glm::vec2 m_scale { 1.0F, 1.0F };
    float m_rotation { 0.0F };
    bool m_geometry_dirty = true;

    // Geometry
    std::vector<uint8_t> m_vertex_bytes;
    bool m_uses_custom_vertices = false;

    // GPU resources
    std::shared_ptr<Core::VKImage> m_gpu_texture;
    std::shared_ptr<TextureProcessor> m_texture_processor;

    // Geometry generation
    void generate_default_quad();
    void generate_quad_with_transform();
    static size_t calculate_quad_vertex_size();
};

} // namespace MayaFlux::Buffers
