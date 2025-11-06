#include "TextureLoom.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/BackendResoureManager.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VulkanBackend.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Portal::Graphics {

bool TextureLoom::s_initialized = false;

bool TextureLoom::initialize(const std::shared_ptr<Core::VulkanBackend>& backend)
{
    if (s_initialized) {
        MF_WARN(Journal::Component::Portal, Journal::Context::ImageProcessing,
            "TextureLoom already initialized (static flag)");
        return true;
    }

    if (!backend) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::ImageProcessing,
            "Cannot initialize TextureLoom with null backend");
        return false;
    }

    if (m_backend) {
        MF_WARN(Journal::Component::Portal, Journal::Context::ImageProcessing,
            "TextureLoom already initialized");
        return true;
    }

    m_backend = backend;
    m_resource_manager = &m_backend->get_resource_manager();
    s_initialized = true;

    MF_INFO(Journal::Component::Portal, Journal::Context::ImageProcessing,
        "TextureLoom initialized");
    return true;
}

void TextureLoom::shutdown()
{
    if (!s_initialized) {
        return;
    }

    if (!m_backend) {
        return;
    }

    MF_INFO(Journal::Component::Portal, Journal::Context::ImageProcessing,
        "Shutting down TextureLoom...");

    for (auto& texture : m_textures) {
        if (texture && texture->is_initialized()) {
            m_resource_manager->cleanup_image(texture);
        }
    }
    m_textures.clear();
    m_sampler_cache.clear();
    m_resource_manager = nullptr;
    m_backend = nullptr;

    s_initialized = false;

    MF_INFO(Journal::Component::Portal, Journal::Context::ImageProcessing,
        "TextureLoom shutdown complete");
}

//==============================================================================
// Texture Creation
//==============================================================================

std::shared_ptr<Core::VKImage> TextureLoom::create_2d(
    uint32_t width, uint32_t height,
    ImageFormat format, const void* data, uint32_t mip_levels)
{
    if (!is_initialized()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::ImageProcessing,
            "TextureLoom not initialized");
        return nullptr;
    }

    auto vk_format = to_vulkan_format(format);
    auto image = std::make_shared<Core::VKImage>(
        width, height, 1, vk_format,
        Core::VKImage::Usage::TEXTURE_2D,
        Core::VKImage::Type::TYPE_2D,
        mip_levels, 1,
        Kakshya::DataModality::IMAGE_COLOR);

    m_resource_manager->initialize_image(image);

    if (!image->is_initialized()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::ImageProcessing,
            "Failed to initialize VKImage");
        return nullptr;
    }

    if (data) {
        size_t data_size = calculate_image_size(width, height, 1, format);
        upload_data(image, data, data_size);
    } else {
        m_resource_manager->transition_image_layout(
            image->get_image(),
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eShaderReadOnlyOptimal,
            mip_levels, 1, vk::ImageAspectFlagBits::eColor);
        image->set_current_layout(vk::ImageLayout::eShaderReadOnlyOptimal);
    }

    m_textures.push_back(image);
    MF_INFO(Journal::Component::Portal, Journal::Context::ImageProcessing,
        "Created 2D texture: {}x{}, format: {}, mips: {}",
        width, height, vk::to_string(vk_format), mip_levels);
    return image;
}

std::shared_ptr<Core::VKImage> TextureLoom::create_3d(
    uint32_t width, uint32_t height, uint32_t depth,
    ImageFormat format, const void* data)
{
    if (!is_initialized()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::ImageProcessing,
            "TextureLoom not initialized");
        return nullptr;
    }

    auto vk_format = to_vulkan_format(format);
    auto image = std::make_shared<Core::VKImage>(
        width, height, depth, vk_format,
        Core::VKImage::Usage::TEXTURE_2D,
        Core::VKImage::Type::TYPE_3D,
        1, 1, Kakshya::DataModality::VOLUMETRIC_3D);

    m_resource_manager->initialize_image(image);

    if (!image->is_initialized()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::ImageProcessing,
            "Failed to initialize 3D VKImage");
        return nullptr;
    }

    if (data) {
        size_t data_size = calculate_image_size(width, height, depth, format);
        upload_data(image, data, data_size);
    } else {
        m_resource_manager->transition_image_layout(
            image->get_image(),
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eShaderReadOnlyOptimal,
            1, 1, vk::ImageAspectFlagBits::eColor);
        image->set_current_layout(vk::ImageLayout::eShaderReadOnlyOptimal);
    }

    m_textures.push_back(image);
    MF_INFO(Journal::Component::Portal, Journal::Context::ImageProcessing,
        "Created 3D texture: {}x{}x{}, format: {}",
        width, height, depth, vk::to_string(vk_format));
    return image;
}

std::shared_ptr<Core::VKImage> TextureLoom::create_cubemap(
    uint32_t size, ImageFormat format, const void* data)
{
    if (!is_initialized()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::ImageProcessing,
            "TextureLoom not initialized");
        return nullptr;
    }

    auto vk_format = to_vulkan_format(format);
    auto image = std::make_shared<Core::VKImage>(
        size, size, 1, vk_format,
        Core::VKImage::Usage::TEXTURE_2D,
        Core::VKImage::Type::TYPE_CUBE,
        1, 6, Kakshya::DataModality::IMAGE_COLOR);

    m_resource_manager->initialize_image(image);

    if (!image->is_initialized()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::ImageProcessing,
            "Failed to initialize cubemap VKImage");
        return nullptr;
    }

    if (data) {
        size_t face_size = calculate_image_size(size, size, 1, format);
        size_t total_size = face_size * 6;
        upload_data(image, data, total_size);
    } else {
        m_resource_manager->transition_image_layout(
            image->get_image(),
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eShaderReadOnlyOptimal,
            1, 6, vk::ImageAspectFlagBits::eColor);
        image->set_current_layout(vk::ImageLayout::eShaderReadOnlyOptimal);
    }

    m_textures.push_back(image);
    MF_INFO(Journal::Component::Portal, Journal::Context::ImageProcessing,
        "Created cubemap: {}x{}, format: {}", size, size, vk::to_string(vk_format));
    return image;
}

std::shared_ptr<Core::VKImage> TextureLoom::create_render_target(
    uint32_t width, uint32_t height, ImageFormat format)
{
    if (!is_initialized()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::ImageProcessing,
            "TextureLoom not initialized");
        return nullptr;
    }

    auto vk_format = to_vulkan_format(format);
    auto image = std::make_shared<Core::VKImage>(
        width, height, 1, vk_format,
        Core::VKImage::Usage::RENDER_TARGET,
        Core::VKImage::Type::TYPE_2D,
        1, 1, Kakshya::DataModality::IMAGE_COLOR);

    m_resource_manager->initialize_image(image);

    if (!image->is_initialized()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::ImageProcessing,
            "Failed to initialize render target VKImage");
        return nullptr;
    }

    m_resource_manager->transition_image_layout(
        image->get_image(),
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eColorAttachmentOptimal,
        1, 1, vk::ImageAspectFlagBits::eColor);
    image->set_current_layout(vk::ImageLayout::eColorAttachmentOptimal);

    m_textures.push_back(image);
    MF_INFO(Journal::Component::Portal, Journal::Context::ImageProcessing,
        "Created render target: {}x{}, format: {}",
        width, height, vk::to_string(vk_format));
    return image;
}

std::shared_ptr<Core::VKImage> TextureLoom::create_depth_buffer(
    uint32_t width, uint32_t height, bool with_stencil)
{
    if (!is_initialized()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::ImageProcessing,
            "TextureLoom not initialized");
        return nullptr;
    }

    vk::Format vk_format = with_stencil
        ? vk::Format::eD24UnormS8Uint
        : vk::Format::eD32Sfloat;

    auto image = std::make_shared<Core::VKImage>(
        width, height, 1, vk_format,
        Core::VKImage::Usage::DEPTH_STENCIL,
        Core::VKImage::Type::TYPE_2D,
        1, 1, Kakshya::DataModality::IMAGE_2D);

    m_resource_manager->initialize_image(image);

    if (!image->is_initialized()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::ImageProcessing,
            "Failed to initialize depth buffer VKImage");
        return nullptr;
    }

    vk::ImageAspectFlags aspect = with_stencil
        ? (vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil)
        : vk::ImageAspectFlagBits::eDepth;

    m_resource_manager->transition_image_layout(
        image->get_image(),
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eDepthStencilAttachmentOptimal,
        1, 1, aspect);
    image->set_current_layout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

    m_textures.push_back(image);
    MF_INFO(Journal::Component::Portal, Journal::Context::ImageProcessing,
        "Created depth buffer: {}x{}, format: {}, stencil: {}",
        width, height, vk::to_string(vk_format), with_stencil);
    return image;
}

std::shared_ptr<Core::VKImage> TextureLoom::create_storage_image(
    uint32_t width, uint32_t height, ImageFormat format)
{
    if (!is_initialized()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::ImageProcessing,
            "TextureLoom not initialized");
        return nullptr;
    }

    auto vk_format = to_vulkan_format(format);
    auto image = std::make_shared<Core::VKImage>(
        width, height, 1, vk_format,
        Core::VKImage::Usage::STORAGE,
        Core::VKImage::Type::TYPE_2D,
        1, 1, Kakshya::DataModality::IMAGE_COLOR);

    m_resource_manager->initialize_image(image);

    if (!image->is_initialized()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::ImageProcessing,
            "Failed to initialize storage image VKImage");
        return nullptr;
    }

    m_resource_manager->transition_image_layout(
        image->get_image(),
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eGeneral,
        1, 1, vk::ImageAspectFlagBits::eColor);
    image->set_current_layout(vk::ImageLayout::eGeneral);

    m_textures.push_back(image);
    MF_INFO(Journal::Component::Portal, Journal::Context::ImageProcessing,
        "Created storage image: {}x{}, format: {}",
        width, height, vk::to_string(vk_format));
    return image;
}

//==============================================================================
// Data Upload/Download
//==============================================================================

void TextureLoom::upload_data(
    const std::shared_ptr<Core::VKImage>& image, const void* data, size_t size)
{
    if (!is_initialized() || !image || !data) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::ImageProcessing,
            "Invalid parameters for upload_data");
        return;
    }

    m_resource_manager->upload_image_data(image, data, size);
}

void TextureLoom::download_data(
    const std::shared_ptr<Core::VKImage>& image, void* data, size_t size)
{
    if (!is_initialized() || !image || !data) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::ImageProcessing,
            "Invalid parameters for download_data");
        return;
    }

    m_resource_manager->download_image_data(image, data, size);
}

//==============================================================================
// Sampler Management
//==============================================================================

vk::Sampler TextureLoom::get_or_create_sampler(const SamplerConfig& config)
{
    if (!is_initialized()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::ImageProcessing,
            "TextureLoom not initialized");
        return nullptr;
    }

    size_t hash = hash_sampler_config(config);
    auto it = m_sampler_cache.find(hash);
    if (it != m_sampler_cache.end()) {
        return it->second;
    }

    vk::Sampler sampler = create_sampler(config);
    m_sampler_cache[hash] = sampler;
    return sampler;
}

vk::Sampler TextureLoom::get_default_sampler()
{
    SamplerConfig config;
    return get_or_create_sampler(config);
}

vk::Sampler TextureLoom::get_nearest_sampler()
{
    SamplerConfig config;
    config.mag_filter = FilterMode::NEAREST;
    config.min_filter = FilterMode::NEAREST;
    config.address_mode_u = AddressMode::CLAMP_TO_EDGE;
    config.address_mode_v = AddressMode::CLAMP_TO_EDGE;
    return get_or_create_sampler(config);
}

vk::Sampler TextureLoom::create_sampler(const SamplerConfig& config)
{
    return m_resource_manager->create_sampler(
        static_cast<vk::Filter>(config.mag_filter),
        static_cast<vk::SamplerAddressMode>(config.address_mode_u),
        config.max_anisotropy);
}

size_t TextureLoom::hash_sampler_config(const SamplerConfig& config)
{
    size_t hash = 0;
    hash ^= std::hash<int> {}(static_cast<int>(config.mag_filter)) << 0;
    hash ^= std::hash<int> {}(static_cast<int>(config.min_filter)) << 4;
    hash ^= std::hash<int> {}(static_cast<int>(config.address_mode_u)) << 8;
    hash ^= std::hash<int> {}(static_cast<int>(config.address_mode_v)) << 12;
    hash ^= std::hash<int> {}(static_cast<int>(config.address_mode_w)) << 16;
    hash ^= std::hash<float> {}(config.max_anisotropy) << 20;
    hash ^= std::hash<bool> {}(config.enable_mipmaps) << 24;
    return hash;
}

//==============================================================================
// Utilities
//==============================================================================

vk::Format TextureLoom::to_vulkan_format(ImageFormat format)
{
    switch (format) {
    case ImageFormat::R8:
        return vk::Format::eR8Unorm;
    case ImageFormat::RG8:
        return vk::Format::eR8G8Unorm;
    case ImageFormat::RGB8:
        return vk::Format::eR8G8B8Unorm;
    case ImageFormat::RGBA8:
        return vk::Format::eR8G8B8A8Unorm;
    case ImageFormat::RGBA8_SRGB:
        return vk::Format::eR8G8B8A8Srgb;
    case ImageFormat::R16F:
        return vk::Format::eR16Sfloat;
    case ImageFormat::RG16F:
        return vk::Format::eR16G16Sfloat;
    case ImageFormat::RGBA16F:
        return vk::Format::eR16G16B16A16Sfloat;
    case ImageFormat::R32F:
        return vk::Format::eR32Sfloat;
    case ImageFormat::RG32F:
        return vk::Format::eR32G32Sfloat;
    case ImageFormat::RGBA32F:
        return vk::Format::eR32G32B32A32Sfloat;
    case ImageFormat::DEPTH16:
        return vk::Format::eD16Unorm;
    case ImageFormat::DEPTH24:
        return vk::Format::eX8D24UnormPack32;
    case ImageFormat::DEPTH32F:
        return vk::Format::eD32Sfloat;
    case ImageFormat::DEPTH24_STENCIL8:
        return vk::Format::eD24UnormS8Uint;
    default:
        return vk::Format::eR8G8B8A8Unorm;
    }
}

size_t TextureLoom::get_bytes_per_pixel(ImageFormat format)
{
    switch (format) {
    case ImageFormat::R8:
        return 1;
    case ImageFormat::RG8:
        return 2;
    case ImageFormat::RGB8:
        return 3;
    case ImageFormat::RGBA8:
    case ImageFormat::RGBA8_SRGB:
        return 4;
    case ImageFormat::R16F:
        return 2;
    case ImageFormat::RG16F:
        return 4;
    case ImageFormat::RGBA16F:
        return 8;
    case ImageFormat::R32F:
        return 4;
    case ImageFormat::RG32F:
        return 8;
    case ImageFormat::RGBA32F:
        return 16;
    case ImageFormat::DEPTH16:
        return 2;
    case ImageFormat::DEPTH24:
    case ImageFormat::DEPTH32F:
    case ImageFormat::DEPTH24_STENCIL8:
    default:
        return 4;
    }
}

size_t TextureLoom::calculate_image_size(
    uint32_t width, uint32_t height, uint32_t depth, ImageFormat format)
{
    return static_cast<size_t>(width) * height * depth * get_bytes_per_pixel(format);
}

} // namespace MayaFlux::Portal::Graphics
