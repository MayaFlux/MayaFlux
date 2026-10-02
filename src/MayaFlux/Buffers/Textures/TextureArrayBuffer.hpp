#pragma once

#include "TextureBuffer.hpp"

#include "MayaFlux/Kakshya/Source/DynamicVideoStream.hpp"
#include "MayaFlux/Portal/Graphics/TextureLoom.hpp"

namespace MayaFlux::Core {
class VKImage;
}

namespace MayaFlux::Buffers {

/**
 * @class TextureArrayBuffer
 * @brief A TextureBuffer whose image is a stack of layers, composed in one draw.
 *
 * Each layer is a frame of the same size and format, bound to the fragment shader
 * as a sampler2DArray named textureArray. What the stack means is the shader's
 * choice: a blend, a median, a slit-scan, a delay map, a difference, a mosaic.
 * A layer that is not written keeps its last frame, so sources at different rates
 * can share one array.
 *
 * A layer is written from raw pixels, an ImageData or a VKImage, by index or at a
 * write head that advances on every push_layer. A frame whose extent differs from
 * the array is dropped, or fitted into the layer with the FitMode chosen by
 * set_fit. A fit leaves any area it does not cover with the layer's previous
 * contents. Raw pixels must already match the layer, and are written through host
 * storage that is uploaded once per cycle. The first VKImage, or an ImageData that
 * needs converting, moves the buffer to writing each layer straight into the
 * image, which keeps layers written either way intact. Every such write completes
 * before the call returns.
 *
 * A fit that does not cover the whole layer clears the layer first, so a smaller
 * part replacing a larger one leaves nothing of it behind. Each layer may have its
 * own fit through set_layer_fit.
 *
 * The shipped shader, texture_array.frag, takes a push constant block of the layer
 * count, a mode number whose meaning belongs to the shader, and a weight for each
 * of the first WEIGHT_SLOTS layers. Choose a shader with
 * setup_rendering({ .fragment_shader = ... }). Extra textures such as a delay map,
 * or a second array, go in RenderConfig::additional_textures and bind from binding
 * 2 by the sampler's name. A shader with its own push constants replaces the block
 * through set_push_constants().
 *
 * Two vec4 per layer reach the shader beyond the push constants once
 * enable_layer_data is called, or set_layer_params is, before the first frame. A
 * shader reads them with
 * `layout(set = 1, binding = 0) readonly buffer LayerData { vec4 layerData[]; };`
 * where layerData[2 * layer] holds the params, which are the caller's to use for
 * placement, scale, a mask or anything else, and layerData[2 * layer + 1] holds the
 * timing, which a Chimera fills for the layers it feeds. A shader that does not
 * declare the buffer must not enable it.
 *
 * Rendering, transform and window targeting are those of TextureBuffer. Reading
 * the buffer back through a texture read returns every layer and is not meant for
 * this class. Write from the graphics frame clock or the thread that processes the
 * buffer.
 */
class MAYAFLUX_API TextureArrayBuffer : public TextureBuffer {
public:
    /** @brief Layers the shipped push constant block carries weights for. */
    static constexpr uint32_t WEIGHT_SLOTS = 30;

    /**
     * @param width  Width of every layer in pixels.
     * @param height Height of every layer in pixels.
     * @param format Pixel format of every layer.
     * @param layers Number of layers, at least 1.
     */
    TextureArrayBuffer(
        uint32_t width,
        uint32_t height,
        Portal::Graphics::ImageFormat format,
        uint32_t layers);

    /**
     * @brief Build from a stream spec: width, height and format of every layer,
     *        with ring_frames as the layer count. The frame rate is not used.
     * @param spec Extent, format and layer count.
     * @param fit  How a frame of another extent is fitted, or nullopt to drop it.
     */
    explicit TextureArrayBuffer(
        const Kakshya::VideoStreamSpec& spec,
        std::optional<Portal::Graphics::FitMode> fit = Portal::Graphics::FitMode::STRETCH);

    void setup_processors(ProcessingToken token) override;

    void setup_rendering(const RenderConfig& config) override;

    /** @brief Number of layers. */
    [[nodiscard]] uint32_t get_layer_count() const { return m_layers; }

    /** @brief Bytes in one layer. */
    [[nodiscard]] size_t get_layer_byte_size() const { return m_layer_bytes; }

    /** @brief The layer the next push_layer writes. */
    [[nodiscard]] uint32_t get_write_head() const { return m_write_head; }

    /**
     * @brief Choose what happens to a frame whose extent differs from the layer.
     * @param fit    How the frame is fitted, or nullopt to drop it.
     * @param filter Filter used when the fit scales the frame.
     */
    void set_fit(std::optional<Portal::Graphics::FitMode> fit,
        Portal::Graphics::FilterMode filter = Portal::Graphics::FilterMode::LINEAR)
    {
        m_fit = fit;
        m_filter = filter;
    }

    /**
     * @brief Give one layer its own fit, replacing the one from set_fit for that layer.
     * @param layer Layer index.
     * @param fit   How a frame of another extent is fitted, or nullopt to drop it.
     */
    void set_layer_fit(uint32_t layer, std::optional<Portal::Graphics::FitMode> fit)
    {
        m_layer_fit[layer] = fit;
    }

    /**
     * @brief Create the per-layer buffer the shader reads as layerData.
     *
     * Call it before the first frame, and only for a shader that declares the
     * buffer. Calling it again does nothing.
     */
    void enable_layer_data();

    /** @brief Whether the per-layer buffer exists. */
    [[nodiscard]] bool has_layer_data() const { return m_layer_data != nullptr; }

    /**
     * @brief Set the params of one layer, layerData[2 * layer]. Enables layer data.
     * @param layer  Layer index.
     * @param values Four floats whose meaning belongs to the shader.
     */
    void set_layer_params(uint32_t layer, const glm::vec4& values);

    /**
     * @brief Set the timing of one layer, layerData[2 * layer + 1]. Enables layer data.
     *
     * A Chimera fills it for the layers it feeds, as age in seconds, frames behind
     * the live head, ring position and 1 once the layer has been fed.
     *
     * @param layer  Layer index.
     * @param values Four floats.
     */
    void set_layer_timing(uint32_t layer, const glm::vec4& values);

    /**
     * @brief Replace the frame of one layer with raw pixels.
     * @param layer  Layer index.
     * @param pixels Exactly get_layer_byte_size() bytes.
     * @return True if the frame was stored.
     */
    bool submit_layer(uint32_t layer, std::span<const uint8_t> pixels);

    /**
     * @brief Replace the frame of one layer with an image.
     *
     * An image of the layer's extent and format is stored as raw pixels. Any other
     * is uploaded and fitted, or dropped when set_fit chose nullopt.
     *
     * @return True if the frame was stored.
     */
    bool submit_layer(uint32_t layer, const Kakshya::ImageData& image);

    /**
     * @brief Replace the frame of one layer with a GPU image, with no host read.
     *
     * The image is blitted into the layer, converting format and fitting its extent
     * as set_fit says. It must not be this buffer's own image.
     *
     * @return True if the frame was stored.
     */
    bool submit_layer(uint32_t layer, const std::shared_ptr<Core::VKImage>& image);

    /**
     * @brief Write a frame at the write head and advance it, wrapping at the last
     *        layer. The head does not move if the frame was not stored.
     * @tparam Source Anything submit_layer accepts.
     * @return True if the frame was stored.
     */
    template <typename Source>
    bool push_layer(const Source& source)
    {
        if (!submit_layer(m_write_head, source)) {
            return false;
        }
        m_write_head = (m_write_head + 1) % m_layers;
        return true;
    }

    /**
     * @brief Set the weight of one layer.
     *
     * What a weight does is up to the shader. In the shipped shader a weight of
     * zero or less hides the layer. Layers past WEIGHT_SLOTS have no weight.
     *
     * @param layer  Layer index.
     * @param weight Weight passed to the shader.
     */
    void set_weight(uint32_t layer, float weight);

    /**
     * @brief Set the weights of the first layers in order.
     * @param weights Weights, at most WEIGHT_SLOTS or get_layer_count() are used.
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
        std::array<float, WEIGHT_SLOTS> weights {};
    };

    uint32_t m_layers;
    size_t m_layer_bytes;
    uint32_t m_write_head {};
    bool m_layer_mode {};
    std::optional<Portal::Graphics::FitMode> m_fit { Portal::Graphics::FitMode::STRETCH };
    Portal::Graphics::FilterMode m_filter { Portal::Graphics::FilterMode::LINEAR };
    std::unordered_map<uint32_t, std::optional<Portal::Graphics::FitMode>> m_layer_fit;
    std::shared_ptr<VKBuffer> m_layer_staging;
    std::shared_ptr<VKBuffer> m_source_staging;
    Portal::Graphics::ImageCacheEntry m_source_cache;
    std::vector<uint8_t> m_blank;
    std::shared_ptr<VKBuffer> m_layer_data;
    std::vector<glm::vec4> m_layer_values;
    Push m_push;
    std::vector<uint8_t> m_custom_push;

    void ensure_array();
    void enter_layer_mode();
    void flush_push();
    void clear_layer(uint32_t layer);
    void bind_layer_data();
    void write_layer_value(size_t index, const glm::vec4& values);
    [[nodiscard]] std::optional<Portal::Graphics::FitMode> fit_for(uint32_t layer) const;
};

}
