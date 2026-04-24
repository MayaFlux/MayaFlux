#include "TextureLoom.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/BackendResoureManager.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VulkanBackend.hpp"

#include "MayaFlux/Kakshya/NDData/TextureAccess.hpp"

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/BufferService.hpp"

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

std::shared_ptr<Core::VKImage> TextureLoom::create_2d(
    const Kakshya::DataVariant& variant,
    uint32_t width,
    uint32_t height,
    ImageFormat format)
{
    if (!is_initialized()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::ImageProcessing,
            "TextureLoom not initialized");
        return nullptr;
    }

    const auto access = Kakshya::as_texture_access(variant);
    if (!access) {
        return nullptr;
    }

    const size_t expected = calculate_image_size(width, height, 1, format);
    if (access->byte_count != expected) {
        error<std::invalid_argument>(
            Journal::Component::Portal,
            Journal::Context::ImageProcessing,
            std::source_location::current(),
            "create_2d: byte count mismatch — {} bytes supplied, "
            "{}x{} format {} requires {}",
            access->byte_count, width, height, static_cast<int>(format), expected);
    }

    return create_2d(width, height, format, access->data_ptr, 1);
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

void TextureLoom::upload_data(
    const std::shared_ptr<Core::VKImage>& image,
    const void* data,
    size_t size,
    const std::shared_ptr<Buffers::VKBuffer>& staging)
{
    if (!is_initialized() || !image || !data || !staging) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::ImageProcessing,
            "Invalid parameters for upload_data_streaming");
        return;
    }

    m_resource_manager->upload_image_data_with_staging(image, data, size, staging);
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

void TextureLoom::download_data_async(
    const std::shared_ptr<Core::VKImage>& image, void* data, size_t size)
{
    if (!is_initialized() || !image || !data) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::ImageProcessing,
            "Invalid parameters for download_data_async");
        return;
    }

    auto buffer_service = Registry::BackendRegistry::instance()
                              .get_service<Registry::Service::BufferService>();
    if (!buffer_service || !buffer_service->execute_fenced
        || !buffer_service->wait_fenced || !buffer_service->release_fenced
        || !buffer_service->initialize_buffer || !buffer_service->destroy_buffer
        || !buffer_service->invalidate_range) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::ImageProcessing,
            "download_data_async: BufferService unavailable or incomplete");
        return;
    }

    auto staging = std::make_shared<Buffers::VKBuffer>(
        size,
        Buffers::VKBuffer::Usage::STAGING,
        Kakshya::DataModality::IMAGE_COLOR);

    buffer_service->initialize_buffer(std::static_pointer_cast<void>(staging));

    auto handle = buffer_service->execute_fenced([&](void* cmd_ptr) {
        vk::CommandBuffer cmd(static_cast<VkCommandBuffer>(cmd_ptr));

        vk::ImageMemoryBarrier barrier {};
        barrier.oldLayout = image->get_current_layout();
        barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image->get_image();
        barrier.subresourceRange.aspectMask = image->get_aspect_flags();
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = image->get_mip_levels();
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = image->get_array_layers();
        barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

        cmd.pipelineBarrier(
            vk::PipelineStageFlagBits::eFragmentShader,
            vk::PipelineStageFlagBits::eTransfer,
            vk::DependencyFlags {}, {}, {}, barrier);

        vk::BufferImageCopy region {};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = image->get_aspect_flags();
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = image->get_array_layers();
        region.imageOffset = vk::Offset3D { 0, 0, 0 };
        region.imageExtent = vk::Extent3D {
            image->get_width(),
            image->get_height(),
            image->get_depth()
        };

        cmd.copyImageToBuffer(
            image->get_image(),
            vk::ImageLayout::eTransferSrcOptimal,
            staging->get_buffer(),
            1, &region);

        barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
        barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
        barrier.dstAccessMask = vk::AccessFlagBits::eMemoryRead;

        cmd.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eFragmentShader,
            vk::DependencyFlags {}, {}, {}, barrier);
    });

    if (!handle) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::ImageProcessing,
            "download_data_async: execute_fenced returned null handle");
        buffer_service->destroy_buffer(std::static_pointer_cast<void>(staging));
        return;
    }

    image->set_current_layout(vk::ImageLayout::eShaderReadOnlyOptimal);

    buffer_service->wait_fenced(handle);

    auto& resources = staging->get_buffer_resources();
    buffer_service->invalidate_range(resources.memory, 0, 0);

    void* mapped = staging->get_mapped_ptr();
    if (mapped) {
        std::memcpy(data, mapped, size);
    } else {
        MF_ERROR(Journal::Component::Portal, Journal::Context::ImageProcessing,
            "download_data_async: staging buffer has no mapped pointer");
    }

    buffer_service->release_fenced(handle);
    buffer_service->destroy_buffer(std::static_pointer_cast<void>(staging));

    MF_DEBUG(Journal::Component::Portal, Journal::Context::ImageProcessing,
        "download_data_async: completed {} byte download for {}x{}",
        size, image->get_width(), image->get_height());
}

void TextureLoom::transition_layout(
    const std::shared_ptr<Core::VKImage>& image,
    vk::ImageLayout old_layout,
    vk::ImageLayout new_layout,
    uint32_t mip_levels,
    uint32_t array_layers,
    vk::ImageAspectFlags aspect_mask)
{
    if (!is_initialized() || !image) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::ImageProcessing,
            "Invalid parameters for transition_vk_image_layout");
        return;
    }

    m_resource_manager->transition_image_layout(
        image->get_image(),
        old_layout,
        new_layout,
        mip_levels, array_layers, aspect_mask);

    image->set_current_layout(new_layout);
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
    case ImageFormat::BGRA8:
        return vk::Format::eB8G8R8A8Unorm;
    case ImageFormat::BGRA8_SRGB:
        return vk::Format::eB8G8R8A8Srgb;
        return vk::Format::eR8G8B8A8Srgb;
    case ImageFormat::R16:
        return vk::Format::eR16Unorm;
    case ImageFormat::RG16:
        return vk::Format::eR16G16Unorm;
    case ImageFormat::RGBA16:
        return vk::Format::eR16G16B16A16Unorm;
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
    case ImageFormat::BGRA8:
    case ImageFormat::BGRA8_SRGB:
        return 4;
    case ImageFormat::R16:
        return 2;
    case ImageFormat::RG16:
        return 4;
    case ImageFormat::RGBA16:
        return 8;
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

std::optional<ImageFormat> TextureLoom::from_vulkan_format(vk::Format vk_format)
{
    switch (vk_format) {
    case vk::Format::eR8Unorm:
        return ImageFormat::R8;
    case vk::Format::eR8G8Unorm:
        return ImageFormat::RG8;
    case vk::Format::eR8G8B8Unorm:
        return ImageFormat::RGB8;
    case vk::Format::eR8G8B8A8Unorm:
        return ImageFormat::RGBA8;
    case vk::Format::eR8G8B8A8Srgb:
        return ImageFormat::RGBA8_SRGB;
    case vk::Format::eB8G8R8A8Unorm:
        return ImageFormat::BGRA8;

    case vk::Format::eR16Unorm:
        return ImageFormat::R16;
    case vk::Format::eR16G16Unorm:
        return ImageFormat::RG16;
    case vk::Format::eR16G16B16A16Unorm:
        return ImageFormat::RGBA16;

    case vk::Format::eR16Sfloat:
        return ImageFormat::R16F;
    case vk::Format::eR16G16Sfloat:
        return ImageFormat::RG16F;
    case vk::Format::eR16G16B16A16Sfloat:
        return ImageFormat::RGBA16F;

    case vk::Format::eR32Sfloat:
        return ImageFormat::R32F;
    case vk::Format::eR32G32Sfloat:
        return ImageFormat::RG32F;
    case vk::Format::eR32G32B32A32Sfloat:
        return ImageFormat::RGBA32F;

    case vk::Format::eD16Unorm:
        return ImageFormat::DEPTH16;
    case vk::Format::eX8D24UnormPack32:
        return ImageFormat::DEPTH24;
    case vk::Format::eD32Sfloat:
        return ImageFormat::DEPTH32F;
    case vk::Format::eD24UnormS8Uint:
        return ImageFormat::DEPTH24_STENCIL8;

    default:
        return std::nullopt;
    }
}

size_t TextureLoom::calculate_image_size(
    uint32_t width, uint32_t height, uint32_t depth, ImageFormat format)
{
    return static_cast<size_t>(width) * height * depth * get_bytes_per_pixel(format);
}

uint32_t TextureLoom::get_channel_count(ImageFormat format)
{
    switch (format) {
    case ImageFormat::R8:
    case ImageFormat::R16:
    case ImageFormat::R16F:
    case ImageFormat::R32F:
        return 1;

    case ImageFormat::RG8:
    case ImageFormat::RG16:
    case ImageFormat::RG16F:
    case ImageFormat::RG32F:
        return 2;

    case ImageFormat::RGB8:
        return 3;

    case ImageFormat::RGBA8:
    case ImageFormat::RGBA8_SRGB:
    case ImageFormat::BGRA8:
    case ImageFormat::RGBA16:
    case ImageFormat::RGBA16F:
    case ImageFormat::RGBA32F:
        return 4;

    case ImageFormat::DEPTH16:
    case ImageFormat::DEPTH24:
    case ImageFormat::DEPTH32F:
        return 1;

    case ImageFormat::DEPTH24_STENCIL8:
        return 2;

    default:
        return 0;
    }
}

} // namespace MayaFlux::Portal::Graphics
