#include "BackendResoureManager.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKCommandManager.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKContext.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Registry/Service/BufferService.hpp"

namespace MayaFlux::Core {

BackendResourceManager::BackendResourceManager(VKContext& context, VKCommandManager& command_manager)
    : m_context(context)
    , m_command_manager(command_manager)
{
}

void BackendResourceManager::setup_backend_service(const std::shared_ptr<Registry::Service::BufferService>& buffer_service)
{
    buffer_service->initialize_buffer = [this](const std::shared_ptr<void>& vk_buf) -> void {
        auto buffer = std::static_pointer_cast<Buffers::VKBuffer>(vk_buf);
        this->initialize_buffer(buffer);
    };

    buffer_service->destroy_buffer = [this](const std::shared_ptr<void>& vk_buf) {
        auto buffer = std::static_pointer_cast<Buffers::VKBuffer>(vk_buf);
        this->cleanup_buffer(buffer);
    };

    buffer_service->execute_immediate = [this](const std::function<void(void*)>& recorder) {
        this->execute_immediate_commands([recorder](vk::CommandBuffer cmd) {
            recorder(static_cast<void*>(cmd));
        });
    };

    buffer_service->record_deferred = [this](const std::function<void(void*)>& recorder) {
        this->record_deferred_commands([recorder](vk::CommandBuffer cmd) {
            recorder(static_cast<void*>(cmd));
        });
    };

    buffer_service->flush_range = [this](void* memory, size_t offset, size_t size) {
        vk::DeviceMemory mem(reinterpret_cast<VkDeviceMemory>(memory));
        vk::MappedMemoryRange range { mem, offset, size == 0 ? VK_WHOLE_SIZE : size };
        m_context.get_device().flushMappedMemoryRanges(1, &range);
    };

    buffer_service->invalidate_range = [this](void* memory, size_t offset, size_t size) {
        vk::DeviceMemory mem(reinterpret_cast<VkDeviceMemory>(memory));
        vk::MappedMemoryRange range { mem, offset, size == 0 ? VK_WHOLE_SIZE : size };
        m_context.get_device().invalidateMappedMemoryRanges(1, &range);
    };

    buffer_service->map_buffer = [this](void* memory, size_t offset, size_t size) -> void* {
        vk::DeviceMemory mem(reinterpret_cast<VkDeviceMemory>(memory));
        return m_context.get_device().mapMemory(mem, offset, size == 0 ? VK_WHOLE_SIZE : size);
    };

    buffer_service->unmap_buffer = [this](void* memory) {
        vk::DeviceMemory mem(reinterpret_cast<VkDeviceMemory>(memory));
        m_context.get_device().unmapMemory(mem);
    };
}

void BackendResourceManager::initialize_buffer(const std::shared_ptr<Buffers::VKBuffer>& buffer)
{
    if (!buffer) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Attempted to initialize null VulkanBuffer");
        return;
    }

    if (buffer->is_initialized()) {
        MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "VulkanBuffer already initialized, skipping");
        return;
    }

    vk::BufferCreateInfo buffer_info {};
    buffer_info.size = buffer->get_size_bytes();
    buffer_info.usage = buffer->get_usage_flags();
    buffer_info.sharingMode = vk::SharingMode::eExclusive;

    vk::Buffer vk_buffer;
    try {
        vk_buffer = m_context.get_device().createBuffer(buffer_info);
    } catch (const vk::SystemError& e) {
        error_rethrow(
            Journal::Component::Core,
            Journal::Context::GraphicsBackend,
            std::source_location::current(),
            "Failed to create VkBuffer: " + std::string(e.what()));
    }

    vk::MemoryRequirements mem_requirements;
    mem_requirements = m_context.get_device().getBufferMemoryRequirements(vk_buffer);

    vk::MemoryAllocateInfo alloc_info;
    alloc_info.allocationSize = mem_requirements.size;

    alloc_info.memoryTypeIndex = find_memory_type(
        mem_requirements.memoryTypeBits,
        vk::MemoryPropertyFlags(buffer->get_memory_properties()));

    vk::DeviceMemory memory;
    try {
        memory = m_context.get_device().allocateMemory(alloc_info);
    } catch (const vk::SystemError& e) {
        m_context.get_device().destroyBuffer(vk_buffer);
        error_rethrow(
            Journal::Component::Core,
            Journal::Context::GraphicsBackend,
            std::source_location::current(),
            "Failed to allocate VkDeviceMemory: " + std::string(e.what()));
    }

    try {
        m_context.get_device().bindBufferMemory(vk_buffer, memory, 0);
    } catch (const vk::SystemError& e) {
        m_context.get_device().freeMemory(memory);
        m_context.get_device().destroyBuffer(vk_buffer);

        error_rethrow(
            Journal::Component::Core,
            Journal::Context::GraphicsBackend,
            std::source_location::current(),
            "Failed to bind buffer memory: " + std::string(e.what()));
    }

    void* mapped_ptr = nullptr;
    if (buffer->is_host_visible()) {
        try {
            mapped_ptr = m_context.get_device().mapMemory(memory, 0, buffer->get_size_bytes());
        } catch (const vk::SystemError& e) {
            m_context.get_device().freeMemory(memory);
            m_context.get_device().destroyBuffer(vk_buffer);

            error_rethrow(
                Journal::Component::Core,
                Journal::Context::GraphicsBackend,
                std::source_location::current(),
                "Failed to map buffer memory: " + std::string(e.what()));
        }
    }

    Buffers::VKBufferResources resources { .buffer = vk_buffer, .memory = memory, .mapped_ptr = mapped_ptr };
    buffer->set_buffer_resources(resources);
    m_managed_buffers.push_back(buffer);

    MF_INFO(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "VulkanBuffer initialized: {} bytes, modality: {}, VkBuffer: {:p}",
        buffer->get_size_bytes(),
        Kakshya::modality_to_string(buffer->get_modality()),
        (void*)buffer->get_buffer());
}

void BackendResourceManager::cleanup_buffer(const std::shared_ptr<Buffers::VKBuffer>& buffer)
{
    if (!buffer) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Attempted to cleanup null VulkanBuffer");
        return;
    }

    auto it = std::ranges::find(m_managed_buffers, buffer);
    if (it == m_managed_buffers.end()) {
        return;
    }

    auto& [vk_buffer, memory, mapped_ptr] = it->get()->get_buffer_resources();

    if (mapped_ptr) {
        m_context.get_device().unmapMemory(memory);
    }

    if (vk_buffer) {
        m_context.get_device().destroyBuffer(vk_buffer);
    }

    if (memory) {
        m_context.get_device().freeMemory(memory);
    }

    m_managed_buffers.erase(it);

    MF_INFO(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "VulkanBuffer cleaned up: {:p}", (void*)vk_buffer);
}

void BackendResourceManager::flush_pending_buffer_operations()
{
    for (auto& buffer_wrapper : m_managed_buffers) {
        auto& resources = buffer_wrapper->get_buffer_resources();
        auto dirty_ranges = buffer_wrapper->get_and_clear_dirty_ranges();
        if (!dirty_ranges.empty()) {
            for (auto [offset, size] : dirty_ranges) {
                vk::MappedMemoryRange range;
                range.memory = resources.memory;
                range.offset = offset;
                range.size = size;
                m_context.get_device().flushMappedMemoryRanges(1, &range);
            }
            MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
                "Flushed {} dirty ranges for buffer {:p}", dirty_ranges.size(),
                (void*)buffer_wrapper->get_buffer());
        }

        auto invalid_ranges = buffer_wrapper->get_and_clear_invalid_ranges();
        if (!invalid_ranges.empty()) {
            for (auto [offset, size] : invalid_ranges) {
                vk::MappedMemoryRange range;
                range.memory = buffer_wrapper->get_buffer_resources().memory;
                range.offset = offset;
                range.size = size;
                m_context.get_device().invalidateMappedMemoryRanges(1, &range);
            }
            MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
                "Invalidated {} ranges for buffer {:p}", invalid_ranges.size(),
                (void*)buffer_wrapper->get_buffer());
        }
    }
}

void BackendResourceManager::initialize_image(const std::shared_ptr<VKImage>& image)
{
    if (!image) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Attempted to initialize null VKImage");
        return;
    }

    if (image->is_initialized()) {
        MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "VKImage already initialized, skipping");
        return;
    }

    // ========================================================================
    // Step 1: Create VkImage
    // ========================================================================

    vk::ImageCreateInfo image_info {};

    switch (image->get_type()) {
    case VKImage::Type::TYPE_1D:
        image_info.imageType = vk::ImageType::e1D;
        break;
    case VKImage::Type::TYPE_2D:
    case VKImage::Type::TYPE_CUBE:
        image_info.imageType = vk::ImageType::e2D;
        break;
    case VKImage::Type::TYPE_3D:
        image_info.imageType = vk::ImageType::e3D;
        break;
    }

    image_info.extent.width = image->get_width();
    image_info.extent.height = image->get_height();
    image_info.extent.depth = image->get_depth();
    image_info.mipLevels = image->get_mip_levels();
    image_info.arrayLayers = image->get_array_layers();
    image_info.format = image->get_format();
    image_info.tiling = vk::ImageTiling::eOptimal; // Optimal GPU tiling
    image_info.initialLayout = vk::ImageLayout::eUndefined;
    image_info.usage = image->get_usage_flags();
    image_info.sharingMode = vk::SharingMode::eExclusive;
    image_info.samples = vk::SampleCountFlagBits::e1; // No MSAA for now
    image_info.flags = (image->get_type() == VKImage::Type::TYPE_CUBE)
        ? vk::ImageCreateFlagBits::eCubeCompatible
        : vk::ImageCreateFlags {};

    vk::Image vk_image;
    try {
        vk_image = m_context.get_device().createImage(image_info);
    } catch (const vk::SystemError& e) {
        error_rethrow(
            Journal::Component::Core,
            Journal::Context::GraphicsBackend,
            std::source_location::current(),
            "Failed to create VkImage: " + std::string(e.what()));
    }

    // ========================================================================
    // Step 2: Allocate memory
    // ========================================================================

    vk::MemoryRequirements mem_requirements;
    mem_requirements = m_context.get_device().getImageMemoryRequirements(vk_image);

    vk::MemoryAllocateInfo alloc_info {};
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = find_memory_type(
        mem_requirements.memoryTypeBits,
        image->get_memory_properties());

    vk::DeviceMemory memory;
    try {
        memory = m_context.get_device().allocateMemory(alloc_info);
    } catch (const vk::SystemError& e) {
        m_context.get_device().destroyImage(vk_image);
        error_rethrow(
            Journal::Component::Core,
            Journal::Context::GraphicsBackend,
            std::source_location::current(),
            "Failed to allocate VkDeviceMemory for image: " + std::string(e.what()));
    }

    // ========================================================================
    // Step 3: Bind memory to image
    // ========================================================================

    try {
        m_context.get_device().bindImageMemory(vk_image, memory, 0);
    } catch (const vk::SystemError& e) {
        m_context.get_device().freeMemory(memory);
        m_context.get_device().destroyImage(vk_image);
        error_rethrow(
            Journal::Component::Core,
            Journal::Context::GraphicsBackend,
            std::source_location::current(),
            "Failed to bind memory to VkImage: " + std::string(e.what()));
    }

    // ========================================================================
    // Step 4: Create image view
    // ========================================================================

    vk::ImageViewCreateInfo view_info {};

    switch (image->get_type()) {
    case VKImage::Type::TYPE_1D:
        view_info.viewType = (image->get_array_layers() > 1)
            ? vk::ImageViewType::e1DArray
            : vk::ImageViewType::e1D;
        break;
    case VKImage::Type::TYPE_2D:
        view_info.viewType = (image->get_array_layers() > 1)
            ? vk::ImageViewType::e2DArray
            : vk::ImageViewType::e2D;
        break;
    case VKImage::Type::TYPE_3D:
        view_info.viewType = vk::ImageViewType::e3D;
        break;
    case VKImage::Type::TYPE_CUBE:
        view_info.viewType = vk::ImageViewType::eCube;
        break;
    }

    view_info.image = vk_image;
    view_info.format = image->get_format();
    view_info.subresourceRange.aspectMask = image->get_aspect_flags();
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = image->get_mip_levels();
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = image->get_array_layers();

    view_info.components.r = vk::ComponentSwizzle::eIdentity;
    view_info.components.g = vk::ComponentSwizzle::eIdentity;
    view_info.components.b = vk::ComponentSwizzle::eIdentity;
    view_info.components.a = vk::ComponentSwizzle::eIdentity;

    vk::ImageView image_view;
    try {
        image_view = m_context.get_device().createImageView(view_info);
    } catch (const vk::SystemError& e) {
        m_context.get_device().freeMemory(memory);
        m_context.get_device().destroyImage(vk_image);
        error_rethrow(
            Journal::Component::Core,
            Journal::Context::GraphicsBackend,
            std::source_location::current(),
            "Failed to create VkImageView: " + std::string(e.what()));
    }

    // ========================================================================
    // Step 5: Store handles in VKImage
    // ========================================================================

    VKImageResources resources {};
    resources.image = vk_image;
    resources.image_view = image_view;
    resources.memory = memory;
    resources.sampler = nullptr;

    image->set_image_resources(resources);
    image->set_current_layout(vk::ImageLayout::eUndefined);

    MF_INFO(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "VKImage initialized: {}x{}x{}, format: {}, {} mips, {} layers",
        image->get_width(), image->get_height(), image->get_depth(),
        vk::to_string(image->get_format()),
        image->get_mip_levels(), image->get_array_layers());
}

void BackendResourceManager::cleanup_image(const std::shared_ptr<VKImage>& image)
{
    if (!image || !image->is_initialized()) {
        return;
    }

    const auto& resources = image->get_image_resources();

    if (resources.image_view) {
        m_context.get_device().destroyImageView(resources.image_view);
    }

    if (resources.image) {
        m_context.get_device().destroyImage(resources.image);
    }

    if (resources.memory) {
        m_context.get_device().freeMemory(resources.memory);
    }

    MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "VKImage cleaned up");
}

void BackendResourceManager::transition_image_layout(
    vk::Image image,
    vk::ImageLayout old_layout,
    vk::ImageLayout new_layout,
    uint32_t mip_levels,
    uint32_t array_layers,
    vk::ImageAspectFlags aspect_flags)
{
    execute_immediate_commands([&](vk::CommandBuffer cmd) {
        vk::ImageMemoryBarrier barrier {};
        barrier.oldLayout = old_layout;
        barrier.newLayout = new_layout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = aspect_flags;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = mip_levels;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = array_layers;

        vk::PipelineStageFlags src_stage;
        vk::PipelineStageFlags dst_stage;

        if (old_layout == vk::ImageLayout::eUndefined && new_layout == vk::ImageLayout::eTransferDstOptimal) {
            barrier.srcAccessMask = vk::AccessFlags {};
            barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
            src_stage = vk::PipelineStageFlagBits::eTopOfPipe;
            dst_stage = vk::PipelineStageFlagBits::eTransfer;
        } else if (old_layout == vk::ImageLayout::eTransferDstOptimal && new_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
            barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
            barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
            src_stage = vk::PipelineStageFlagBits::eTransfer;
            dst_stage = vk::PipelineStageFlagBits::eFragmentShader;
        } else if (old_layout == vk::ImageLayout::eUndefined && new_layout == vk::ImageLayout::eColorAttachmentOptimal) {
            barrier.srcAccessMask = vk::AccessFlags {};
            barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
            src_stage = vk::PipelineStageFlagBits::eTopOfPipe;
            dst_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        } else if (old_layout == vk::ImageLayout::eUndefined && new_layout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
            barrier.srcAccessMask = vk::AccessFlags {};
            barrier.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
            src_stage = vk::PipelineStageFlagBits::eTopOfPipe;
            dst_stage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
        } else if (old_layout == vk::ImageLayout::eUndefined && new_layout == vk::ImageLayout::eGeneral) {
            barrier.srcAccessMask = vk::AccessFlags {};
            barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite;
            src_stage = vk::PipelineStageFlagBits::eTopOfPipe;
            dst_stage = vk::PipelineStageFlagBits::eComputeShader;
        } else {
            barrier.srcAccessMask = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite;
            barrier.dstAccessMask = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite;
            src_stage = vk::PipelineStageFlagBits::eAllCommands;
            dst_stage = vk::PipelineStageFlagBits::eAllCommands;

            MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
                "Using generic image layout transition");
        }

        cmd.pipelineBarrier(
            src_stage, dst_stage,
            vk::DependencyFlags {},
            0, nullptr, // Memory barriers
            0, nullptr, // Buffer barriers
            1, &barrier // Image barriers
        );
    });

    MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Image layout transitioned: {} -> {}",
        vk::to_string(old_layout), vk::to_string(new_layout));
}

void BackendResourceManager::upload_image_data(
    std::shared_ptr<VKImage> image,
    const void* data,
    size_t size)
{
    if (!image || !data) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Invalid parameters for upload_image_data");
        return;
    }

    // Create staging buffer
    auto staging = std::make_shared<Buffers::VKBuffer>(
        size,
        Buffers::VKBuffer::Usage::STAGING,
        Kakshya::DataModality::IMAGE_COLOR);

    initialize_buffer(staging);

    // Copy data to staging buffer
    void* mapped = staging->get_mapped_ptr();
    if (!mapped) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to map staging buffer for image upload");
        cleanup_buffer(staging);
        return;
    }

    std::memcpy(mapped, data, size);
    staging->mark_dirty_range(0, size);

    // Flush staging buffer
    auto& resources = staging->get_buffer_resources();
    vk::MappedMemoryRange range { resources.memory, 0, VK_WHOLE_SIZE };
    m_context.get_device().flushMappedMemoryRanges(1, &range);

    // Execute copy command
    execute_immediate_commands([&](vk::CommandBuffer cmd) {
        // Transition to transfer destination
        vk::ImageMemoryBarrier barrier {};
        barrier.oldLayout = image->get_current_layout();
        barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image->get_image();
        barrier.subresourceRange.aspectMask = image->get_aspect_flags();
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = image->get_mip_levels();
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = image->get_array_layers();
        barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        cmd.pipelineBarrier(
            vk::PipelineStageFlagBits::eFragmentShader,
            vk::PipelineStageFlagBits::eTransfer,
            vk::DependencyFlags {},
            0, nullptr, 0, nullptr, 1, &barrier);

        // Copy buffer to image
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

        cmd.copyBufferToImage(
            staging->get_buffer(),
            image->get_image(),
            vk::ImageLayout::eTransferDstOptimal,
            1, &region);

        // Transition to shader read
        barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        cmd.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eFragmentShader,
            vk::DependencyFlags {},
            0, nullptr, 0, nullptr, 1, &barrier);
    });

    image->set_current_layout(vk::ImageLayout::eShaderReadOnlyOptimal);
    cleanup_buffer(staging);

    MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Uploaded {} bytes to image {}x{}",
        size, image->get_width(), image->get_height());
}

void BackendResourceManager::download_image_data(
    std::shared_ptr<VKImage> image,
    void* data,
    size_t size)
{
    if (!image || !data) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Invalid parameters for download_image_data");
        return;
    }

    // Create staging buffer
    auto staging = std::make_shared<Buffers::VKBuffer>(
        size,
        Buffers::VKBuffer::Usage::STAGING,
        Kakshya::DataModality::IMAGE_COLOR);

    initialize_buffer(staging);

    // Execute copy command
    execute_immediate_commands([&](vk::CommandBuffer cmd) {
        // Transition to transfer source
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
            vk::DependencyFlags {},
            0, nullptr, 0, nullptr, 1, &barrier);

        // Copy image to buffer
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

        // Transition back to original layout
        barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
        barrier.newLayout = image->get_current_layout();
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        cmd.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eFragmentShader,
            vk::DependencyFlags {},
            0, nullptr, 0, nullptr, 1, &barrier);
    });

    // Invalidate and copy from staging to host
    staging->mark_invalid_range(0, size);
    auto& resources = staging->get_buffer_resources();
    vk::MappedMemoryRange range { resources.memory, 0, VK_WHOLE_SIZE };
    m_context.get_device().invalidateMappedMemoryRanges(1, &range);

    void* mapped = staging->get_mapped_ptr();
    if (mapped) {
        std::memcpy(data, mapped, size);
    }

    cleanup_buffer(staging);

    MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Downloaded {} bytes from image {}x{}",
        size, image->get_width(), image->get_height());
}

vk::Sampler BackendResourceManager::create_sampler(
    vk::Filter filter,
    vk::SamplerAddressMode address_mode,
    float max_anisotropy)
{
    size_t hash = 0;
    auto hash_combine = [](size_t& seed, size_t value) {
        seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    };

    hash_combine(hash, static_cast<size_t>(filter));
    hash_combine(hash, static_cast<size_t>(address_mode));
    hash_combine(hash, std::hash<float> {}(max_anisotropy));

    auto it = m_sampler_cache.find(hash);
    if (it != m_sampler_cache.end()) {
        MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Reusing cached sampler (hash: 0x{:X})", hash);
        return it->second;
    }

    vk::SamplerCreateInfo sampler_info;
    sampler_info.magFilter = filter;
    sampler_info.minFilter = filter;
    sampler_info.mipmapMode = vk::SamplerMipmapMode::eLinear;
    sampler_info.addressModeU = address_mode;
    sampler_info.addressModeV = address_mode;
    sampler_info.addressModeW = address_mode;
    sampler_info.mipLodBias = 0.0F;
    sampler_info.anisotropyEnable = max_anisotropy > 0.0F;
    sampler_info.maxAnisotropy = max_anisotropy;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.compareOp = vk::CompareOp::eAlways;
    sampler_info.minLod = 0.0F;
    sampler_info.maxLod = VK_LOD_CLAMP_NONE;
    sampler_info.borderColor = vk::BorderColor::eFloatOpaqueBlack;
    sampler_info.unnormalizedCoordinates = VK_FALSE;

    vk::Sampler sampler;
    try {
        sampler = m_context.get_device().createSampler(sampler_info);
    } catch (const vk::SystemError& e) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to create sampler: {}", e.what());
        return nullptr;
    }

    m_sampler_cache[hash] = sampler;

    MF_INFO(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Created sampler (filter: {}, address: {}, anisotropy: {}, hash: 0x{:X})",
        vk::to_string(filter), vk::to_string(address_mode), max_anisotropy, hash);

    return sampler;
}

void BackendResourceManager::destroy_sampler(vk::Sampler sampler)
{
    if (!sampler) {
        return;
    }

    for (auto it = m_sampler_cache.begin(); it != m_sampler_cache.end(); ++it) {
        if (it->second == sampler) {
            m_sampler_cache.erase(it);
            break;
        }
    }

    m_context.get_device().destroySampler(sampler);

    MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Destroyed sampler");
}

uint32_t BackendResourceManager::find_memory_type(uint32_t type_filter, vk::MemoryPropertyFlags properties) const
{
    vk::PhysicalDeviceMemoryProperties mem_properties;
    mem_properties = m_context.get_physical_device().getMemoryProperties();

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    error<std::runtime_error>(
        Journal::Component::Core,
        Journal::Context::GraphicsBackend,
        std::source_location::current(),
        "Failed to find suitable memory type");

    return 0;
}

void BackendResourceManager::execute_immediate_commands(const std::function<void(vk::CommandBuffer)>& recorder)
{
    vk::CommandBuffer cmd = m_command_manager.begin_single_time_commands();
    recorder(cmd);
    m_command_manager.end_single_time_commands(cmd, m_context.get_graphics_queue());
}

void BackendResourceManager::record_deferred_commands(const std::function<void(vk::CommandBuffer)>& recorder)
{
    // TODO: batch commands for later submission
    // For now, just execute immediately
    execute_immediate_commands(recorder);
}

void BackendResourceManager::cleanup()
{
    for (auto& [hash, sampler] : m_sampler_cache) {
        if (sampler) {
            m_context.get_device().destroySampler(sampler);
        }
    }
    m_sampler_cache.clear();

    for (auto& buffer : m_managed_buffers) {
        if (buffer && buffer->is_initialized()) {
            cleanup_buffer(buffer);
        }
    }
    m_managed_buffers.clear();
}

size_t BackendResourceManager::compute_sampler_hash(vk::Filter filter, vk::SamplerAddressMode address_mode, float max_anisotropy) const
{
    size_t hash = 0;
    auto hash_combine = [](size_t& seed, size_t value) {
        seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    };

    hash_combine(hash, static_cast<size_t>(filter));
    hash_combine(hash, static_cast<size_t>(address_mode));
    hash_combine(hash, std::hash<float> {}(max_anisotropy));

    return hash;
}

}
