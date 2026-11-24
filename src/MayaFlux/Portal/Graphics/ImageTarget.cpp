#include "ImageTarget.hpp"

#include "TextureLoom.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"

namespace MayaFlux::Portal::Graphics {

ImageTarget::ImageTarget(
    std::shared_ptr<Core::VKImage> image,
    std::shared_ptr<Core::VKImage> depth,
    const Config& config)
    : m_image(std::move(image))
    , m_depth(std::move(depth))
    , m_width(config.width)
    , m_height(config.height)
    , m_color_format(m_image ? m_image->get_format() : vk::Format::eUndefined)
    , m_depth_format(m_depth ? m_depth->get_format() : vk::Format::eUndefined)
    , m_continuous(config.continuous)
{
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
    request_redraw();
}

}
