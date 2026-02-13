#pragma once

#include "ShaderFoundry.hpp"

namespace MayaFlux::Registry::Service {
struct DisplayService;
}

namespace MayaFlux::Core {
class VKGraphicsPipeline;
class Window;
}

namespace MayaFlux::Buffers {
class RootGraphicsBuffer;
class VKBuffer;
}

namespace MayaFlux::Portal::Graphics {

const std::array<float, 4> default_color { 0.0F, 0.0F, 0.0F, 1.0F };

/**
 * @class RenderFlow
 * @brief Graphics pipeline orchestration for dynamic rendering
 *
 * RenderFlow is the rendering counterpart to ComputePress.
 * It manages graphics pipelines and draw command recording using Vulkan 1.3 dynamic rendering.
 *
 * Responsibilities:
 * - Create graphics pipelines for dynamic rendering
 * - Record render commands to secondary command buffers
 * - Manage dynamic rendering state (begin/end rendering)
 * - Coordinate with ShaderFoundry for resources
 *
 * Design Philosophy (parallel to ComputePress):
 * - Uses ShaderFoundry for low-level resources
 * - Provides high-level rendering API
 * - Backend-agnostic interface
 * - Integrates with RootGraphicsBuffer
 * - No render pass objects - uses vkCmdBeginRendering/vkCmdEndRendering
 *
 * Usage Pattern:
 * ```cpp
 * auto& flow = Portal::Graphics::get_render_flow();
 * auto& foundry = Portal::Graphics::get_shader_foundry();
 *
 * // Create pipeline for dynamic rendering
 * RenderPipelineConfig config;
 * config.vertex_shader = vertex_id;
 * config.fragment_shader = fragment_id;
 * config.semantic_vertex_layout = buffer->get_vertex_layout();
 * auto pipeline_id = flow.create_pipeline(config, { swapchain_format });
 *
 * // In RenderProcessor - record secondary command buffer:
 * auto cmd_id = foundry.begin_secondary_commands(swapchain_format);
 * flow.bind_pipeline(cmd_id, pipeline_id);
 * flow.bind_vertex_buffers(cmd_id, {buffer});
 * flow.draw(cmd_id, vertex_count);
 * foundry.end_commands(cmd_id);
 *
 * // In PresentProcessor - execute secondaries in primary:
 * auto primary_id = foundry.begin_commands(CommandBufferType::GRAPHICS);
 * flow.begin_rendering(primary_id, window, swapchain_image);
 * primary_cmd.executeCommands(secondary_buffers);
 * flow.end_rendering(primary_id, window);
 * display_service->submit_and_present(window, primary_cmd);
 * ```
 */
class MAYAFLUX_API RenderFlow {
public:
    static RenderFlow& instance()
    {
        static RenderFlow flow;
        return flow;
    }

    RenderFlow(const RenderFlow&) = delete;
    RenderFlow& operator=(const RenderFlow&) = delete;
    RenderFlow(RenderFlow&&) noexcept = delete;
    RenderFlow& operator=(RenderFlow&&) noexcept = delete;

    bool initialize();
    void stop();
    void shutdown();

    [[nodiscard]] bool is_initialized() const { return m_shader_foundry != nullptr; }

    //==========================================================================
    // Pipeline Creation
    //==========================================================================

    /**
     * @brief Create graphics pipeline for dynamic rendering (no render pass object)
     * @param config Pipeline configuration
     * @param color_formats Color attachment formats for dynamic rendering
     * @param depth_format Depth attachment format
     * @return Pipeline ID or INVALID_RENDER_PIPELINE on error
     */
    RenderPipelineID create_pipeline(
        const RenderPipelineConfig& config,
        const std::vector<vk::Format>& color_formats,
        vk::Format depth_format = vk::Format::eUndefined);

    /**
     * @brief Destroy a graphics pipeline
     * @param pipeline_id Pipeline ID to destroy
     */
    void destroy_pipeline(RenderPipelineID pipeline_id);

    //==========================================================================
    // Dynamic Rendering
    //==========================================================================

    /**
     * @brief Begin dynamic rendering to a window
     * @param cmd_id Command buffer ID
     * @param window Target window
     * @param swapchain_image Swapchain image to render to
     * @param clear_color Clear color (RGBA)
     *
     * Uses vkCmdBeginRendering - no render pass objects needed.
     */
    void begin_rendering(
        CommandBufferID cmd_id,
        const std::shared_ptr<Core::Window>& window,
        vk::Image swapchain_image,
        const std::array<float, 4>& clear_color = default_color);

    /**
     * @brief End dynamic rendering
     * @param cmd_id Command buffer ID
     * @param window Target window
     */
    void end_rendering(CommandBufferID cmd_id, const std::shared_ptr<Core::Window>& window);

    //==========================================================================
    // Command Recording
    //==========================================================================

    /**
     * @brief Bind graphics pipeline
     * @param cmd_id Command buffer ID
     * @param pipeline Pipeline ID
     */
    void bind_pipeline(CommandBufferID cmd_id, RenderPipelineID pipeline);

    /**
     * @brief Bind vertex buffers
     * @param cmd_id Command buffer ID
     * @param buffers Vertex buffers to bind
     * @param first_binding First binding index
     */
    void bind_vertex_buffers(
        CommandBufferID cmd_id,
        const std::vector<std::shared_ptr<Buffers::VKBuffer>>& buffers,
        uint32_t first_binding = 0);

    /**
     * @brief Bind index buffer
     * @param cmd_id Command buffer ID
     * @param buffer Index buffer
     * @param index_type Index type (16-bit or 32-bit)
     */
    void bind_index_buffer(
        CommandBufferID cmd_id,
        const std::shared_ptr<Buffers::VKBuffer>& buffer,
        vk::IndexType index_type = vk::IndexType::eUint32);

    /**
     * @brief Bind descriptor sets
     * @param cmd_id Command buffer ID
     * @param pipeline Pipeline ID
     * @param descriptor_sets Descriptor set IDs
     */
    void bind_descriptor_sets(
        CommandBufferID cmd_id,
        RenderPipelineID pipeline,
        const std::vector<DescriptorSetID>& descriptor_sets);

    /**
     * @brief Push constants
     * @param cmd_id Command buffer ID
     * @param pipeline Pipeline ID
     * @param data Constant data
     * @param size Data size in bytes
     */
    void push_constants(
        CommandBufferID cmd_id,
        RenderPipelineID pipeline,
        const void* data,
        size_t size);

    /**
     * @brief Draw command
     * @param cmd_id Command buffer ID
     * @param vertex_count Number of vertices
     * @param instance_count Number of instances (default: 1)
     * @param first_vertex First vertex index
     * @param first_instance First instance index
     */
    void draw(
        CommandBufferID cmd_id,
        uint32_t vertex_count,
        uint32_t instance_count = 1,
        uint32_t first_vertex = 0,
        uint32_t first_instance = 0);

    /**
     * @brief Indexed draw command
     * @param cmd_id Command buffer ID
     * @param index_count Number of indices
     * @param instance_count Number of instances (default: 1)
     * @param first_index First index
     * @param vertex_offset Vertex offset
     * @param first_instance First instance index
     */
    void draw_indexed(
        CommandBufferID cmd_id,
        uint32_t index_count,
        uint32_t instance_count = 1,
        uint32_t first_index = 0,
        int32_t vertex_offset = 0,
        uint32_t first_instance = 0);

    /**
     * @brief Draw mesh tasks (mesh shading pipeline only)
     */
    void draw_mesh_tasks(
        CommandBufferID cmd_id,
        uint32_t group_count_x,
        uint32_t group_count_y = 1,
        uint32_t group_count_z = 1);

    /**
     * @brief Draw mesh tasks indirect
     */
    void draw_mesh_tasks_indirect(
        CommandBufferID cmd_id,
        const std::shared_ptr<Buffers::VKBuffer>& buffer,
        vk::DeviceSize offset = 0,
        uint32_t draw_count = 1,
        uint32_t stride = sizeof(VkDrawMeshTasksIndirectCommandEXT));

    //==========================================================================
    // Window Rendering Registration
    //==========================================================================

    /**
     * @brief Register a window for dynamic rendering
     * @param window Target window for rendering
     *
     * The window must be registered with GraphicsSubsystem first
     * (i.e., window->is_graphics_registered() must be true).
     *
     * This associates the window with RenderFlow for tracking.
     * No render pass attachment - dynamic rendering handles this per-frame.
     */
    void register_window_for_rendering(const std::shared_ptr<Core::Window>& window);

    /**
     * @brief Unregister a window from rendering
     * @param window Window to unregister
     */
    void unregister_window(const std::shared_ptr<Core::Window>& window);

    /**
     * @brief Check if a window is registered for rendering
     */
    [[nodiscard]] bool is_window_registered(const std::shared_ptr<Core::Window>& window) const;

    /**
     * @brief Get all registered windows
     */
    [[nodiscard]] std::vector<std::shared_ptr<Core::Window>> get_registered_windows() const;

    //==========================================================================
    // Convenience Methods
    //==========================================================================

    /**
     * @brief Allocate descriptor sets for pipeline
     * @param pipeline Pipeline ID
     * @return Vector of allocated descriptor set IDs
     */
    std::vector<DescriptorSetID> allocate_pipeline_descriptors(RenderPipelineID pipeline);

private:
    struct WindowRenderAssociation {
        std::weak_ptr<Core::Window> window;
        vk::Image swapchain_image {};
    };

    struct PipelineState {
        std::vector<ShaderID> shader_ids;
        std::shared_ptr<Core::VKGraphicsPipeline> pipeline;
        std::vector<vk::DescriptorSetLayout> layouts;
        vk::PipelineLayout layout;
        vk::ShaderStageFlags push_constant_stages = vk::ShaderStageFlagBits::eVertex
            | vk::ShaderStageFlagBits::eFragment;
    };

    std::unordered_map<RenderPipelineID, PipelineState> m_pipelines;
    std::unordered_map<std::shared_ptr<Core::Window>, WindowRenderAssociation> m_window_associations;

    std::atomic<uint64_t> m_next_pipeline_id { 1 };

    ShaderFoundry* m_shader_foundry = nullptr;
    Registry::Service::DisplayService* m_display_service = nullptr;

    static bool s_initialized;

    RenderFlow() = default;
    ~RenderFlow() { shutdown(); }
    void cleanup_pipelines();

    /**
     * @brief Get current image view for window
     * @param window Target window
     * @return vk::ImageView of current swapchain image
     */
    vk::ImageView get_current_image_view(const std::shared_ptr<Core::Window>& window);
};

/**
 * @brief Get the global render flow instance
 */
inline MAYAFLUX_API RenderFlow& get_render_flow()
{
    return RenderFlow::instance();
}

} // namespace MayaFlux::Portal::Graphics
