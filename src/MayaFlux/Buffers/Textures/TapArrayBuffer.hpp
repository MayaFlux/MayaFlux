#pragma once

#include "TextureBuffer.hpp"

#include <array>
#include <span>

namespace MayaFlux::Buffers {

/**
 * @class TapArrayBuffer
 * @brief A TextureBuffer whose image is a stack of layers, composed in one draw.
 *
 * Each layer is a frame of the same size and format. Submit a frame to a layer
 * and the buffer uploads the whole stack once per cycle. A layer that is not
 * submitted keeps its last frame. The layers are bound to the fragment shader as
 * a sampler2DArray named tapArray, so what the stack means is the shader's
 * choice: a blend, a median, a slit-scan, a delay map, a difference, a mosaic.
 *
 * The shaders shipped in data/shaders share one push constant block: the layer
 * count, a mode number whose meaning belongs to the shader, and a weight per
 * layer. Choose a shader with setup_rendering({ .fragment_shader = ... }); the
 * default is tap_array.frag. Extra textures such as a delay map go in
 * RenderConfig::additional_textures and bind from binding 2. A shader with its
 * own push constants replaces the block through set_push_constants().
 *
 * Rendering, transform and window targeting are those of TextureBuffer.
 * Reading the buffer back through a texture read returns every layer and is not
 * meant for this class. Submit from the graphics frame clock or the thread that
 * processes the buffer.
 */
class MAYAFLUX_API TapArrayBuffer : public TextureBuffer {
public:
    /** @brief Most layers the shipped shaders read. */
    static constexpr uint32_t MAX_LAYERS = 30;

    /**
     * @param width  Width of every layer in pixels.
     * @param height Height of every layer in pixels.
     * @param format Pixel format of every layer.
     * @param layers Number of layers, between 1 and MAX_LAYERS.
     */
    TapArrayBuffer(
        uint32_t width,
        uint32_t height,
        Portal::Graphics::ImageFormat format,
        uint32_t layers);

    void setup_processors(ProcessingToken token) override;

    void setup_rendering(const RenderConfig& config) override;

    /** @brief Number of layers. */
    [[nodiscard]] uint32_t get_layer_count() const { return m_layers; }

    /** @brief Bytes in one layer. */
    [[nodiscard]] size_t get_layer_byte_size() const { return m_layer_bytes; }

    /**
     * @brief Replace the frame of one layer.
     * @param layer  Layer index.
     * @param pixels Exactly get_layer_byte_size() bytes.
     * @return True if the frame was stored.
     */
    bool submit_layer(uint32_t layer, std::span<const uint8_t> pixels);

    /**
     * @brief Set the weight of one layer.
     *
     * What a weight does is up to the shader. In the shipped shaders a weight of
     * zero or less hides the layer.
     *
     * @param layer  Layer index.
     * @param weight Weight passed to the shader.
     */
    void set_weight(uint32_t layer, float weight);

    /**
     * @brief Set the weights of the first layers in order.
     * @param weights Weights, at most get_layer_count() are used.
     */
    void set_weights(std::span<const float> weights);

    /**
     * @brief Set the mode number passed to the shader.
     *
     * Each shader interprets it; see the comment above the push constant block
     * of the shader in use.
     */
    void set_mode(uint32_t mode);

    /**
     * @brief Replace the default push constants with the shader's own.
     *
     * After this call the layer count, mode and weights are no longer sent. The
     * data is resent unchanged every frame until it is replaced.
     *
     * @param data Bytes of the push constant block.
     * @param size Size of the block in bytes.
     */
    void set_push_constants(const void* data, size_t size);

    /** @brief Replace the default push constants with a block of type T. */
    template <typename T>
    void set_push_constants(const T& block)
    {
        set_push_constants(&block, sizeof(T));
    }

private:
    struct Push {
        uint32_t layer_count {};
        uint32_t mode {};
        std::array<float, MAX_LAYERS> weights {};
    };

    uint32_t m_layers;
    size_t m_layer_bytes;
    Push m_push;
    std::vector<uint8_t> m_custom_push;

    void ensure_array();
    void flush_push();
};

}
