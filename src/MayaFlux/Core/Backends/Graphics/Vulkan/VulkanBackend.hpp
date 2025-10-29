#pragma once

#include "MayaFlux/Core/Backends/Graphics/GraphicsBackend.hpp"
#include "vulkan/vulkan.hpp"

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
class VKSwapchain;
class VKCommandManager;
class VKRenderPass;
class VKFramebuffer;
class VKShaderModule;
class VKDescriptorManager;
class VKComputePipeline;
class VKGraphicsPipeline;
class VKImage;
struct GraphicsPipelineConfig;

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

    /**
     * @brief Check if a window is registered with the graphics backend
     * @param window Shared pointer to the window to check
     * @return True if the window is registered, false otherwise
     */
    [[nodiscard]] bool is_window_registered(std::shared_ptr<Window> window) override;

    /**
     * @brief Initialize a buffer for use with the graphics backend
     * @param buffer Shared pointer to the buffer to initialize
     */
    void initialize_buffer(const std::shared_ptr<class Buffers::VKBuffer>& buffer);

    /**
     * @brief Cleanup a buffer and release associated resources
     * @param buffer Shared pointer to the buffer to cleanup
     */
    void cleanup_buffer(const std::shared_ptr<class Buffers::VKBuffer>& buffer);

    /**
     * @brief Flush any pending buffer operations (e.g., uploads/downloads)
     */
    void flush_pending_buffer_operations();

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

    /** @brief Create a shader module from SPIR-V binary
     * @param spirv_path Path to the SPIR-V binary file
     * @param stage Shader stage (e.g., vertex, fragment, compute)
     * @return Shared pointer to the created VKShaderModule
     */
    std::shared_ptr<VKShaderModule> create_shader_module(const std::string& spirv_path, uint32_t stage);

    /** @brief Create a descriptor manager for managing descriptor sets
     * @param pool_size Number of descriptor sets to allocate in the pool
     * @return Shared pointer to the created VKDescriptorManager
     */
    std::shared_ptr<VKDescriptorManager> create_descriptor_manager(uint32_t pool_size);

    /** @brief Create a descriptor set layout
     * @param manager Descriptor manager to use for allocation
     * @param bindings Vector of pairs specifying binding index and descriptor type
     * @return Created vk::DescriptorSetLayout
     */
    vk::DescriptorSetLayout create_descriptor_layout(
        const std::shared_ptr<VKDescriptorManager>& manager,
        const std::vector<std::pair<uint32_t, uint32_t>>& bindings);

    /** @brief Create a compute pipeline
     * @param shader Shader module to use for the compute pipeline
     * @param layouts Vector of descriptor set layouts used by the pipeline
     * @param push_constant_size Size of push constant range in bytes
     * @return Shared pointer to the created VKComputePipeline
     */
    std::shared_ptr<VKComputePipeline> create_compute_pipeline(
        const std::shared_ptr<VKShaderModule>& shader,
        const std::vector<vk::DescriptorSetLayout>& layouts,
        uint32_t push_constant_size);

    /**
     * @brief Create graphics pipeline
     * @param config Graphics pipeline configuration
     * @return Shared pointer to created pipeline, or nullptr on failure
     */
    std::shared_ptr<VKGraphicsPipeline> create_graphics_pipeline(
        const GraphicsPipelineConfig& config);

    /**
     * @brief Create sampler
     * @param filter Mag/min filter
     * @param address_mode Texture address mode (wrap, clamp, etc.)
     * @param anisotropy Max anisotropy (0 = disabled)
     * @return Sampler handle
     */
    vk::Sampler create_sampler(
        vk::Filter filter = vk::Filter::eLinear,
        vk::SamplerAddressMode address_mode = vk::SamplerAddressMode::eRepeat,
        float max_anisotropy = 0.0f);

    /**
     * @brief Destroy sampler
     */
    void destroy_sampler(vk::Sampler sampler);

    /** @brief Cleanup a compute resource allocated by the backend
     * @param resource Pointer to the resource to cleanup
     */
    void cleanup_compute_resource(void* resource);

    /**
     * @brief Initialize a VKImage (allocate VkImage, memory, and create image view)
     * @param image VKImage to initialize
     *
     * Follows the same pattern as initialize_buffer:
     * 1. Create VkImage
     * 2. Allocate VkDeviceMemory
     * 3. Bind memory to image
     * 4. Create VkImageView
     * 5. Store handles in VKImage
     */
    void initialize_image(const std::shared_ptr<VKImage>& image);

    /**
     * @brief Cleanup a VKImage (destroy view, image, and free memory)
     * @param image VKImage to cleanup
     */
    void cleanup_image(const std::shared_ptr<VKImage>& image);

    /**
     * @brief Transition image layout using a pipeline barrier
     * @param image VkImage handle
     * @param old_layout Current layout
     * @param new_layout Target layout
     * @param mip_levels Number of mip levels to transition
     * @param array_layers Number of array layers to transition
     *
     * Executes immediately on graphics queue. Use for initial setup and
     * one-off transitions. For rendering, prefer manual barriers.
     */
    void transition_image_layout(
        vk::Image image,
        vk::ImageLayout old_layout,
        vk::ImageLayout new_layout,
        uint32_t mip_levels = 1,
        uint32_t array_layers = 1,
        vk::ImageAspectFlags aspect_flags = vk::ImageAspectFlagBits::eColor);

private:
    std::unique_ptr<VKContext> m_context;
    std::unique_ptr<VKCommandManager> m_command_manager;
    std::vector<WindowRenderContext> m_window_contexts;
    std::vector<std::shared_ptr<Buffers::VKBuffer>> m_managed_buffers;
    std::unordered_map<size_t, vk::Sampler> m_sampler_cache;

    bool m_is_initialized {};

    WindowRenderContext* find_window_context(const std::shared_ptr<Window>& window)
    {
        auto it = std::ranges::find_if(m_window_contexts,
            [window](const auto& config) { return config.window == window; });
        return it != m_window_contexts.end() ? &(*it) : nullptr;
    }

    /**
     * @brief Find a suitable memory type for Vulkan buffer allocation
     * @param type_filter Memory type bits filter
     * @param properties Desired memory property flags
     * @return Index of the suitable memory type
     */
    [[nodiscard]] uint32_t find_memory_type(uint32_t type_filter, vk::MemoryPropertyFlags properties) const;

    /**
     * @brief Execute immediate command recording for buffer operations
     * @param recorder Command recording function
     */
    void execute_immediate_commands(const std::function<void(vk::CommandBuffer)>& recorder);

    /**
     * @brief Record deferred command recording for buffer operations
     * @param recorder Command recording function
     */
    void record_deferred_commands(const std::function<void(vk::CommandBuffer)>& recorder);

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

    void register_backend_services();

    void unregister_backend_services();

    std::shared_ptr<Registry::Service::BufferService> m_buffer_service;
    std::shared_ptr<Registry::Service::ComputeService> m_compute_service;
    std::shared_ptr<Registry::Service::DisplayService> m_display_service;
};

}
