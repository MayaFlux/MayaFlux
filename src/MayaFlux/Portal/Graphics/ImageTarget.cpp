#include "ImageTarget.hpp"

#include "TextureLoom.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"

namespace MayaFlux::Portal::Graphics {

size_t ImageKeyHash::operator()(const ImageKey& key) const noexcept
{
    uint64_t hash = 14695981039346656037ULL;
    const auto mix = [&hash](uint64_t value) {
        hash = (hash ^ value) * 1099511628211ULL;
    };

    mix(key.width);
    mix(key.height);
    mix(key.layers);
    mix(key.mip_levels);
    mix(static_cast<uint64_t>(key.format));
    mix(static_cast<uint64_t>(key.kind));

    return static_cast<size_t>(hash);
}

ImageTarget::ImageTarget(
    std::shared_ptr<Core::VKImage> image,
    std::shared_ptr<Core::VKImage> depth,
    const Config& config)
    : m_image(std::move(image))
    , m_depth(std::move(depth))
    , m_mip_filter(config.mip_filter)
    , m_full_chain(config.mip_levels == 0)
    , m_color_format(m_image ? m_image->get_format() : vk::Format::eUndefined)
    , m_depth_format(m_depth ? m_depth->get_format() : vk::Format::eUndefined)
    , m_continuous(config.continuous)
    , m_preserve(config.preserve)
{
    m_width.store(config.width, std::memory_order_relaxed);
    m_height.store(config.height, std::memory_order_relaxed);
    for (size_t i = 0; i < m_clear.size(); ++i) {
        m_clear[i].store(config.clear_color[i], std::memory_order_relaxed);
    }
}

ImageTarget::~ImageTarget()
{
    auto& loom = get_texture_manager();
    loom.release_image(m_image);
    loom.release_image(m_depth);
}

std::array<float, 4> ImageTarget::clear_color() const noexcept
{
    return { m_clear[0].load(std::memory_order_relaxed),
        m_clear[1].load(std::memory_order_relaxed),
        m_clear[2].load(std::memory_order_relaxed),
        m_clear[3].load(std::memory_order_relaxed) };
}

void ImageTarget::set_clear_color(std::array<float, 4> color) noexcept
{
    for (size_t i = 0; i < m_clear.size(); ++i) {
        m_clear[i].store(color[i], std::memory_order_relaxed);
    }
    request_clear();
}

bool ImageTarget::resize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0) {
        return false;
    }

    const uint32_t old_width = m_width.load(std::memory_order_relaxed);
    const uint32_t old_height = m_height.load(std::memory_order_relaxed);
    if (width == old_width && height == old_height) {
        return true;
    }

    const auto color_format = TextureLoom::from_vulkan_format(m_color_format);
    if (!color_format) {
        return false;
    }

    auto& loom = get_texture_manager();

    const auto reshape = [&loom](std::shared_ptr<Core::VKImage>& image, const ImageKey& key) {
        if (!image) {
            return true;
        }
        ImageCacheEntry entry { .image = image };
        if (!loom.acquire_cached_image(entry, key)) {
            return false;
        }
        if (entry.image != image) {
            loom.release_image(image);
            image = entry.image;
        }
        return true;
    };

    const auto depth_key = [](uint32_t w, uint32_t h) {
        return ImageKey { .width = w, .height = h, .kind = ImageKey::Kind::DEPTH };
    };

    if (!reshape(m_depth, depth_key(width, height))) {
        return false;
    }

    const uint32_t mips = m_full_chain
        ? static_cast<uint32_t>(std::bit_width(std::max(width, height)))
        : m_image->get_mip_levels();
    if (!reshape(m_image, { .width = width, .height = height, .mip_levels = mips, .format = *color_format, .kind = ImageKey::Kind::RENDER_TARGET })) {
        reshape(m_depth, depth_key(old_width, old_height));
        return false;
    }
    loom.transition_layout(m_image, vk::ImageLayout::eShaderReadOnlyOptimal, SubmitMode::IMMEDIATE);

    m_width.store(width, std::memory_order_relaxed);
    m_height.store(height, std::memory_order_relaxed);
    request_clear();
    return true;
}

}
