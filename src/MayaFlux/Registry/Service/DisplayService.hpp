#pragma once

namespace MayaFlux::Registry::Service {

/**
 * @brief Backend display and presentation service interface
 *
 * Manages window surfaces, swapchains, and frame presentation.
 * Handles window resize events and ensures proper surface recreation.
 */
struct MAYAFLUX_API DisplayService {

    /**
     * @brief Submit a primary command buffer and present the frame
     * @param window_handle Window handle
     * @param primary_command_buffer The primary command buffer (uint64_t bits of vk::CommandBuffer)
     *
     * Handles semaphore choreography: waits on image_available, signals render_finished,
     * then presents. Must be called after acquire_next_swapchain_image().
     */
    std::function<void(const std::shared_ptr<void>&, uint64_t)> submit_and_present;

    /**
     * @brief Wait for all GPU operations to complete
     *
     * Blocks until all submitted command buffers have finished execution.
     * Used for synchronization before shutdown or major state changes.
     * Can significantly impact performance - use sparingly.
     */
    std::function<void()> wait_idle;

    /**
     * @brief Resize rendering surface for a window
     * @param window_handle Window to resize
     * @param width New width in pixels
     * @param height New height in pixels
     *
     * Recreates swapchain and associated framebuffers for new dimensions.
     * Must be called when window size changes to avoid rendering artifacts.
     * Automatically waits for idle before reconstruction.
     */
    std::function<void(const std::shared_ptr<void>&, uint32_t, uint32_t)> resize_surface;

    /**
     * @brief Get current swapchain image count
     * @param window_handle Window handle
     * @return Number of images in the swapchain
     *
     * Useful for allocating per-frame resources. Typically 2-3 images
     * for double/triple buffering.
     */
    std::function<uint32_t(const std::shared_ptr<void>&)> get_swapchain_image_count;

    /**
     * @brief Acquire the next swapchain image for a window
     * @param window_handle Window handle
     * @return Index of the acquired swapchain image
     *
     * Must be called before get_current_image_view() for dynamic rendering.
     * Stores the acquired image index internally for subsequent calls.
     */
    std::function<uint64_t(const std::shared_ptr<void>&)> acquire_next_swapchain_image;

    /**
     * @brief Get actual swapchain format for a window
     * @param window_handle Window handle
     * @return Vulkan format (vk::Format cast to uint32_t)
     *
     * Returns the actual format used by the window's swapchain.
     * Used to ensure multiple dynamic render calls are compatible
     */
    std::function<int(const std::shared_ptr<void>&)> get_swapchain_format;

    /**
     * @brief Get swapchain extent for a window
     * @param window_handle Window handle
     * @param out_width Output parameter for width
     * @param out_height Output parameter for height
     *
     * Retrieves current swapchain dimensions. Sets out_width and out_height
     * to 0 if window not registered or swapchain unavailable.
     */
    std::function<void(const std::shared_ptr<void>&, uint32_t&, uint32_t&)> get_swapchain_extent;

    /**
     * @brief Get current swapchain image view for rendering
     * @param window_handle Window handle
     * @return vk::ImageView cast to void*
     *
     * Returns the image view for the currently acquired swapchain image.
     * Used with dynamic rendering.
     */
    std::function<void*(const std::shared_ptr<void>&)> get_current_image_view;
};

} // namespace MayaFlux::Registry::Services
