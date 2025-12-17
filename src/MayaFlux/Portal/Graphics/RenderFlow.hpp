#pragma once

#include "MayaFlux/Kakshya/NDData/VertexLayout.hpp"

#include "ShaderFoundry.hpp"

namespace MayaFlux::Registry::Service {
struct DisplayService;
}

namespace MayaFlux::Core {
class VKGraphicsPipeline;
class VKRenderPass;
class VKFramebuffer;
class Window;
struct VertexBinding;
struct VertexAttribute;
}

namespace MayaFlux::Buffers {
class RootGraphicsBuffer;
class VKBuffer;
}

namespace MayaFlux::Portal::Graphics {

using RenderPipelineID = uint64_t;
using RenderPassID = uint64_t;
using FramebufferID = uint64_t;

constexpr RenderPipelineID INVALID_RENDER_PIPELINE = 0;
constexpr RenderPassID INVALID_RENDER_PASS = 0;
constexpr FramebufferID INVALID_FRAMEBUFFER = 0;

/**
 * @struct RasterizationConfig
 * @brief Rasterization state configuration
 */
struct RasterizationConfig {
    PolygonMode polygon_mode = PolygonMode::FILL;
    CullMode cull_mode = CullMode::BACK;
    bool front_face_ccw = true;
    float line_width = 1.0F;
    bool depth_clamp = false;
    bool depth_bias = false;

    RasterizationConfig() = default;
};

/**
 * @struct DepthStencilConfig
 * @brief Depth and stencil test configuration
 */
struct DepthStencilConfig {
    bool depth_test_enable = true;
    bool depth_write_enable = true;
    CompareOp depth_compare_op = CompareOp::NEVER;
    bool stencil_test_enable = false;

    DepthStencilConfig() = default;
};

/**
 * @struct BlendAttachmentConfig
 * @brief Per-attachment blend configuration
 */
struct BlendAttachmentConfig {
    bool blend_enable = false;
    BlendFactor src_color_factor = BlendFactor::ONE;
    BlendFactor dst_color_factor = BlendFactor::ZERO;
    BlendOp color_blend_op = BlendOp::ADD;
    BlendFactor src_alpha_factor = BlendFactor::ONE;
    BlendFactor dst_alpha_factor = BlendFactor::ZERO;
    BlendOp alpha_blend_op = BlendOp::ADD;

    BlendAttachmentConfig() = default;

    /// @brief Create standard alpha blending configuration
    static BlendAttachmentConfig alpha_blend()
    {
        BlendAttachmentConfig config;
        config.blend_enable = true;
        config.src_color_factor = BlendFactor::SRC_ALPHA;
        config.dst_color_factor = BlendFactor::ONE_MINUS_SRC_ALPHA;
        config.src_alpha_factor = BlendFactor::ONE;
        config.dst_alpha_factor = BlendFactor::ZERO;
        return config;
    }
};

/**
 * @struct RenderPipelineConfig
 * @brief Complete render pipeline configuration
 */
struct RenderPipelineConfig {
    // Shader stages
    ShaderID vertex_shader = INVALID_SHADER;
    ShaderID fragment_shader = INVALID_SHADER;
    ShaderID geometry_shader = INVALID_SHADER; ///< Optional
    ShaderID tess_control_shader = INVALID_SHADER; ///< Optional
    ShaderID tess_eval_shader = INVALID_SHADER; ///< Optional

    // Vertex input
    std::vector<Core::VertexBinding> vertex_bindings;
    std::vector<Core::VertexAttribute> vertex_attributes;

    // Input assembly
    PrimitiveTopology topology = PrimitiveTopology::TRIANGLE_LIST;

    // Optional semantic vertex layout
    std::optional<Kakshya::VertexLayout> semantic_vertex_layout;

    // Use reflection to auto-configure from vertex shader
    bool use_vertex_shader_reflection = true;

    // Rasterization
    RasterizationConfig rasterization;

    // Depth/stencil
    DepthStencilConfig depth_stencil;

    // Blend
    std::vector<BlendAttachmentConfig> blend_attachments;

    // Descriptor sets (similar to compute)
    std::vector<std::vector<DescriptorBindingInfo>> descriptor_sets;

    // Push constants
    size_t push_constant_size = 0;

    // Render pass compatibility
    RenderPassID render_pass = INVALID_RENDER_PASS;
    uint32_t subpass = 0;

    RenderPipelineConfig() = default;
};

/**
 * @struct RenderPassAttachment
 * @brief Render pass attachment configuration
 */
struct RenderPassAttachment {
    vk::Format format = vk::Format::eB8G8R8A8Unorm;
    vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1;
    vk::AttachmentLoadOp load_op = vk::AttachmentLoadOp::eClear;
    vk::AttachmentStoreOp store_op = vk::AttachmentStoreOp::eStore;
    vk::ImageLayout initial_layout = vk::ImageLayout::eUndefined;
    vk::ImageLayout final_layout = vk::ImageLayout::ePresentSrcKHR;

    RenderPassAttachment() = default;
};

/**
 * @class RenderFlow
 * @brief Graphics pipeline and render pass orchestration
 *
 * RenderFlow is the rendering counterpart to ComputePress.
 * It manages graphics pipelines, render passes, and draw command recording.
 *
 * Responsibilities:
 * - Create graphics pipelines
 * - Create render passes
 * - Record render commands
 * - Manage rendering state
 * - Coordinate with ShaderFoundry for resources
 *
 * Design Philosophy (parallel to ComputePress):
 * - Uses ShaderFoundry for low-level resources
 * - Provides high-level rendering API
 * - Backend-agnostic interface
 * - Integrates with RootGraphicsBuffer
 *
 * Usage Pattern:
 * ```cpp
 * auto& flow = Portal::Graphics::get_render_flow();
 *
 * // Create pipeline
 * RenderPipelineConfig config;
 * config.vertex_shader = vertex_id;
 * config.fragment_shader = fragment_id;
 * config.vertex_bindings = {{0, sizeof(Vertex)}};
 * config.vertex_attributes = {{0, 0, vk::Format::eR32G32B32Sfloat, 0}};
 * auto pipeline_id = flow.create_pipeline(config);
 *
 * // In RenderProcessor callback:
 * auto cmd_id = foundry.begin_commands(CommandBufferType::GRAPHICS);
 * flow.begin_render_pass(cmd_id, render_pass_id, framebuffer_id);
 * flow.bind_pipeline(cmd_id, pipeline_id);
 * flow.bind_vertex_buffers(cmd_id, {vertex_buffer});
 * flow.draw(cmd_id, vertex_count);
 * flow.end_render_pass(cmd_id);
 * foundry.submit_and_present(cmd_id);
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
    void shutdown();

    [[nodiscard]] bool is_initialized() const { return m_shader_foundry != nullptr; }

    //==========================================================================
    // Render Pass Management
    //==========================================================================

    /**
     * @brief Create a render pass
     * @param attachments Attachment descriptions
     * @return Render pass ID
     */
    RenderPassID create_render_pass(
        const std::vector<RenderPassAttachment>& attachments);

    /**
     * @brief Create a simple single-color render pass
     * @param format Color attachment format
     * @param load_clear Whether to clear on load
     * @return Render pass ID
     */
    RenderPassID create_simple_render_pass(
        vk::Format format = vk::Format::eB8G8R8A8Unorm,
        bool load_clear = true);

    void destroy_render_pass(RenderPassID render_pass_id);

    //==========================================================================
    // Pipeline Creation
    //==========================================================================

    /**
     * @brief Create graphics pipeline with full configuration
     * @param config Complete pipeline configuration
     * @return Pipeline ID or INVALID_RENDER_PIPELINE on error
     */
    RenderPipelineID create_pipeline(const RenderPipelineConfig& config);

    /**
     * @brief Create simple graphics pipeline (auto-configure most settings)
     * @param vertex_shader Vertex shader ID
     * @param fragment_shader Fragment shader ID
     * @param render_pass Render pass ID
     * @return Pipeline ID or INVALID_RENDER_PIPELINE on error
     */
    RenderPipelineID create_simple_pipeline(
        ShaderID vertex_shader,
        ShaderID fragment_shader,
        RenderPassID render_pass);

    void destroy_pipeline(RenderPipelineID pipeline_id);

    //==========================================================================
    // Command Recording
    //==========================================================================

    /**
     * @brief Begin render pass
     * @param cmd_id Command buffer ID
     * @param window Target window for rendering
     * @param clear_color Clear color (if load op is clear)
     */
    void begin_render_pass(
        CommandBufferID cmd_id,
        const std::shared_ptr<Core::Window>& window,
        const std::array<float, 4>& clear_color = { 0.0F, 0.0F, 0.0F, 1.0F });

    /**
     * @brief End current render pass
     * @param cmd_id Command buffer ID
     */
    void end_render_pass(CommandBufferID cmd_id);

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
     * @brief Present rendered image to window
     * @param cmd_id Command buffer ID
     * @param window Target window for presentation
     */
    void present_rendered_image(CommandBufferID cmd_id, const std::shared_ptr<Core::Window>& window);

    //==========================================================================
    // Window Rendering Registration
    //==========================================================================

    /**
     * @brief Associate a window with a render pass for rendering
     * @param window Target window for rendering
     * @param render_pass_id Render pass to use for this window
     *
     * The window must be registered with GraphicsSubsystem first.
     * RenderFlow will query framebuffer/extent from DisplayService when needed.
     *
     * Usage:
     *   auto rp = flow.create_simple_render_pass();
     *   flow.register_window_for_rendering(my_window, rp);
     */
    void register_window_for_rendering(
        const std::shared_ptr<Core::Window>& window,
        RenderPassID render_pass_id);

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
        RenderPassID render_pass_id;
    };

    RenderFlow() = default;
    ~RenderFlow() { shutdown(); }

    struct PipelineState {
        std::vector<ShaderID> shader_ids;
        std::shared_ptr<Core::VKGraphicsPipeline> pipeline;
        std::vector<vk::DescriptorSetLayout> layouts;
        vk::PipelineLayout layout;
        RenderPassID render_pass;
    };

    struct RenderPassState {
        std::shared_ptr<Core::VKRenderPass> render_pass;
        std::vector<RenderPassAttachment> attachments;
    };

    std::unordered_map<RenderPipelineID, PipelineState> m_pipelines;
    std::unordered_map<RenderPassID, RenderPassState> m_render_passes;
    std::unordered_map<std::shared_ptr<Core::Window>, WindowRenderAssociation> m_window_associations;

    std::atomic<uint64_t> m_next_pipeline_id { 1 };
    std::atomic<uint64_t> m_next_render_pass_id { 1 };

    ShaderFoundry* m_shader_foundry = nullptr;
    Registry::Service::DisplayService* m_display_service = nullptr;

    static bool s_initialized;
};

/**
 * @brief Get the global render flow instance
 */
inline MAYAFLUX_API RenderFlow& get_render_flow()
{
    return RenderFlow::instance();
}

} // namespace MayaFlux::Portal::Graphics
