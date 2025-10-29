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
     * @brief Present a rendered frame to window
     * @param window_handle Opaque window/surface handle
     *
     * Submits the current frame for presentation to the display.
     * Blocks until presentation completes or returns immediately
     * depending on vsync settings. Thread-safe.
     */
    // std::function<void(void*)> present_frame;
    std::function<void(const std::shared_ptr<void>&)> present_frame;

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
};

} // namespace MayaFlux::Registry::Services
