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
     * @param command_buffer_handle Opaque command buffer handle with rendering commands
     *
     * Submits the current frame for presentation to the display.
     * Blocks until presentation completes or returns immediately
     * depending on vsync settings. Thread-safe.
     */
    std::function<void(const std::shared_ptr<void>&, uint64_t)> present_frame;

    /**
     * @brief Present a complete frame with multiple command buffers (RECOMMENDED)
     * @param window_handle Opaque window/surface handle
     * @param command_buffers Vector of Vulkan command buffers to submit together
     *
     * Batches multiple command buffers into a single frame submission.
     * Acquires swapchain image once, submits all command buffers, then presents.
     * This is the correct method when multiple VKBuffers render to the same window.
     */
    std::function<void(const std::shared_ptr<void>&, std::vector<uint64_t>)>
        present_frame_batch;

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
     * @brief Get actual swapchain format for a window
     * @param window_handle Window handle
     * @return Vulkan format (vk::Format cast to uint32_t)
     *
     * Returns the actual format used by the window's swapchain.
     * Used to ensure render passes are compatible with framebuffers.
     */
    std::function<int(const std::shared_ptr<void>&)> get_swapchain_format;

    /**
     * @brief Get current framebuffer for a window
     * @param window_handle Window handle
     * @return Vulkan framebuffer handle (vk::Framebuffer cast to void*)
     *
     * Returns the framebuffer corresponding to the current swapchain image.
     * Updated internally during frame acquisition. Returns nullptr if window
     * not registered or no framebuffer available.
     */
    std::function<void*(const std::shared_ptr<void>&)> get_current_framebuffer;

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

    /**
     * @brief Get backend render pass for a window
     * @param window_handle Window handle
     * @return Vulkan render pass handle (vk::RenderPass cast to void*)
     *
     * Returns the render pass created by the backend for this window's swapchain.
     * Managed by backend, compatible with window's framebuffers.
     * Returns nullptr if window not registered.
     */
    std::function<void*(const std::shared_ptr<void>&)> get_window_render_pass;

    /**
     * @brief Attach a custom render pass to a window
     * @param window_handle Window handle
     * @param render_pass_handle Opaque render pass handle (Core::VKRenderPass cast to shared void*)
     * @return bool True on success, false on failure
     * Replaces the backend-managed render pass with a user-provided one.
     * Recreates framebuffers to be compatible with the new render pass.
     * Used for advanced rendering techniques requiring custom render passes.
     * Waits for idle before making changes.
     */
    std::function<bool(const std::shared_ptr<void>&, const std::shared_ptr<void>&)> attach_render_pass;
};

} // namespace MayaFlux::Registry::Services
