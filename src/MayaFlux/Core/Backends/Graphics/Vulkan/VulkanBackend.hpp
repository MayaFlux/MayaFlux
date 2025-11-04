#pragma once

#include "MayaFlux/Core/Backends/Graphics/GraphicsBackend.hpp"

namespace MayaFlux::Registry::Service {
struct BufferService;
struct ComputeService;
struct DisplayService;
}

namespace MayaFlux::Buffers {
class VKBuffer;
}

namespace MayaFlux::Core {

class VKContext;
class VKCommandManager;

class BackendResourceManager;
class BackendPipelineManager;
class BackendWindowHandler;

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

    /**
     * @brief Check if a window is registered with the graphics backend
     * @param window Shared pointer to the window to check
     * @return True if the window is registered, false otherwise
     */
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

    /**
     * @brief Get reference to the backend resource manager
     *
     * Responsible for managing Vulkan resources like buffers, images, samplers, command and memory management.
     * @return Reference to BackendResourceManager
     */
    BackendResourceManager& get_resource_manager() { return *m_resource_manager; }

    /**
     * @brief Get reference to the backend pipeline manager
     *
     * Responsible for managing Vulkan pipelines, descriptor sets, and shader modules.
     * @return Reference to BackendPipelineManager
     */
    BackendPipelineManager& get_pipeline_manager() { return *m_pipeline_manager; }

    /**
     * @brief Get reference to the backend window handler
     *
     * Responsible for managing windows, swapchains, framebuffers, and rendering loops.
     * @return Reference to BackendWindowHandler
     */
    VKContext& get_context() { return *m_context; }

    /**
     * @brief Get reference to the backend command manager
     *
     * Responsible for managing Vulkan command pools and command buffers.
     * @return Reference to VKCommandManager
     */
    VKCommandManager& get_command_manager() { return *m_command_manager; }

private:
    std::unique_ptr<VKContext> m_context;
    std::unique_ptr<VKCommandManager> m_command_manager;

    std::unique_ptr<BackendResourceManager> m_resource_manager;
    std::unique_ptr<BackendPipelineManager> m_pipeline_manager;
    std::unique_ptr<BackendWindowHandler> m_window_handler;

    bool m_is_initialized {};

    void register_backend_services();

    void unregister_backend_services();

    std::shared_ptr<Registry::Service::BufferService> m_buffer_service;
    std::shared_ptr<Registry::Service::ComputeService> m_compute_service;
    std::shared_ptr<Registry::Service::DisplayService> m_display_service;
};

}
