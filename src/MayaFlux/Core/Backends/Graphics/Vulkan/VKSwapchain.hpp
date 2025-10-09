#pragma once

#include "VKContext.hpp"

namespace MayaFlux::Core {

class Window;

/**
 * @struct SwapchainSupportDetails
 * @brief Holds swapchain capability information for a physical device
 */
struct SwapchainSupportDetails {
    vk::SurfaceCapabilitiesKHR capabilities;
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR> present_modes;

    [[nodiscard]] bool is_adequate() const
    {
        return !formats.empty() && !present_modes.empty();
    }
};

/**
 * @class VKSwapchain
 * @brief Manages Vulkan swapchain using C++ API (vulkan.hpp)
 */
class VKSwapchain {
public:
    VKSwapchain() = default;
    ~VKSwapchain();

    VKSwapchain(const VKSwapchain&) = delete;
    VKSwapchain& operator=(const VKSwapchain&) = delete;

    /**
     * @brief Create swapchain for the given surface using VKContext
     * @param context Vulkan context (provides device and configuration)
     * @param surface Window surface
     * @param window_config per-window configuration overrides
     * @return True if creation succeeded
     */
    bool create(VKContext& context, vk::SurfaceKHR surface, const WindowCreateInfo& window_config);

    /**
     * @brief Recreate swapchain (for window resize)
     * @param width New width
     * @param height New height
     * @return True if recreation succeeded
     */
    bool recreate(uint32_t width, uint32_t height);

    /**
     * @brief Cleanup swapchain resources
     */
    void cleanup();

    /**
     * @brief Acquire next image from swapchain
     * @param signal_semaphore Semaphore to signal when image is acquired
     * @param timeout_ns Timeout in nanoseconds (UINT64_MAX = no timeout)
     * @return Image index, or empty if acquisition failed
     */
    std::optional<uint32_t> acquire_next_image(
        vk::Semaphore signal_semaphore,
        uint64_t timeout_ns = UINT64_MAX);

    /**
     * @brief Present image to screen
     * @param image_index Index of image to present
     * @param wait_semaphore Semaphore to wait on before presenting
     * @param present_queue Queue to submit present operation to (optional, uses context's graphics queue if null)
     * @return True if present succeeded, false if swapchain out of date
     */
    bool present(uint32_t image_index,
        vk::Semaphore wait_semaphore,
        vk::Queue present_queue = nullptr);

    // Getters
    [[nodiscard]] vk::SwapchainKHR get_swapchain() const { return m_swapchain; }
    [[nodiscard]] vk::Format get_image_format() const { return m_image_format; }
    [[nodiscard]] vk::Extent2D get_extent() const { return m_extent; }
    [[nodiscard]] uint32_t get_image_count() const { return static_cast<uint32_t>(m_images.size()); }
    [[nodiscard]] const std::vector<vk::Image>& get_images() const { return m_images; }
    [[nodiscard]] const std::vector<vk::ImageView>& get_image_views() const { return m_image_views; }

    /**
     * @brief Query swapchain support details for a device
     */
    static SwapchainSupportDetails query_support(
        vk::PhysicalDevice physical_device,
        vk::SurfaceKHR surface);

private:
    VKContext* m_context = nullptr; // Non-owning pointer to context
    vk::SurfaceKHR m_surface;
    const WindowCreateInfo* m_window_config = nullptr; // Store for recreation

    vk::SwapchainKHR m_swapchain;
    std::vector<vk::Image> m_images;
    std::vector<vk::ImageView> m_image_views;

    vk::Format m_image_format;
    vk::Extent2D m_extent;

    /**
     * @brief Choose best surface format from available formats based on config
     */
    [[nodiscard]] vk::SurfaceFormatKHR choose_surface_format(
        const std::vector<vk::SurfaceFormatKHR>& available_formats,
        GraphicsSurfaceInfo::SurfaceFormat desired_format,
        GraphicsSurfaceInfo::ColorSpace desired_color_space) const;

    /**
     * @brief Choose best present mode from available modes based on config
     */
    [[nodiscard]] vk::PresentModeKHR choose_present_mode(
        const std::vector<vk::PresentModeKHR>& available_modes,
        GraphicsSurfaceInfo::PresentMode desired_mode) const;

    /**
     * @brief Choose swap extent based on capabilities
     */
    [[nodiscard]] vk::Extent2D choose_extent(
        const vk::SurfaceCapabilitiesKHR& capabilities,
        uint32_t width,
        uint32_t height) const;

    /**
     * @brief Find an HDR-capable format from available formats
     */
    [[nodiscard]] std::optional<vk::SurfaceFormatKHR> find_hdr_format(
        const std::vector<vk::SurfaceFormatKHR>& available_formats) const;

    /**
     * @brief Create image views for swapchain images
     */
    bool create_image_views();

    /**
     * @brief Cleanup old swapchain during recreation
     */
    void cleanup_swapchain();
};

} // namespace MayaFlux::Core
