#include "TextureCollection.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kakshya {

using Portal::Graphics::FilterMode;
using Portal::Graphics::TextureLoom;

namespace {

    /**
     * @brief The largest rectangle of the source's aspect ratio that fits
     *        centred in a @p dst_w x @p dst_h image.
     */
    vk::Rect2D fitted(uint32_t src_w, uint32_t src_h, uint32_t dst_w, uint32_t dst_h)
    {
        const double scale = std::min(
            static_cast<double>(dst_w) / static_cast<double>(src_w),
            static_cast<double>(dst_h) / static_cast<double>(src_h));

        const auto w = std::clamp(static_cast<uint32_t>(std::lround(src_w * scale)), 1U, dst_w);
        const auto h = std::clamp(static_cast<uint32_t>(std::lround(src_h * scale)), 1U, dst_h);

        return {
            vk::Offset2D { static_cast<int32_t>((dst_w - w) / 2), static_cast<int32_t>((dst_h - h) / 2) },
            vk::Extent2D { w, h }
        };
    }

} // namespace

TextureCollection::TextureCollection(uint32_t width, uint32_t height, ImageFormat format)
    : TextureContainer(width, height, format, LayerCount {})
{
}

bool TextureCollection::append(const std::shared_ptr<Core::VKImage>& image)
{
    if (!image || !image->is_initialized()) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "TextureCollection::append called with an uninitialised image");
        return false;
    }

    auto& loom = TextureLoom::instance();

    if (!m_blank) {
        const std::vector<uint8_t> zeros(byte_size(), 0);
        m_blank = loom.create_2d(get_width(), get_height(), get_format(), zeros.data());
        if (!m_blank)
            return false;
    }

    const bool same_extent = image->get_width() == get_width() && image->get_height() == get_height();
    const auto filter = same_extent || !loom.can_blit(image, m_blank, FilterMode::LINEAR)
        ? FilterMode::NEAREST
        : FilterMode::LINEAR;

    if (!loom.can_blit(image, m_blank, filter)) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "TextureCollection::append cannot blit {} into {}",
            vk::to_string(image->get_format()), vk::to_string(m_blank->get_format()));
        return false;
    }

    auto layer = loom.create_2d(get_width(), get_height(), get_format());
    if (!layer)
        return false;

    const bool copied = same_extent
        ? loom.blit_layer(image, layer, { .filter = filter })
        : loom.blit_layer(m_blank, layer, { .filter = FilterMode::NEAREST })
            && loom.blit_layer(image, layer,
                { .dst = fitted(image->get_width(), image->get_height(), get_width(), get_height()), .filter = filter });

    if (!copied)
        return false;

    append_image_layer(std::move(layer));
    return true;
}

} // namespace MayaFlux::Kakshya
