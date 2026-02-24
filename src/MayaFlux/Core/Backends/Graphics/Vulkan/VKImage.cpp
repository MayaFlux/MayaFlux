#include "VKImage.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKEnumUtils.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Core {

VKImage::VKImage(
    uint32_t width,
    uint32_t height,
    uint32_t depth,
    vk::Format format,
    Usage usage,
    Type type,
    uint32_t mip_levels,
    uint32_t array_layers,
    Kakshya::DataModality modality)
    : m_width(width)
    , m_height(height)
    , m_depth(depth)
    , m_format(format)
    , m_usage(usage)
    , m_type(type)
    , m_mip_levels(mip_levels)
    , m_array_layers(array_layers)
    , m_modality(modality)
{
    infer_dimensions_from_parameters();
}

void VKImage::set_modality(Kakshya::DataModality modality)
{
    m_modality = modality;
    infer_dimensions_from_parameters();
}

vk::ImageUsageFlags VKImage::get_usage_flags() const
{
    vk::ImageUsageFlags flags = vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst;

    switch (m_usage) {
    case Usage::TEXTURE_2D:
        flags |= vk::ImageUsageFlagBits::eSampled;
        break;

    case Usage::RENDER_TARGET:
        flags |= vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
        break;

    case Usage::DEPTH_STENCIL:
        flags |= vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled;
        break;

    case Usage::STORAGE:
        flags |= vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled;
        break;

    case Usage::TRANSFER_SRC:
    case Usage::TRANSFER_DST:
    case Usage::STAGING:
        // Staging images are rare in Vulkan (usually use buffers)
        // Just transfer ops
        break;
    }

    return flags;
}

vk::MemoryPropertyFlags VKImage::get_memory_properties() const
{
    if (is_host_visible()) {
        return vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
    }

    return vk::MemoryPropertyFlagBits::eDeviceLocal;
}

vk::ImageAspectFlags VKImage::get_aspect_flags() const
{
    if (m_usage == Usage::DEPTH_STENCIL) {
        switch (m_format) {
        case vk::Format::eD16UnormS8Uint:
        case vk::Format::eD24UnormS8Uint:
        case vk::Format::eD32SfloatS8Uint:
            return vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;

        case vk::Format::eD16Unorm:
        case vk::Format::eD32Sfloat:
        case vk::Format::eX8D24UnormPack32:
            return vk::ImageAspectFlagBits::eDepth;

        case vk::Format::eS8Uint:
            return vk::ImageAspectFlagBits::eStencil;

        default:
            return vk::ImageAspectFlagBits::eDepth;
        }
    }

    return vk::ImageAspectFlagBits::eColor;
}

size_t VKImage::get_size_bytes() const
{
    const size_t bpp = vk_format_bytes_per_pixel(m_format);

    size_t total = 0;
    uint32_t mip_w = m_width;
    uint32_t mip_h = m_height;
    uint32_t mip_d = m_depth;

    for (uint32_t i = 0; i < m_mip_levels; ++i) {
        total += static_cast<size_t>(mip_w) * mip_h * mip_d * bpp;
        mip_w = std::max(1U, mip_w / 2);
        mip_h = std::max(1U, mip_h / 2);
        mip_d = std::max(1U, mip_d / 2);
    }

    return total * m_array_layers;
}

void VKImage::infer_dimensions_from_parameters()
{
    using namespace Kakshya;

    m_dimensions.clear();

    switch (m_type) {
    case Type::TYPE_1D:
        m_dimensions.push_back(DataDimension::spatial_1d(m_width));
        break;

    case Type::TYPE_2D:
        m_dimensions.push_back(DataDimension::spatial_2d(m_width, m_height));
        break;

    case Type::TYPE_3D:
        m_dimensions.push_back(DataDimension::spatial_3d(m_width, m_height, m_depth));
        break;

    case Type::TYPE_CUBE:
        m_dimensions.push_back(DataDimension::grouped("cubemap_faces", 6, 2));

        break;
    }

    uint32_t num_channels = 0;
    switch (m_format) {
    case vk::Format::eR8Unorm:
    case vk::Format::eR16Unorm:
    case vk::Format::eR32Sfloat:
        num_channels = 1;
        break;

    case vk::Format::eR8G8Unorm:
    case vk::Format::eR16G16Unorm:
    case vk::Format::eR32G32Sfloat:
        num_channels = 2;
        break;

    case vk::Format::eR8G8B8Unorm:
    case vk::Format::eB8G8R8Unorm:
        num_channels = 3;
        break;

    case vk::Format::eR8G8B8A8Unorm:
    case vk::Format::eB8G8R8A8Unorm:
    case vk::Format::eR8G8B8A8Srgb:
    case vk::Format::eR16G16B16A16Unorm:
    case vk::Format::eR32G32B32A32Sfloat:
    default:
        num_channels = 4; // Assume RGBA for unknown formats
        break;
    }

    if (num_channels > 0) {
        m_dimensions.push_back(DataDimension::channel(num_channels));
    }

    if (m_mip_levels > 1) {
        m_dimensions.push_back(DataDimension::mipmap_levels(m_mip_levels));
    }

    if (m_array_layers > 1 && m_type != Type::TYPE_CUBE) {
        m_dimensions.push_back(DataDimension::grouped("cubemap_faces", 6, 2));
    }

    MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "VKImage dimensions inferred: {}x{}x{}, {} channels, {} mips, {} layers",
        m_width, m_height, m_depth, num_channels, m_mip_levels, m_array_layers);
}

} // namespace MayaFlux::Core
