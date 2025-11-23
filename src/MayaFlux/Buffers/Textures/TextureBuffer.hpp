#pragma once

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Nodes/Graphics/TextureNode.hpp"
#include "TextureBindingsProcessor.hpp"

namespace MayaFlux::Buffers {

/**
 * @class TextureBuffer
 * @brief Specialized buffer for generative texture/pixel data from TextureNode
 *
 * Automatically handles CPU→GPU upload of procedurally generated pixels.
 * Designed for algorithmic image generation: fractals, noise fields,
 * audio visualizations, data mappings, etc.
 *
 * Philosophy:
 * - Textures are GENERATED, not loaded from files
 * - Data flows from algorithm → GPU → screen/shader
 * - Pixels are just numbers - transform and visualize data freely
 *
 * Usage:
 *   class PerlinNoise : public TextureNode {
 *       void compute_frame() override {
 *           for (uint32_t y = 0; y < m_height; y++) {
 *               for (uint32_t x = 0; x < m_width; x++) {
 *                   float noise_val = perlin(x, y, m_time);
 *                   set_pixel(x, y, noise_val, noise_val, noise_val, 1.0f);
 *               }
 *           }
 *           m_time += 0.016f;
 *       }
 *   };
 *
 *   auto noise = std::make_shared<PerlinNoise>(512, 512);
 *   auto buffer = std::make_shared<TextureBuffer>(noise);
 *
 *   auto render = std::make_shared<RenderProcessor>(config);
 *   buffer->add_processor(render) | Graphics;
 */
class MAYAFLUX_API TextureBuffer : public VKBuffer {
public:
    /**
     * @brief Create texture buffer from generative node
     * @param node TextureNode that generates pixels each frame
     * @param binding_name Logical name for this texture binding (default: "texture")
     *
     * Buffer size is automatically calculated from texture dimensions:
     * width * height * 4 channels * sizeof(float)
     */
    explicit TextureBuffer(
        std::shared_ptr<Nodes::GpuSync::TextureNode> node,
        std::string binding_name = "texture");

    ~TextureBuffer() override = default;

    /**
     * @brief Initialize the buffer and its processors
     */
    void initialize();

    /**
     * @brief Get the texture node driving this buffer
     */
    [[nodiscard]] std::shared_ptr<Nodes::GpuSync::TextureNode> get_texture_node() const
    {
        return m_texture_node;
    }

    /**
     * @brief Get the bindings processor managing uploads
     */
    [[nodiscard]] std::shared_ptr<TextureBindingsProcessor> get_bindings_processor() const
    {
        return m_bindings_processor;
    }

    /**
     * @brief Get the logical binding name
     */
    [[nodiscard]] const std::string& get_binding_name() const
    {
        return m_binding_name;
    }

    /**
     * @brief Get texture dimensions
     */
    [[nodiscard]] std::pair<uint32_t, uint32_t> get_dimensions() const
    {
        return m_texture_node ? std::make_pair(m_texture_node->get_width(), m_texture_node->get_height()) : std::make_pair(0U, 0U);
    }

    /**
     * @brief Trigger pixel computation on the node
     *
     * Calls node->compute_frame() to regenerate pixels.
     * Useful for explicit frame updates when not using domain-driven processing.
     */
    void update_texture()
    {
        if (m_texture_node) {
            m_texture_node->compute_frame();
        }
    }

private:
    std::shared_ptr<Nodes::GpuSync::TextureNode> m_texture_node;
    std::shared_ptr<TextureBindingsProcessor> m_bindings_processor;
    std::string m_binding_name;

    /**
     * @brief Calculate texture buffer size from node dimensions
     */
    static size_t calculate_buffer_size(const std::shared_ptr<Nodes::GpuSync::TextureNode>& node);
};

} // namespace MayaFlux::Buffers
