#include "TextBuffer.hpp"

#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "TextureProcessor.hpp"

namespace MayaFlux::Buffers {

void TextBuffer::setup_rendering(const RenderConfig& config)
{
    TextureBuffer::setup_rendering(config);
    get_texture_processor()->set_streaming_mode(true);
    get_render_processor()->enable_alpha_blending();
}

} // namespace MayaFlux::Buffers
