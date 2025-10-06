#include "VKSwapchain.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Core {

VKSwapchain::~VKSwapchain()
{
    cleanup();
}

bool VKSwapchain::create(VKContext& context,
    vk::SurfaceKHR surface,
    const WindowCreateInfo& window_config)
{
    m_context = &context;
    m_surface = surface;

    const GraphicsSurfaceInfo& surface_info = m_context->get_surface_info();

    GraphicsSurfaceInfo::SurfaceFormat desired_format = window_config.surface_format.has_value()
        ? window_config.surface_format.value()
        : surface_info.default_format;

    GraphicsSurfaceInfo::ColorSpace desired_color_space = surface_info.default_color_space;

    GraphicsSurfaceInfo::PresentMode desired_present_mode = window_config.present_mode.has_value()
        ? window_config.present_mode.value()
        : surface_info.default_present_mode;

    uint32_t desired_image_count = surface_info.preferred_image_count;

    vk::PhysicalDevice physical_device = m_context->get_physical_device();
    vk::Device device = m_context->get_device();

    SwapchainSupportDetails support = query_support(physical_device, surface);

    if (!support.is_adequate()) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Swapchain support inadequate (formats: {}, modes: {})",
            support.formats.size(), support.present_modes.size());
        return false;
    }

    vk::SurfaceFormatKHR surface_format = choose_surface_format(
        support.formats, desired_format, desired_color_space);

    MF_INFO(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Chosen swapchain format: {}, color space: {}",
        vk::to_string(surface_format.format),
        vk::to_string(surface_format.colorSpace));

    vk::PresentModeKHR present_mode = choose_present_mode(
        support.present_modes, desired_present_mode);
    vk::Extent2D extent = choose_extent(support.capabilities, window_config.width, window_config.height);

    uint32_t image_count = std::max(desired_image_count, support.capabilities.minImageCount);
    if (support.capabilities.maxImageCount > 0) {
        image_count = std::min(image_count, support.capabilities.maxImageCount);
    }

    vk::SwapchainCreateInfoKHR create_info {};
    create_info.surface = surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst;

    const auto& queue_families = m_context->get_queue_families();

    std::vector<uint32_t> queue_family_indices;
    uint32_t graphics_family = queue_families.graphics_family.value();
    uint32_t present_family = queue_families.present_family.value();

    if (graphics_family != present_family) {
        queue_family_indices = { graphics_family, present_family };
        create_info.imageSharingMode = vk::SharingMode::eConcurrent;
        create_info.queueFamilyIndexCount = static_cast<uint32_t>(queue_family_indices.size());
        create_info.pQueueFamilyIndices = queue_family_indices.data();
    } else {
        create_info.imageSharingMode = vk::SharingMode::eExclusive;
        create_info.queueFamilyIndexCount = 0;
        create_info.pQueueFamilyIndices = nullptr;
    }

    create_info.preTransform = support.capabilities.currentTransform;
    create_info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = nullptr;

    try {
        m_swapchain = device.createSwapchainKHR(create_info);
    } catch (const vk::SystemError& e) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to create swapchain: {}", e.what());
        return false;
    }

    m_images = device.getSwapchainImagesKHR(m_swapchain);

    m_image_format = surface_format.format;
    m_extent = extent;

    m_window_config = &window_config;

    if (!create_image_views()) {
        cleanup_swapchain();
        return false;
    }

    MF_INFO(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Swapchain created: {}x{}, {} images, format {}",
        extent.width, extent.height, m_images.size(),
        vk::to_string(surface_format.format));

    return true;
}

bool VKSwapchain::recreate(uint32_t width, uint32_t height)
{
    if (!m_context) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot recreate swapchain: no context set");
        return false;
    }

    m_context->get_device().waitIdle();

    cleanup_swapchain();

    return create(*m_context, m_surface, *m_window_config);
}

void VKSwapchain::cleanup()
{
    cleanup_swapchain();
    m_context = nullptr;
}

void VKSwapchain::cleanup_swapchain()
{
    if (!m_context) {
        return;
    }

    vk::Device device = m_context->get_device();

    for (auto image_view : m_image_views) {
        device.destroyImageView(image_view);
    }
    m_image_views.clear();

    if (m_swapchain) {
        device.destroySwapchainKHR(m_swapchain);
        m_swapchain = nullptr;
    }

    m_images.clear();
}

std::optional<uint32_t> VKSwapchain::acquire_next_image(
    vk::Semaphore signal_semaphore,
    uint64_t timeout_ns)
{
    if (!m_context) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot acquire image: no context set");
        return std::nullopt;
    }

    vk::Device device = m_context->get_device();

    try {
        auto result = device.acquireNextImageKHR(
            m_swapchain,
            timeout_ns,
            signal_semaphore,
            nullptr);

        if (result.result == vk::Result::eErrorOutOfDateKHR) {
            return std::nullopt;
        }

        if (result.result != vk::Result::eSuccess && result.result != vk::Result::eSuboptimalKHR) {
            MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
                "Failed to acquire swapchain image");
            return std::nullopt;
        }

        return result.value;

    } catch (const vk::SystemError& e) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Exception acquiring swapchain image: {}", e.what());
        return std::nullopt;
    }
}

bool VKSwapchain::present(uint32_t image_index,
    vk::Semaphore wait_semaphore,
    vk::Queue present_queue)
{
    if (!m_context) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot present: no context set");
        return false;
    }

    vk::Queue queue = present_queue ? present_queue : m_context->get_graphics_queue();

    vk::PresentInfoKHR present_info {};
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &wait_semaphore;

    present_info.swapchainCount = 1;
    present_info.pSwapchains = &m_swapchain;
    present_info.pImageIndices = &image_index;
    present_info.pResults = nullptr;

    try {
        auto result = queue.presentKHR(present_info);

        if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR) {
            return false;
        }

        if (result != vk::Result::eSuccess) {
            MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
                "Failed to present swapchain image");
            return false;
        }

        return true;

    } catch (const vk::SystemError& e) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Exception presenting swapchain image: {}", e.what());
        return false;
    }
}

bool VKSwapchain::create_image_views()
{
    if (!m_context) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot create image views: no context set");
        return false;
    }

    vk::Device device = m_context->get_device();
    m_image_views.resize(m_images.size());

    for (size_t i = 0; i < m_images.size(); i++) {
        vk::ImageViewCreateInfo create_info {};
        create_info.image = m_images[i];
        create_info.viewType = vk::ImageViewType::e2D;
        create_info.format = m_image_format;

        create_info.components.r = vk::ComponentSwizzle::eIdentity;
        create_info.components.g = vk::ComponentSwizzle::eIdentity;
        create_info.components.b = vk::ComponentSwizzle::eIdentity;
        create_info.components.a = vk::ComponentSwizzle::eIdentity;

        create_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        create_info.subresourceRange.baseMipLevel = 0;
        create_info.subresourceRange.levelCount = 1;
        create_info.subresourceRange.baseArrayLayer = 0;
        create_info.subresourceRange.layerCount = 1;

        try {
            m_image_views[i] = device.createImageView(create_info);
        } catch (const vk::SystemError& e) {
            MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
                "Failed to create image view {}: {}", i, e.what());
            return false;
        }
    }

    return true;
}

SwapchainSupportDetails VKSwapchain::query_support(
    vk::PhysicalDevice physical_device,
    vk::SurfaceKHR surface)
{
    SwapchainSupportDetails details;

    details.capabilities = physical_device.getSurfaceCapabilitiesKHR(surface);
    details.formats = physical_device.getSurfaceFormatsKHR(surface);
    details.present_modes = physical_device.getSurfacePresentModesKHR(surface);

    return details;
}

vk::SurfaceFormatKHR VKSwapchain::choose_surface_format(
    const std::vector<vk::SurfaceFormatKHR>& available_formats,
    GraphicsSurfaceInfo::SurfaceFormat desired_format,
    GraphicsSurfaceInfo::ColorSpace desired_color_space) const
{
    auto to_vk_format = [](GraphicsSurfaceInfo::SurfaceFormat fmt) -> vk::Format {
        switch (fmt) {
        case GraphicsSurfaceInfo::SurfaceFormat::B8G8R8A8_SRGB:
            return vk::Format::eB8G8R8A8Srgb;
        case GraphicsSurfaceInfo::SurfaceFormat::R8G8B8A8_SRGB:
            return vk::Format::eR8G8B8A8Srgb;
        case GraphicsSurfaceInfo::SurfaceFormat::B8G8R8A8_UNORM:
            return vk::Format::eB8G8R8A8Unorm;
        case GraphicsSurfaceInfo::SurfaceFormat::R8G8B8A8_UNORM:
            return vk::Format::eR8G8B8A8Unorm;
        case GraphicsSurfaceInfo::SurfaceFormat::R16G16B16A16_SFLOAT:
            return vk::Format::eR16G16B16A16Sfloat;
        case GraphicsSurfaceInfo::SurfaceFormat::A2B10G10R10_UNORM:
            return vk::Format::eA2B10G10R10UnormPack32;
        case GraphicsSurfaceInfo::SurfaceFormat::R32G32B32A32_SFLOAT:
            return vk::Format::eR32G32B32A32Sfloat;
        default:
            return vk::Format::eB8G8R8A8Srgb;
        }
    };

    auto to_vk_color_space = [](GraphicsSurfaceInfo::ColorSpace space) -> vk::ColorSpaceKHR {
        switch (space) {
        case GraphicsSurfaceInfo::ColorSpace::SRGB_NONLINEAR:
            return vk::ColorSpaceKHR::eSrgbNonlinear;
        case GraphicsSurfaceInfo::ColorSpace::EXTENDED_SRGB:
            return vk::ColorSpaceKHR::eExtendedSrgbLinearEXT;
        case GraphicsSurfaceInfo::ColorSpace::HDR10_ST2084:
            return vk::ColorSpaceKHR::eHdr10St2084EXT;
        case GraphicsSurfaceInfo::ColorSpace::DISPLAY_P3:
            return vk::ColorSpaceKHR::eDisplayP3NonlinearEXT;
        default:
            return vk::ColorSpaceKHR::eSrgbNonlinear;
        }
    };

    vk::Format vk_format = to_vk_format(desired_format);
    vk::ColorSpaceKHR vk_color_space = to_vk_color_space(desired_color_space);

    for (const auto& format : available_formats) {
        if (format.format == vk_format && format.colorSpace == vk_color_space) {
            return format;
        }
    }

    for (const auto& format : available_formats) {
        if (format.format == vk_format) {
            MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
                "Exact color space match not found, using format match with different color space");
            return format;
        }
    }

    MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Desired format not available, falling back to: {}",
        vk::to_string(available_formats[0].format));
    return available_formats[0];
}

vk::PresentModeKHR VKSwapchain::choose_present_mode(
    const std::vector<vk::PresentModeKHR>& available_modes,
    GraphicsSurfaceInfo::PresentMode desired_mode) const
{
    auto to_vk_present_mode = [](GraphicsSurfaceInfo::PresentMode mode) -> vk::PresentModeKHR {
        switch (mode) {
        case GraphicsSurfaceInfo::PresentMode::IMMEDIATE:
            return vk::PresentModeKHR::eImmediate;
        case GraphicsSurfaceInfo::PresentMode::MAILBOX:
            return vk::PresentModeKHR::eMailbox;
        case GraphicsSurfaceInfo::PresentMode::FIFO:
            return vk::PresentModeKHR::eFifo;
        case GraphicsSurfaceInfo::PresentMode::FIFO_RELAXED:
            return vk::PresentModeKHR::eFifoRelaxed;
        default:
            return vk::PresentModeKHR::eFifo;
        }
    };

    vk::PresentModeKHR vk_mode = to_vk_present_mode(desired_mode);

    for (const auto& mode : available_modes) {
        if (mode == vk_mode) {
            return mode;
        }
    }

    if (desired_mode == GraphicsSurfaceInfo::PresentMode::IMMEDIATE || desired_mode == GraphicsSurfaceInfo::PresentMode::MAILBOX) {
        for (const auto& mode : available_modes) {
            if (mode == vk::PresentModeKHR::eMailbox) {
                MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
                    "Desired present mode not available, using MAILBOX");
                return mode;
            }
        }
        for (const auto& mode : available_modes) {
            if (mode == vk::PresentModeKHR::eImmediate) {
                MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
                    "Desired present mode not available, using IMMEDIATE");
                return mode;
            }
        }
    }

    MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Desired present mode not available, falling back to FIFO (VSync)");
    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D VKSwapchain::choose_extent(
    const vk::SurfaceCapabilitiesKHR& capabilities,
    uint32_t width,
    uint32_t height) const
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }
    vk::Extent2D actual_extent = { width, height };

    actual_extent.width = std::clamp(
        actual_extent.width,
        capabilities.minImageExtent.width,
        capabilities.maxImageExtent.width);

    actual_extent.height = std::clamp(
        actual_extent.height,
        capabilities.minImageExtent.height,
        capabilities.maxImageExtent.height);

    return actual_extent;
}

} // namespace MayaFlux::Core
