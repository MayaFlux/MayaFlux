#pragma once

#include "VKShaderModule.hpp"

namespace MayaFlux::Core {

struct VertexBinding {
    uint32_t binding;
    uint32_t stride;
    bool per_instance;
    vk::VertexInputRate input_rate;

    VertexBinding() = default;
    VertexBinding(uint32_t b, uint32_t s, bool instanced = false, vk::VertexInputRate rate = vk::VertexInputRate::eVertex)
        : binding(b)
        , stride(s)
        , per_instance(instanced)
        , input_rate(rate)
    {
    }
};

struct VertexAttribute {
    uint32_t location;
    uint32_t binding;
    vk::Format format;
    uint32_t offset;

    VertexAttribute() = default;
    VertexAttribute(uint32_t loc, uint32_t bind, vk::Format fmt, uint32_t off)
        : location(loc)
        , binding(bind)
        , format(fmt)
        , offset(off)
    {
    }
};

struct ColorBlendAttachment {
    bool blend_enable = false;
    vk::BlendFactor src_color_blend_factor = vk::BlendFactor::eSrcAlpha;
    vk::BlendFactor dst_color_blend_factor = vk::BlendFactor::eOneMinusSrcAlpha;
    vk::BlendOp color_blend_op = vk::BlendOp::eAdd;
    vk::BlendFactor src_alpha_blend_factor = vk::BlendFactor::eOne;
    vk::BlendFactor dst_alpha_blend_factor = vk::BlendFactor::eZero;
    vk::BlendOp alpha_blend_op = vk::BlendOp::eAdd;
    vk::ColorComponentFlags color_write_mask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
};

/**
 * @struct GraphicsPipelineConfig
 * @brief Configuration for creating a graphics pipeline
 *
 * Comprehensive graphics pipeline state. Vulkan requires ALL fixed-function
 * state to be specified at pipeline creation time.
 */
struct GraphicsPipelineConfig {
    //==========================================================================
    // SHADER STAGES
    //==========================================================================

    std::shared_ptr<VKShaderModule> vertex_shader = nullptr; // Required
    std::shared_ptr<VKShaderModule> fragment_shader = nullptr; // Optional (depth-only passes)
    std::shared_ptr<VKShaderModule> geometry_shader = nullptr; // Optional
    std::shared_ptr<VKShaderModule> tess_control_shader = nullptr; // Optional
    std::shared_ptr<VKShaderModule> tess_evaluation_shader = nullptr; // Optional

    //==========================================================================
    // VERTEX INPUT STATE
    //==========================================================================

    std::vector<VertexBinding> vertex_bindings;
    std::vector<VertexAttribute> vertex_attributes;

    bool use_vertex_shader_reflection = true;

    //==========================================================================
    // INPUT ASSEMBLY STATE
    //==========================================================================

    vk::PrimitiveTopology topology = vk::PrimitiveTopology::eTriangleList;
    bool primitive_restart_enable = false;

    //==========================================================================
    // TESSELLATION STATE
    //==========================================================================

    uint32_t patch_control_points = 3;

    //==========================================================================
    // VIEWPORT STATE
    //==========================================================================

    bool dynamic_viewport = true;
    bool dynamic_scissor = true;

    vk::Viewport static_viewport {};
    vk::Rect2D static_scissor {};

    //==========================================================================
    // RASTERIZATION STATE
    //==========================================================================

    bool depth_clamp_enable = false;
    bool rasterizer_discard_enable = false;
    vk::PolygonMode polygon_mode = vk::PolygonMode::eFill;
    vk::CullModeFlags cull_mode = vk::CullModeFlagBits::eBack;
    vk::FrontFace front_face = vk::FrontFace::eCounterClockwise;

    bool depth_bias_enable = false;
    float depth_bias_constant_factor = 0.0f;
    float depth_bias_clamp = 0.0f;
    float depth_bias_slope_factor = 0.0f;

    float line_width = 1.0f;

    //==========================================================================
    // MULTISAMPLE STATE
    //==========================================================================

    vk::SampleCountFlagBits rasterization_samples = vk::SampleCountFlagBits::e1;
    bool sample_shading_enable = false;
    float min_sample_shading = 1.0f;
    std::vector<vk::SampleMask> sample_mask;
    bool alpha_to_coverage_enable = false;
    bool alpha_to_one_enable = false;

    //==========================================================================
    // DEPTH STENCIL STATE
    //==========================================================================

    bool depth_test_enable = true;
    bool depth_write_enable = true;
    vk::CompareOp depth_compare_op = vk::CompareOp::eLess;
    bool depth_bounds_test_enable = false;
    float min_depth_bounds = 0.0f;
    float max_depth_bounds = 1.0f;

    bool stencil_test_enable = false;
    vk::StencilOpState front_stencil {};
    vk::StencilOpState back_stencil {};

    //==========================================================================
    // COLOR BLEND STATE
    //==========================================================================

    bool logic_op_enable = false;
    vk::LogicOp logic_op = vk::LogicOp::eCopy;

    std::vector<ColorBlendAttachment> color_blend_attachments;
    std::array<float, 4> blend_constants = { 0.0f, 0.0f, 0.0f, 0.0f };

    //==========================================================================
    // DYNAMIC STATE
    //==========================================================================

    std::vector<vk::DynamicState> dynamic_states = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor
    };

    //==========================================================================
    // PIPELINE LAYOUT (descriptors + push constants)
    //==========================================================================

    std::vector<vk::DescriptorSetLayout> descriptor_set_layouts;
    std::vector<vk::PushConstantRange> push_constant_ranges;

    //==========================================================================
    // RENDER PASS / DYNAMIC RENDERING
    //==========================================================================

    vk::RenderPass render_pass = nullptr;
    uint32_t subpass = 0;

    std::vector<vk::Format> color_attachment_formats;
    vk::Format depth_attachment_format = vk::Format::eUndefined;
    vk::Format stencil_attachment_format = vk::Format::eUndefined;

    //==========================================================================
    // PIPELINE CACHE
    //==========================================================================

    vk::PipelineCache cache = nullptr;

    //==========================================================================
    // HELPER METHODS
    //==========================================================================

    static GraphicsPipelineConfig default_3d();
    static GraphicsPipelineConfig default_2d();
    static GraphicsPipelineConfig alpha_blended();
    static GraphicsPipelineConfig additive_blended();
    static GraphicsPipelineConfig depth_only();

    void enable_alpha_blending();
    void enable_additive_blending();
    void disable_depth_test();
    void enable_wireframe();
    void enable_back_face_culling();
    void disable_culling();
};

/**
 * @class VKGraphicsPipeline
 * @brief Vulkan graphics pipeline wrapper
 *
 * Handles the complex graphics pipeline state machine. Unlike compute pipelines,
 * graphics pipelines require extensive fixed-function configuration.
 *
 * Responsibilities:
 * - Create graphics pipeline from config
 * - Manage pipeline layout
 * - Bind pipeline to command buffer
 * - Bind vertex/index buffers
 * - Bind descriptor sets
 * - Push constants
 * - Dynamic state updates (viewport, scissor, etc.)
 * - Draw commands
 *
 * Thread Safety:
 * - NOT thread-safe
 * - Create on main thread, use on render thread
 */
class VKGraphicsPipeline {
public:
    VKGraphicsPipeline() = default;
    ~VKGraphicsPipeline();

    VKGraphicsPipeline(const VKGraphicsPipeline&) = delete;
    VKGraphicsPipeline& operator=(const VKGraphicsPipeline&) = delete;
    VKGraphicsPipeline(VKGraphicsPipeline&&) noexcept;
    VKGraphicsPipeline& operator=(VKGraphicsPipeline&&) noexcept;

    //==========================================================================
    // PIPELINE CREATION
    //==========================================================================

    /**
     * @brief Create graphics pipeline from configuration
     * @param device Logical device
     * @param config Pipeline configuration
     * @return true if creation succeeded
     *
     * Validates shaders, creates pipeline layout, and creates pipeline.
     */
    bool create(vk::Device device, const GraphicsPipelineConfig& config);

    /**
     * @brief Cleanup pipeline resources
     */
    void cleanup(vk::Device device);

    //==========================================================================
    // PIPELINE BINDING
    //==========================================================================

    /**
     * @brief Bind pipeline to command buffer
     * @param cmd Command buffer
     */
    void bind(vk::CommandBuffer cmd);

    /**
     * @brief Bind descriptor sets
     * @param cmd Command buffer
     * @param sets Descriptor sets to bind
     * @param first_set First set index (for multiple sets)
     */
    void bind_descriptor_sets(vk::CommandBuffer cmd,
        std::span<vk::DescriptorSet> sets,
        uint32_t first_set = 0);

    /**
     * @brief Push constants
     * @param cmd Command buffer
     * @param stages Shader stages
     * @param offset Offset in push constant block
     * @param size Size of data
     * @param data Pointer to data
     */
    void push_constants(vk::CommandBuffer cmd,
        vk::ShaderStageFlags stages,
        uint32_t offset, uint32_t size,
        const void* data);

    /**
     * @brief Typed push constants
     */
    template <typename T>
    void push_constants_typed(vk::CommandBuffer cmd,
        vk::ShaderStageFlags stages,
        const T& data)
    {
        push_constants(cmd, stages, 0, sizeof(T), &data);
    }

    //==========================================================================
    // VERTEX/INDEX BUFFER BINDING
    //==========================================================================

    /**
     * @brief Bind vertex buffers
     * @param cmd Command buffer
     * @param first_binding First binding index
     * @param buffers Vertex buffers
     * @param offsets Offsets in buffers
     */
    void bind_vertex_buffers(vk::CommandBuffer cmd,
        uint32_t first_binding,
        std::span<vk::Buffer> buffers,
        std::span<vk::DeviceSize> offsets);

    /**
     * @brief Bind single vertex buffer (common case)
     */
    void bind_vertex_buffer(vk::CommandBuffer cmd,
        vk::Buffer buffer,
        vk::DeviceSize offset = 0,
        uint32_t binding = 0);

    /**
     * @brief Bind index buffer
     * @param cmd Command buffer
     * @param buffer Index buffer
     * @param offset Offset in buffer
     * @param index_type Index type (uint16 or uint32)
     */
    void bind_index_buffer(vk::CommandBuffer cmd,
        vk::Buffer buffer,
        vk::DeviceSize offset = 0,
        vk::IndexType index_type = vk::IndexType::eUint32);

    //==========================================================================
    // DYNAMIC STATE
    //==========================================================================

    /**
     * @brief Set viewport (if dynamic)
     * @param cmd Command buffer
     * @param viewport Viewport to set
     */
    void set_viewport(vk::CommandBuffer cmd, const vk::Viewport& viewport);

    /**
     * @brief Set scissor (if dynamic)
     * @param cmd Command buffer
     * @param scissor Scissor rectangle
     */
    void set_scissor(vk::CommandBuffer cmd, const vk::Rect2D& scissor);

    /**
     * @brief Set line width (if dynamic)
     */
    void set_line_width(vk::CommandBuffer cmd, float width);

    /**
     * @brief Set depth bias (if dynamic)
     */
    void set_depth_bias(vk::CommandBuffer cmd,
        float constant_factor,
        float clamp,
        float slope_factor);

    /**
     * @brief Set blend constants (if dynamic)
     */
    void set_blend_constants(vk::CommandBuffer cmd, const float constants[4]);

    //==========================================================================
    // DRAW COMMANDS
    //==========================================================================

    /**
     * @brief Draw vertices
     * @param cmd Command buffer
     * @param vertex_count Number of vertices
     * @param instance_count Number of instances
     * @param first_vertex First vertex index
     * @param first_instance First instance index
     */
    void draw(vk::CommandBuffer cmd,
        uint32_t vertex_count,
        uint32_t instance_count = 1,
        uint32_t first_vertex = 0,
        uint32_t first_instance = 0);

    /**
     * @brief Draw indexed vertices
     * @param cmd Command buffer
     * @param index_count Number of indices
     * @param instance_count Number of instances
     * @param first_index First index
     * @param vertex_offset Vertex offset added to index
     * @param first_instance First instance index
     */
    void draw_indexed(vk::CommandBuffer cmd,
        uint32_t index_count,
        uint32_t instance_count = 1,
        uint32_t first_index = 0,
        int32_t vertex_offset = 0,
        uint32_t first_instance = 0);

    /**
     * @brief Draw indirect (dispatch from GPU buffer)
     * @param cmd Command buffer
     * @param buffer Buffer containing vk::DrawIndirectCommand
     * @param offset Offset in buffer
     * @param draw_count Number of draws
     * @param stride Stride between commands
     */
    void draw_indirect(vk::CommandBuffer cmd,
        vk::Buffer buffer,
        vk::DeviceSize offset,
        uint32_t draw_count,
        uint32_t stride);

    /**
     * @brief Draw indexed indirect
     */
    void draw_indexed_indirect(vk::CommandBuffer cmd,
        vk::Buffer buffer,
        vk::DeviceSize offset,
        uint32_t draw_count,
        uint32_t stride);

    //==========================================================================
    // ACCESSORS
    //==========================================================================

    [[nodiscard]] vk::Pipeline get() const { return m_pipeline; }
    [[nodiscard]] vk::PipelineLayout get_layout() const { return m_layout; }
    [[nodiscard]] bool is_valid() const { return m_pipeline != nullptr; }

    /**
     * @brief Get shader reflections (for introspection)
     */
    [[nodiscard]] const GraphicsPipelineConfig& get_config() const
    {
        return m_config;
    }

private:
    vk::Device m_device;
    vk::Pipeline m_pipeline;
    vk::PipelineLayout m_layout;
    GraphicsPipelineConfig m_config; // Keep for reference

    /**
     * @brief Create pipeline layout from config
     */
    vk::PipelineLayout create_pipeline_layout(
        vk::Device device,
        const GraphicsPipelineConfig& config);

    /**
     * @brief Validate shader stages
     */
    bool validate_shaders(const GraphicsPipelineConfig& config);

    /**
     * @brief Build vertex input state from config
     */
    vk::PipelineVertexInputStateCreateInfo build_vertex_input_state(
        const GraphicsPipelineConfig& config,
        std::vector<vk::VertexInputBindingDescription>& bindings,
        std::vector<vk::VertexInputAttributeDescription>& attributes);

    /**
     * @brief Build input assembly state
     */
    vk::PipelineInputAssemblyStateCreateInfo build_input_assembly_state(
        const GraphicsPipelineConfig& config);

    /**
     * @brief Build tessellation state
     */
    vk::PipelineTessellationStateCreateInfo build_tessellation_state(
        const GraphicsPipelineConfig& config);

    /**
     * @brief Build viewport state
     */
    vk::PipelineViewportStateCreateInfo build_viewport_state(
        const GraphicsPipelineConfig& config,
        std::vector<vk::Viewport>& viewports,
        std::vector<vk::Rect2D>& scissors);

    /**
     * @brief Build rasterization state
     */
    vk::PipelineRasterizationStateCreateInfo build_rasterization_state(
        const GraphicsPipelineConfig& config);

    /**
     * @brief Build multisample state
     */
    vk::PipelineMultisampleStateCreateInfo build_multisample_state(
        const GraphicsPipelineConfig& config);

    /**
     * @brief Build depth stencil state
     */
    vk::PipelineDepthStencilStateCreateInfo build_depth_stencil_state(
        const GraphicsPipelineConfig& config);

    /**
     * @brief Build color blend state
     */
    vk::PipelineColorBlendStateCreateInfo build_color_blend_state(
        const GraphicsPipelineConfig& config,
        std::vector<vk::PipelineColorBlendAttachmentState>& attachments);

    /**
     * @brief Build dynamic state
     */
    vk::PipelineDynamicStateCreateInfo build_dynamic_state(
        const GraphicsPipelineConfig& config);
};

} // namespace MayaFlux::Core
