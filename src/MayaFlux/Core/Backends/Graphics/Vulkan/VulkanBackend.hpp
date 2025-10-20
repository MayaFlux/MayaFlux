#pragma once

#include "MayaFlux/Core/Backends/Graphics/GraphicsBackend.hpp"

#include <vulkan/vulkan.hpp>

namespace MayaFlux::Core {

class VKContext;
class VKSwapchain;
class VKCommandManager;
class VKRenderPass;
class VKFramebuffer;

struct WindowRenderContext {
    std::shared_ptr<Window> window;
    vk::SurfaceKHR surface;
    std::unique_ptr<VKSwapchain> swapchain;
    std::unique_ptr<VKRenderPass> render_pass;
    std::vector<std::unique_ptr<VKFramebuffer>> framebuffers;

    std::vector<vk::Semaphore> image_available;
    std::vector<vk::Semaphore> render_finished;
    std::vector<vk::Fence> in_flight;

    bool needs_recreation = false;
    size_t current_frame = 0;

    WindowRenderContext() = default;
    ~WindowRenderContext() = default;

    WindowRenderContext(WindowRenderContext&&) = default;
    WindowRenderContext& operator=(WindowRenderContext&&) = default;
    WindowRenderContext(const WindowRenderContext&) = delete;
    WindowRenderContext& operator=(const WindowRenderContext&) = delete;

    void cleanup(VKContext& context);
};

/**
 * @class VulkanBackend
 * @brief Vulkan implementation of the IGraphicsBackend interface
 *
 * This class provides a Vulkan-based graphics backend for rendering to windows.
 * It manages Vulkan context, swapchains, command buffers, and synchronization
 * objects for each registered window.
 * It supports window registration, rendering, and handling window resize events.
 */
class MAYAFLUX_API VulkanBackend : public IGraphicsBackend {
public:
    VulkanBackend();

    ~VulkanBackend() override;

    /**
     * @brief Initialize the graphics backend with global configuration
     * @param config Global graphics configuration
     * @return True if initialization was successful, false otherwise
     */
    bool initialize(const GlobalGraphicsConfig& config) override;

    /**
     * @brief Cleanup the graphics backend and release all resources
     */
    void cleanup() override;

    /**
     * @brief Get the type of the graphics backend
     * @return GraphicsBackendType enum value representing the backend type
     */
    inline GlobalGraphicsConfig::GraphicsApi get_backend_type() override { return GlobalGraphicsConfig::GraphicsApi::VULKAN; }

    /**
     * @brief Register a window with the graphics backend for rendering
     * @param window Shared pointer to the window to register
     * @return True if registration was successful, false otherwise
     */
    bool register_window(std::shared_ptr<Window> window) override;

    /**
     * @brief Unregister a window from the graphics backend
     * @param window Shared pointer to the window to unregister
     */
    void unregister_window(std::shared_ptr<Window> window) override;

    [[nodiscard]] bool is_window_registered(std::shared_ptr<Window> window) override;

    /**
     * @brief Begin rendering frame for the specified window
     * @param window Shared pointer to the window to begin frame for
     *
     * Default: no-op (handled in render_window)
     * Vulkan handles frame begin internally when acquiring swapchain image
     */
    void begin_frame(std::shared_ptr<Window> /* window */) override { }

    /**
     * @brief Render the contents of the specified window
     * @param window Shared pointer to the window to render
     */
    void render_window(std::shared_ptr<Window> window) override;

    /**
     * @brief Render all registered windows (batch optimization)
     * Default: calls render_window() for each registered window
     */
    void render_all_windows() override;

    /**
     * @brief End rendering frame for the specified window
     * @param window Shared pointer to the window to end frame for
     *
     * Default: no-op (handled in render_window)
     * Vulkan handles frame end internally after rendering
     */
    void end_frame(std::shared_ptr<Window> /* window */) override { }

    /**
     * @brief Wait until the graphics backend is idle
     */
    void wait_idle() override;

    /**
     * @brief Handle window resize event for the specified window
     */
    void handle_window_resize() override;

    /**
     * @brief Get context pointer specific to the graphics backend (e.g., OpenGL context, Vulkan instance, etc.)
     */
    [[nodiscard]] void* get_native_context() override;
    [[nodiscard]] const void* get_native_context() const override;

private:
    std::unique_ptr<VKContext> m_vulkan_context;
    std::unique_ptr<VKCommandManager> m_command_manager;

    std::vector<WindowRenderContext> m_window_contexts;

    bool m_is_initialized {};

    WindowRenderContext* find_window_context(const std::shared_ptr<Window>& window)
    {
        auto it = std::ranges::find_if(m_window_contexts,
            [window](const auto& config) { return config.window == window; });
        return it != m_window_contexts.end() ? &(*it) : nullptr;
    }

    /**
     * @brief Create synchronization objects for a window's swapchain
     * @param config Window swapchain configuration to populate
     * @return True if creation succeeded
     */
    bool create_sync_objects(WindowRenderContext& config);

    /**
     * @brief Internal rendering logic for a window
     * @param context Window render context
     */
    void render_window_internal(WindowRenderContext& context);

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
};

}
