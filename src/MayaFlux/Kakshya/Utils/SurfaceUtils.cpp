#include "SurfaceUtils.hpp"

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Core/Backends/Windowing/Window.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kakshya/NDData/DataAccess.hpp"
#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/BufferService.hpp"
#include "MayaFlux/Registry/Service/DisplayService.hpp"

#include <vulkan/vulkan.hpp>

namespace MayaFlux::Kakshya {

namespace {

    /**
     * @brief Resolve DisplayService from the backend registry.
     *        Returns nullptr and logs on failure.
     */
    Registry::Service::DisplayService* get_display_service()
    {
        auto* svc = Registry::BackendRegistry::instance()
                        .get_service<Registry::Service::DisplayService>();
        if (!svc) {
            MF_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
                "SurfaceUtils: DisplayService not available");
        }
        return svc;
    }

    /**
     * @brief Resolve BufferService from the backend registry.
     *        Returns nullptr and logs on failure.
     */
    Registry::Service::BufferService* get_buffer_service()
    {
        auto* svc = Registry::BackendRegistry::instance()
                        .get_service<Registry::Service::BufferService>();
        if (!svc) {
            MF_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
                "SurfaceUtils: BufferService not available");
        }
        return svc;
    }

    /**
     * @brief Build DataDimension vector for a pixel readback result.
     *        Follows IMAGE_COLOR convention: [height, width, channels].
     */
    std::vector<DataDimension> make_pixel_dimensions(
        uint32_t pixel_width, uint32_t pixel_height, uint32_t channel_count)
    {
        return DataDimension::create_dimensions(
            DataModality::IMAGE_COLOR,
            { static_cast<uint64_t>(pixel_height),
                static_cast<uint64_t>(pixel_width),
                static_cast<uint64_t>(channel_count) },
            MemoryLayout::ROW_MAJOR);
    }

} // namespace

// =========================================================================
// Public API
// =========================================================================

DataAccess readback_region(
    const std::shared_ptr<Core::Window>& window,
    uint32_t x_offset,
    uint32_t y_offset,
    uint32_t pixel_width,
    uint32_t pixel_height,
    uint32_t channel_count,
    DataVariant& out_variant)
{
    static std::vector<DataDimension> s_empty_dims;
    static DataVariant s_empty_var = std::vector<uint8_t> {};
    static DataAccess s_fail { s_empty_var, s_empty_dims, DataModality::UNKNOWN };

    auto* display = get_display_service();
    auto* buf_svc = get_buffer_service();
    if (!display || !buf_svc)
        return s_fail;

    auto window_handle = std::static_pointer_cast<void>(window);

    const uint64_t image_bits = display->get_current_swapchain_image(window_handle);
    if (image_bits == 0) {
        MF_RT_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "SurfaceUtils::readback_region: no current swapchain image for '{}'",
            window->get_create_info().title);
        return s_fail;
    }

    auto image = vk::Image(reinterpret_cast<VkImage>(image_bits));

    const size_t byte_count = static_cast<size_t>(pixel_width) * pixel_height * channel_count;

    auto staging = std::make_shared<Buffers::VKBuffer>(
        byte_count,
        Buffers::VKBuffer::Usage::STAGING,
        DataModality::IMAGE_COLOR);

    buf_svc->initialize_buffer(std::static_pointer_cast<void>(staging));

    bool copy_ok = false;

    buf_svc->execute_immediate([&](void* raw_cmd) {
        auto cmd = static_cast<vk::CommandBuffer>(reinterpret_cast<VkCommandBuffer>(raw_cmd));

        vk::ImageMemoryBarrier to_transfer;
        to_transfer.oldLayout = vk::ImageLayout::ePresentSrcKHR;
        to_transfer.newLayout = vk::ImageLayout::eTransferSrcOptimal;
        to_transfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        to_transfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        to_transfer.image = image;
        to_transfer.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
        to_transfer.srcAccessMask = vk::AccessFlagBits::eMemoryRead;
        to_transfer.dstAccessMask = vk::AccessFlagBits::eTransferRead;

        cmd.pipelineBarrier(
            vk::PipelineStageFlagBits::eBottomOfPipe,
            vk::PipelineStageFlagBits::eTransfer,
            {}, {}, {}, to_transfer);

        vk::BufferImageCopy region;
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
        region.imageOffset = vk::Offset3D { static_cast<int32_t>(x_offset), static_cast<int32_t>(y_offset), 0 };
        region.imageExtent = vk::Extent3D { pixel_width, pixel_height, 1U };

        cmd.copyImageToBuffer(image, vk::ImageLayout::eTransferSrcOptimal, staging->get_buffer(), region);

        vk::ImageMemoryBarrier to_present;
        to_present.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
        to_present.newLayout = vk::ImageLayout::ePresentSrcKHR;
        to_present.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        to_present.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        to_present.image = image;
        to_present.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
        to_present.srcAccessMask = vk::AccessFlagBits::eTransferRead;
        to_present.dstAccessMask = vk::AccessFlagBits::eMemoryRead;

        cmd.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eTopOfPipe,
            {}, {}, {}, to_present);

        copy_ok = true;
    });

    if (!copy_ok) {
        buf_svc->destroy_buffer(std::static_pointer_cast<void>(staging));
        MF_RT_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "SurfaceUtils::readback_region: GPU copy failed for '{}'",
            window->get_create_info().title);
        return s_fail;
    }

    const auto& res = staging->get_buffer_resources();
    std::vector<uint8_t> pixels(byte_count);

    buf_svc->invalidate_range(res.memory, 0, byte_count);
    void* mapped = buf_svc->map_buffer(res.memory, 0, byte_count);
    if (mapped) {
        std::memcpy(pixels.data(), mapped, byte_count);
        buf_svc->unmap_buffer(res.memory);
    } else {
        buf_svc->destroy_buffer(std::static_pointer_cast<void>(staging));
        MF_RT_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "SurfaceUtils::readback_region: staging map failed for '{}'",
            window->get_create_info().title);
        return s_fail;
    }

    buf_svc->destroy_buffer(std::static_pointer_cast<void>(staging));

    out_variant = std::move(pixels);
    auto dims = make_pixel_dimensions(pixel_width, pixel_height, channel_count);
    return { out_variant, dims, DataModality::IMAGE_COLOR };
}

std::pair<uint32_t, uint32_t> query_surface_extent(
    const std::shared_ptr<Core::Window>& window)
{
    auto* svc = get_display_service();
    if (!svc)
        return { 0U, 0U };

    uint32_t w = 0, h = 0;
    svc->get_swapchain_extent(std::static_pointer_cast<void>(window), w, h);
    return { w, h };
}

bool is_readback_available(const std::shared_ptr<Core::Window>& window)
{
    auto* svc = get_display_service();
    if (!svc)
        return false;

    uint32_t w = 0, h = 0;
    svc->get_swapchain_extent(std::static_pointer_cast<void>(window), w, h);
    return w > 0 && h > 0;
}

} // namespace MayaFlux::Kakshya
