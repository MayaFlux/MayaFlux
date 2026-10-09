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

TextureCollection::TextureCollection(const TextureCollectionSpec& spec)
    : TextureContainer(spec.width, spec.height, spec.format, LayerCount {})
    , m_spec(spec)
{
}

bool TextureCollection::is_full() const
{
    if (m_spec.ring_layers > 0 || m_spec.max_bytes == 0)
        return false;

    return (static_cast<uint64_t>(get_layer_count()) + 1) * byte_size() > m_spec.max_bytes;
}

bool TextureCollection::append(const std::shared_ptr<Core::VKImage>& image)
{
    if (!image || !image->is_initialized()) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "TextureCollection::append called with an uninitialised image");
        return false;
    }

    if (is_full()) {
        if (!m_full_reported) {
            MF_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
                "TextureCollection is full at {} layers ({} byte cap)", get_layer_count(), m_spec.max_bytes);
            m_full_reported = true;
        }
        return false;
    }

    const bool reuse = m_spec.ring_layers > 0 && m_write_head >= m_spec.ring_layers;
    const auto slot = reuse
        ? static_cast<uint32_t>(m_write_head % m_spec.ring_layers)
        : get_layer_count();

    auto target = reuse
        ? layer_image(slot)
        : TextureLoom::instance().create_2d(get_width(), get_height(), get_format());

    if (!target || !copy_into(image, target))
        return false;

    if (reuse) {
        hold_layer(slot);
    } else {
        append_image_layer(std::move(target));
    }

    ++m_write_head;
    return true;
}

bool TextureCollection::copy_into(
    const std::shared_ptr<Core::VKImage>& image,
    const std::shared_ptr<Core::VKImage>& target)
{
    auto& loom = TextureLoom::instance();

    if (!m_blank) {
        const std::vector<uint8_t> zeros(byte_size(), 0);
        m_blank = loom.create_2d(get_width(), get_height(), get_format(), zeros.data());
        if (!m_blank)
            return false;
    }

    const bool same_extent = image->get_width() == get_width() && image->get_height() == get_height();
    const auto filter = same_extent || !loom.can_blit(image, target, FilterMode::LINEAR)
        ? FilterMode::NEAREST
        : FilterMode::LINEAR;

    if (!loom.can_blit(image, target, filter)) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "TextureCollection::append cannot blit {} into {}",
            vk::to_string(image->get_format()), vk::to_string(target->get_format()));
        return false;
    }

    if (same_extent)
        return loom.blit_layer(image, target, { .filter = filter });

    return loom.blit_layer(m_blank, target, { .filter = FilterMode::NEAREST })
        && loom.blit_layer(image, target,
            { .dst = fitted(image->get_width(), image->get_height(), get_width(), get_height()), .filter = filter });
}

} // namespace MayaFlux::Kakshya
