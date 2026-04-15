#pragma once

#include "TextureBuffer.hpp"

namespace MayaFlux::Buffers {

/**
 * @class TextBuffer
 * @brief TextureBuffer specialization for CPU-composited glyph textures.
 *
 * Encodes two invariants that hold for all pre-multiplied RGBA glyph
 * coverage textures: streaming mode is enabled on the TextureProcessor
 * to avoid per-frame staging buffer allocation under animated text, and
 * alpha blending is enabled on the RenderProcessor so glyph coverage
 * composites correctly over whatever is behind it.
 *
 * Produced by Portal::Text::InkPress. Callers interact with it identically
 * to TextureBuffer -- set_position, set_scale, setup_rendering -- with no
 * knowledge of the invariants required.
 */
class MAYAFLUX_API TextBuffer : public TextureBuffer {
public:
    using TextureBuffer::TextureBuffer;

    /**
     * @brief Delegates to TextureBuffer::setup_rendering, then enables
     *        streaming mode on the TextureProcessor and alpha blending
     *        on the RenderProcessor.
     * @param config  Same as TextureBuffer::setup_rendering.
     */
    void setup_rendering(const RenderConfig& config) override;
};

} // namespace MayaFlux::Buffers
