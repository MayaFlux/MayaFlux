#include "TextureCollection.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"
#include "MayaFlux/Portal/Graphics/SamplerForge.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kakshya {

using Portal::Graphics::FilterMode;
using Portal::Graphics::FitMode;
using Portal::Graphics::SamplerForge;
using Portal::Graphics::TextureLoom;

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

bool TextureCollection::append(const std::shared_ptr<Core::VKImage>& image, Portal::Graphics::SubmitMode mode)
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

    if (!target || !copy_into(image, target, mode))
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
    const std::shared_ptr<Core::VKImage>& target,
    Portal::Graphics::SubmitMode mode)
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
        return loom.blit_layer(image, target, { .filter = filter }, mode);

    if (!loom.blit_layer(m_blank, target, { .filter = FilterMode::NEAREST }, mode))
        return false;

    const auto plan = SamplerForge::plan_fit(
        image->get_width(), image->get_height(), get_width(), get_height(),
        FitMode::CONTAIN, filter);

    bool stored = true;
    for (const auto& blit : plan.blits)
        stored = loom.blit_layer(image, target, blit, mode) && stored;
    return stored;
}

} // namespace MayaFlux::Kakshya
