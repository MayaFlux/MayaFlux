#include "TextBuffer.hpp"

#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

void TextBuffer::setup_rendering(const RenderConfig& config)
{
    TextureBuffer::setup_rendering(config);
    if (!get_texture_processor()) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::Configuration,
            "TextBuffer missing TextureProcessor during setup_rendering. Ensure the buffer is registered prior to calling setup rendering");
        return;
    }
    get_render_processor()->enable_alpha_blending();
}

} // namespace MayaFlux::Buffers
