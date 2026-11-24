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
    mix(key.storage ? 1U : 0U);
    mix(key.cube ? 1U : 0U);

    return static_cast<size_t>(hash);
}

ImageTarget::ImageTarget(
    std::shared_ptr<Core::VKImage> image,
    std::vector<std::shared_ptr<Core::VKImage>> extra,
    std::shared_ptr<Core::VKImage> depth,
    const Config& config)
    : m_image(std::move(image))
    , m_extra(std::move(extra))
    , m_depth(std::move(depth))
    , m_mip_filter(config.mip_filter)
    , m_full_chain(config.mip_levels == 0)
    , m_layers(config.cube ? 6U : std::max(config.layers, 1U))
    , m_cube(config.cube)
    , m_storage(config.storage)
    , m_sample_depth(config.sample_depth)
    , m_color_format(m_image ? m_image->get_format() : vk::Format::eUndefined)
    , m_depth_format(m_depth ? m_depth->get_format() : vk::Format::eUndefined)
    , m_continuous(config.continuous)
    , m_preserve(config.preserve)
{
    for (const auto& extra_image : m_extra) {
        m_extra_formats.push_back(extra_image->get_format());
    }
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
    for (const auto& extra_image : m_extra) {
        loom.release_image(extra_image);
    }
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

    auto& loom = get_texture_manager();

    const uint32_t mips = m_full_chain
        ? static_cast<uint32_t>(std::bit_width(std::max(width, height)))
        : m_image->get_mip_levels();

    struct Replacement {
        std::shared_ptr<Core::VKImage>* held;
        ImageKey key;
        bool color;
    };
    std::vector<Replacement> replacements;

    const auto color_key = [&](vk::Format format) -> std::optional<ImageKey> {
        const auto converted = TextureLoom::from_vulkan_format(format);
        if (!converted) {
            return std::nullopt;
        }
        return ImageKey {
            .width = width,
            .height = height,
            .layers = m_layers,
            .mip_levels = mips,
            .format = *converted,
            .kind = ImageKey::Kind::RENDER_TARGET,
            .storage = m_storage,
            .cube = m_cube
        };
    };

    const auto first_key = color_key(m_color_format);
    if (!first_key) {
        return false;
    }
    replacements.push_back({ &m_image, *first_key, true });

    for (size_t i = 0; i < m_extra.size(); ++i) {
        const auto key = color_key(m_extra_formats[i]);
        if (!key) {
            return false;
        }
        replacements.push_back({ &m_extra[i], *key, true });
    }

    if (m_depth) {
        replacements.push_back({ &m_depth,
            { .width = width, .height = height, .kind = ImageKey::Kind::DEPTH },
            false });
    }

    std::vector<ImageCacheEntry> made;
    made.reserve(replacements.size());
    bool complete = true;
    for (const auto& replacement : replacements) {
        ImageCacheEntry entry { .image = *replacement.held };
        if (!loom.acquire_cached_image(entry, replacement.key)) {
            complete = false;
            break;
        }
        made.push_back(std::move(entry));
    }

    if (!complete) {
        for (size_t i = 0; i < made.size(); ++i) {
            if (made[i].image != *replacements[i].held) {
                loom.release_image(made[i].image);
            }
        }
        return false;
    }

    for (size_t i = 0; i < replacements.size(); ++i) {
        const auto& replacement = replacements[i];
        if (made[i].image == *replacement.held) {
            continue;
        }
        loom.release_image(*replacement.held);
        *replacement.held = made[i].image;
        if (replacement.color || m_sample_depth) {
            loom.transition_layout(*replacement.held, vk::ImageLayout::eShaderReadOnlyOptimal, SubmitMode::IMMEDIATE);
        }
    }

    m_width.store(width, std::memory_order_relaxed);
    m_height.store(height, std::memory_order_relaxed);
    m_generation.fetch_add(1, std::memory_order_release);
    request_clear();
    return true;
}

}
