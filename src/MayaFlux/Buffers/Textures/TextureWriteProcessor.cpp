#include "TextureWriteProcessor.hpp"

#include "TextureBuffer.hpp"

namespace MayaFlux::Buffers {

TextureWriteProcessor::TextureWriteProcessor()
{
    m_pixel_source = PixelSource::EXTERNAL_DATA;
    set_streaming_mode(true);
}

void TextureWriteProcessor::set_data(Kakshya::DataVariant variant)
{
    m_pending = std::move(variant);
    m_dirty.test_and_set(std::memory_order_release);

    if (m_texture_buffer) {
        m_texture_buffer->mark_texture_dirty();
    }
}

bool TextureWriteProcessor::has_pending() const noexcept
{
    return m_dirty.test(std::memory_order_acquire);
}

std::optional<Kakshya::DataVariant> TextureWriteProcessor::get_variant_source()
{
    if (m_dirty.test(std::memory_order_acquire)) {
        m_dirty.clear(std::memory_order_release);
        std::swap(m_active, m_pending);
    }

    return m_active;
}

} // namespace MayaFlux::Buffers
