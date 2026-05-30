#include "SurfaceUtils.hpp"

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
     * @brief Reinterpret a raw byte blob as the element type matching the
     *        surface format and move it into out.
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
    if (!display)
        return s_fail;

    const auto window_handle = std::static_pointer_cast<void>(window);

    const int raw_fmt = display->get_swapchain_format(window_handle);
    const auto vk_fmt = static_cast<vk::Format>(raw_fmt);
    const auto mf_fmt = Core::from_vk_format(vk_fmt);
    const auto traits = Core::get_surface_format_traits(mf_fmt);
    const uint32_t bpp = Core::vk_format_bytes_per_pixel(vk_fmt);

    uint32_t full_w = 0, full_h = 0;
    display->get_swapchain_extent(window_handle, full_w, full_h);
    if (full_w == 0 || full_h == 0)
        return s_fail;

    auto frame = display->get_last_frame(window_handle);
    if (!frame || frame->empty()) {
        MF_RT_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "SurfaceUtils::readback_region: no captured frame for '{}'",
            window->get_create_info().title);
        return s_fail;
    }

    if (x_offset + pixel_width > full_w || y_offset + pixel_height > full_h) {
        MF_RT_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "SurfaceUtils::readback_region: region {}x{} at ({},{}) exceeds surface {}x{} for '{}'",
            pixel_width, pixel_height, x_offset, y_offset, full_w, full_h,
            window->get_create_info().title);
        return s_fail;
    }

    const size_t full_row = static_cast<size_t>(full_w) * bpp;
    const size_t region_row = static_cast<size_t>(pixel_width) * bpp;
    const size_t byte_count = region_row * pixel_height;

    const size_t expected = full_row * full_h;
    if (frame->size() < expected) {
        MF_RT_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "SurfaceUtils::readback_region: frame {} bytes, expected {} for '{}'",
            frame->size(), expected, window->get_create_info().title);
        return s_fail;
    }

    std::vector<uint8_t> region(byte_count);
    const uint8_t* base = frame->data()
        + static_cast<size_t>(y_offset) * full_row
        + static_cast<size_t>(x_offset) * bpp;

    for (uint32_t row = 0; row < pixel_height; ++row) {
        std::memcpy(region.data() + row * region_row,
            base + row * full_row,
            region_row);
    }

    fill_variant_from_raw(region.data(), byte_count, traits, out_variant);

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
