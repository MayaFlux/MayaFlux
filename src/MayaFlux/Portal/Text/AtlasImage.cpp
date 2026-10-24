#include "AtlasImage.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Portal/Graphics/TextureLoom.hpp"

namespace MayaFlux::Portal::Text {

std::shared_ptr<Core::VKImage> AtlasImage::sync(GlyphAtlas& atlas)
{
    auto& loom = Portal::Graphics::get_texture_manager();
    if (!loom.is_initialized()) {
        return nullptr;
    }

    const std::span<const uint8_t> pixels = atlas.texture().pixel_bytes(0);
    const uint32_t size = atlas.atlas_size();

    if (!m_image || !m_image->is_initialized() || m_atlas != &atlas || m_size != size) {
        m_image = loom.create_2d(size, size, Portal::Graphics::ImageFormat::R8, pixels.data(), 1);
        if (!m_image) {
            MF_ERROR(Journal::Component::Portal, Journal::Context::API,
                "AtlasImage: could not create a {}x{} atlas image", size, size);
            m_atlas = nullptr;
            m_size = 0;
            return nullptr;
        }
        m_atlas = &atlas;
        m_size = size;
        ++m_generation;
        atlas.clear_dirty();
        return m_image;
    }

    if (atlas.is_dirty()) {
        loom.upload_data(m_image, pixels.data(), pixels.size());
        atlas.clear_dirty();
    }

    return m_image;
}

} // namespace MayaFlux::Portal::Text
