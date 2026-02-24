#pragma once

#include "vulkan/vulkan.hpp"

namespace MayaFlux::Registry::Service {
struct DisplayService;
}

namespace MayaFlux::Core {

class VKContext;
class VKSwapchain;
class VKCommandManager;
class Window;
class BackendResourceManager;

struct WindowRenderContext {
    std::shared_ptr<Window> window;
    vk::SurfaceKHR surface;
    std::unique_ptr<VKSwapchain> swapchain;

    std::vector<vk::Semaphore> image_available;
    std::vector<vk::Semaphore> render_finished;
    std::vector<vk::Fence> in_flight;

    std::vector<vk::CommandBuffer> clear_command_buffers;

    bool needs_recreation {};
    size_t current_frame {};
    uint32_t current_image_index {};

    WindowRenderContext() = default;
    ~WindowRenderContext() = default;

    WindowRenderContext(WindowRenderContext&&) = default;
    WindowRenderContext& operator=(WindowRenderContext&&) = default;
    WindowRenderContext(const WindowRenderContext&) = delete;
    WindowRenderContext& operator=(const WindowRenderContext&) = delete;

    void cleanup(VKContext& context);
};

class MAYAFLUX_API BackendWindowHandler {
public:
    BackendWindowHandler(VKContext& context, VKCommandManager& command_manager);
    ~BackendWindowHandler() = default;

    void setup_backend_service(const std::shared_ptr<Registry::Service::DisplayService>& display_service);

    BackendWindowHandler(const BackendWindowHandler&) = delete;
    BackendWindowHandler& operator=(const BackendWindowHandler&) = delete;

    BackendWindowHandler(BackendWindowHandler&&) noexcept = default;
    BackendWindowHandler& operator=(BackendWindowHandler&&) noexcept = default;

    // ========================================================================
    // Window management
    // ========================================================================
    bool register_window(const std::shared_ptr<Window>& window);
    void unregister_window(const std::shared_ptr<Window>& window);
    [[nodiscard]] bool is_window_registered(const std::shared_ptr<Window>& window) const;

    // ========================================================================
    // Rendering
    // ========================================================================
    void render_window(const std::shared_ptr<Window>& window);
    void render_all_windows();
    void handle_window_resize();

    void submit_and_present(
        const std::shared_ptr<Window>& window,
        const vk::CommandBuffer& command_buffer);

    // ========================================================================
    // Access control
    // ========================================================================
    WindowRenderContext* find_window_context(const std::shared_ptr<Window>& window);
    [[nodiscard]] const WindowRenderContext* find_window_context(const std::shared_ptr<Window>& window) const;

    // Information
    [[nodiscard]] uint32_t get_swapchain_image_count(const std::shared_ptr<Window>& window) const;

    // Cleanup
    void cleanup();

    void set_resource_manager(BackendResourceManager* resource_manager) { m_resource_manager = resource_manager; }

private:
    VKContext& m_context;
    VKCommandManager& m_command_manager;
    std::vector<WindowRenderContext> m_window_contexts;

    /**
     * @brief Create synchronization objects for a window's swapchain
     * @param config Window swapchain configuration to populate
     * @return True if creation succeeded
     */
    bool create_sync_objects(WindowRenderContext& config);

    /**
     * @brief Recreate the swapchain and related resources for a window
     * @param context Window render context
     */
    void recreate_swapchain_for_context(WindowRenderContext& context);

    /**
     * @brief Internal logic to recreate swapchain and related resources
     * @param context Window render context
     * @return True if recreation succeeded
     */
    bool recreate_swapchain_internal(WindowRenderContext& context);

    /**
     * @brief Render empty windows with clear color
     * @param ctx Window render context
     *
     * For windows that are registered for processing but have no buffers attached,
     * this performs a minimal clear pass so the window is visible and responsive
     * to input events.
     */
    void render_empty_window(WindowRenderContext& ctx);

    BackendResourceManager* m_resource_manager {};
};

}
