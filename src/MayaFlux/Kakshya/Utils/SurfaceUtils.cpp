#include "SurfaceUtils.hpp"

#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Kakshya/NDData/DataAccess.hpp"

#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/BufferService.hpp"
#include "MayaFlux/Registry/Service/DisplayService.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKEnumUtils.hpp"
#include "MayaFlux/Core/Backends/Windowing/Window.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

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

    /**
     * @brief Allocate the correctly-typed DataVariant and copy raw bytes into it.
     *
     * The swapchain readback yields a contiguous byte blob. This function
     * reinterprets that blob as the element type that matches the surface
     * format, avoiding a lossy conversion to uint8_t for HDR/float formats.
     *
     * @param raw        Pointer to the mapped staging buffer.
     * @param byte_count Total byte count of the readback region.
     * @param traits     Format traits for the live swapchain surface.
     * @param out        DataVariant to receive the typed data.
     */
    void fill_variant_from_raw(
        const void* raw,
        size_t byte_count,
        const Core::SurfaceFormatTraits& traits,
        DataVariant& out)
    {
        if (traits.is_float && traits.bits_per_channel == 32U) {
            const size_t n = byte_count / sizeof(float);
            std::vector<float> v(n);
            std::memcpy(v.data(), raw, byte_count);
            out = std::move(v);
        } else if (traits.bits_per_channel == 16U) {
            const size_t n = byte_count / sizeof(uint16_t);
            std::vector<uint16_t> v(n);
            std::memcpy(v.data(), raw, byte_count);
            out = std::move(v);
        } else if (traits.is_packed) {
            const size_t n = byte_count / sizeof(uint32_t);
            std::vector<uint32_t> v(n);
            std::memcpy(v.data(), raw, byte_count);
            out = std::move(v);
        } else {
            std::vector<uint8_t> v(byte_count);
            std::memcpy(v.data(), raw, byte_count);
            out = std::move(v);
        }
    }

} // namespace

// =========================================================================
// Public API
// =========================================================================

Core::GraphicsSurfaceInfo::SurfaceFormat query_surface_format(
    const std::shared_ptr<Core::Window>& window)
{
    auto* svc = get_display_service();
    if (!svc)
        return Core::GraphicsSurfaceInfo::SurfaceFormat::B8G8R8A8_SRGB;

    const int raw = svc->get_swapchain_format(std::static_pointer_cast<void>(window));
    return Core::from_vk_format(static_cast<vk::Format>(raw));
}

DataAccess readback_region(
    const std::shared_ptr<Core::Window>& window,
    uint32_t x_offset,
    uint32_t y_offset,
    uint32_t pixel_width,
    uint32_t pixel_height,
    DataVariant& out_variant)
{
    static std::vector<DataDimension> s_empty_dims;
    static DataVariant s_empty_var = std::vector<uint8_t> {};
    static DataAccess s_fail { s_empty_var, s_empty_dims, DataModality::UNKNOWN };

    auto* display = get_display_service();
    auto* buf_svc = get_buffer_service();
    if (!display || !buf_svc)
        return s_fail;

    const auto window_handle = std::static_pointer_cast<void>(window);

    const uint64_t image_bits = display->get_current_swapchain_image(window_handle);
    if (image_bits == 0) {
        MF_RT_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "SurfaceUtils::readback_region: no current swapchain image for '{}'",
            window->get_create_info().title);
        return s_fail;
    }

    const int raw_fmt = display->get_swapchain_format(window_handle);
    const auto vk_fmt = static_cast<vk::Format>(raw_fmt);
    const auto mf_fmt = Core::from_vk_format(vk_fmt);
    const auto traits = Core::get_surface_format_traits(mf_fmt);
    const uint32_t bpp = Core::vk_format_bytes_per_pixel(vk_fmt);

    const size_t byte_count = static_cast<size_t>(pixel_width) * pixel_height * bpp;

    auto image = vk::Image(reinterpret_cast<VkImage>(image_bits));

    auto staging = Buffers::create_staging_buffer(byte_count);
    if (!staging) {
        MF_RT_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "SurfaceUtils::readback_region: staging allocation failed ({} bytes) for '{}'",
            byte_count, window->get_create_info().title);
        return s_fail;
    }

    bool copy_ok = false;

    buf_svc->execute_immediate([&](void* raw_cmd) {
        auto cmd = static_cast<vk::CommandBuffer>(reinterpret_cast<VkCommandBuffer>(raw_cmd));

        vk::ImageMemoryBarrier to_src;
        to_src.oldLayout = vk::ImageLayout::ePresentSrcKHR;
        to_src.newLayout = vk::ImageLayout::eTransferSrcOptimal;
        to_src.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        to_src.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        to_src.image = image;
        to_src.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
        to_src.srcAccessMask = vk::AccessFlagBits::eMemoryRead;
        to_src.dstAccessMask = vk::AccessFlagBits::eTransferRead;

        cmd.pipelineBarrier(
            vk::PipelineStageFlagBits::eBottomOfPipe,
            vk::PipelineStageFlagBits::eTransfer,
            {}, {}, {}, to_src);

        vk::BufferImageCopy region {};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
        region.imageOffset = vk::Offset3D {
            static_cast<int32_t>(x_offset),
            static_cast<int32_t>(y_offset),
            0
        };
        region.imageExtent = vk::Extent3D { pixel_width, pixel_height, 1U };

        cmd.copyImageToBuffer(image, vk::ImageLayout::eTransferSrcOptimal,
            staging->get_buffer(), region);

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

    buf_svc->invalidate_range(res.memory, 0, byte_count);
    const void* mapped = buf_svc->map_buffer(res.memory, 0, byte_count);
    if (!mapped) {
        buf_svc->destroy_buffer(std::static_pointer_cast<void>(staging));
        MF_RT_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "SurfaceUtils::readback_region: staging map failed for '{}'",
            window->get_create_info().title);
        return s_fail;
    }

    fill_variant_from_raw(mapped, byte_count, traits, out_variant);
    buf_svc->unmap_buffer(res.memory);
    buf_svc->destroy_buffer(std::static_pointer_cast<void>(staging));

    auto dims = make_pixel_dimensions(pixel_width, pixel_height, traits.channel_count);
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
