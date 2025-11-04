#include "VKGraphicsPipeline.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Core {

// ============================================================================
// GraphicsPipelineConfig - Static Preset Configurations
// ============================================================================

GraphicsPipelineConfig GraphicsPipelineConfig::default_3d()
{
    GraphicsPipelineConfig config;
    config.topology = vk::PrimitiveTopology::eTriangleList;
    config.polygon_mode = vk::PolygonMode::eFill;
    config.cull_mode = vk::CullModeFlagBits::eBack;
    config.front_face = vk::FrontFace::eCounterClockwise;
    config.depth_test_enable = true;
    config.depth_write_enable = true;
    config.depth_compare_op = vk::CompareOp::eLess;

    ColorBlendAttachment attachment;
    attachment.blend_enable = false;
    config.color_blend_attachments.push_back(attachment);

    return config;
}

GraphicsPipelineConfig GraphicsPipelineConfig::default_2d()
{
    GraphicsPipelineConfig config;
    config.topology = vk::PrimitiveTopology::eTriangleList;
    config.polygon_mode = vk::PolygonMode::eFill;
    config.cull_mode = vk::CullModeFlagBits::eNone;
    config.depth_test_enable = false;
    config.depth_write_enable = false;

    ColorBlendAttachment attachment;
    attachment.blend_enable = false;
    config.color_blend_attachments.push_back(attachment);

    return config;
}

GraphicsPipelineConfig GraphicsPipelineConfig::alpha_blended()
{
    GraphicsPipelineConfig config = default_3d();
    config.enable_alpha_blending();
    return config;
}

GraphicsPipelineConfig GraphicsPipelineConfig::additive_blended()
{
    GraphicsPipelineConfig config = default_3d();
    config.enable_additive_blending();
    return config;
}

GraphicsPipelineConfig GraphicsPipelineConfig::depth_only()
{
    GraphicsPipelineConfig config;
    config.topology = vk::PrimitiveTopology::eTriangleList;
    config.polygon_mode = vk::PolygonMode::eFill;
    config.cull_mode = vk::CullModeFlagBits::eBack;
    config.rasterizer_discard_enable = false;
    config.depth_test_enable = true;
    config.depth_write_enable = true;
    config.depth_compare_op = vk::CompareOp::eLess;
    config.color_blend_attachments.clear();
    return config;
}

// ============================================================================
// GraphicsPipelineConfig - Fluent Configuration Methods
// ============================================================================

void GraphicsPipelineConfig::enable_alpha_blending()
{
    if (color_blend_attachments.empty()) {
        color_blend_attachments.emplace_back();
    }

    for (auto& attachment : color_blend_attachments) {
        attachment.blend_enable = true;
        attachment.src_color_blend_factor = vk::BlendFactor::eSrcAlpha;
        attachment.dst_color_blend_factor = vk::BlendFactor::eOneMinusSrcAlpha;
        attachment.color_blend_op = vk::BlendOp::eAdd;
        attachment.src_alpha_blend_factor = vk::BlendFactor::eOne;
        attachment.dst_alpha_blend_factor = vk::BlendFactor::eZero;
        attachment.alpha_blend_op = vk::BlendOp::eAdd;
    }
}

void GraphicsPipelineConfig::enable_additive_blending()
{
    if (color_blend_attachments.empty()) {
        color_blend_attachments.emplace_back();
    }

    for (auto& attachment : color_blend_attachments) {
        attachment.blend_enable = true;
        attachment.src_color_blend_factor = vk::BlendFactor::eSrcAlpha;
        attachment.dst_color_blend_factor = vk::BlendFactor::eOne;
        attachment.color_blend_op = vk::BlendOp::eAdd;
        attachment.src_alpha_blend_factor = vk::BlendFactor::eOne;
        attachment.dst_alpha_blend_factor = vk::BlendFactor::eZero;
        attachment.alpha_blend_op = vk::BlendOp::eAdd;
    }
}

void GraphicsPipelineConfig::disable_depth_test()
{
    depth_test_enable = false;
    depth_write_enable = false;
}

void GraphicsPipelineConfig::enable_wireframe()
{
    polygon_mode = vk::PolygonMode::eLine;
}

void GraphicsPipelineConfig::enable_back_face_culling()
{
    cull_mode = vk::CullModeFlagBits::eBack;
}

void GraphicsPipelineConfig::disable_culling()
{
    cull_mode = vk::CullModeFlagBits::eNone;
}

// ============================================================================
// VKGraphicsPipeline - Lifecycle
// ============================================================================

VKGraphicsPipeline::~VKGraphicsPipeline()
{
    if (m_pipeline || m_layout) {
        MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "VKGraphicsPipeline destroyed without cleanup() - potential leak");
    }
}

VKGraphicsPipeline::VKGraphicsPipeline(VKGraphicsPipeline&& other) noexcept
    : m_device(other.m_device)
    , m_pipeline(other.m_pipeline)
    , m_layout(other.m_layout)
    , m_config(std::move(other.m_config))
{
    other.m_device = nullptr;
    other.m_pipeline = nullptr;
    other.m_layout = nullptr;
}

VKGraphicsPipeline& VKGraphicsPipeline::operator=(VKGraphicsPipeline&& other) noexcept
{
    if (this != &other) {
        if (m_pipeline || m_layout) {
            MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
                "VKGraphicsPipeline move-assigned without cleanup() - potential leak");
        }

        m_device = other.m_device;
        m_pipeline = other.m_pipeline;
        m_layout = other.m_layout;
        m_config = std::move(other.m_config);

        other.m_device = nullptr;
        other.m_pipeline = nullptr;
        other.m_layout = nullptr;
    }
    return *this;
}

void VKGraphicsPipeline::cleanup(vk::Device device)
{
    if (m_pipeline) {
        device.destroyPipeline(m_pipeline);
        m_pipeline = nullptr;
        MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Graphics pipeline destroyed");
    }

    if (m_layout) {
        device.destroyPipelineLayout(m_layout);
        m_layout = nullptr;
        MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Graphics pipeline layout destroyed");
    }
}

// ============================================================================
// Pipeline Creation
// ============================================================================

bool VKGraphicsPipeline::create(vk::Device device, const GraphicsPipelineConfig& config)
{
    if (!device) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot create graphics pipeline with null device");
        return false;
    }

    m_device = device;
    m_config = config;

    if (!validate_shaders(config)) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Shader validation failed");
        return false;
    }

    m_layout = create_pipeline_layout(device, config);
    if (!m_layout) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to create pipeline layout");
        return false;
    }

    std::vector<vk::PipelineShaderStageCreateInfo> shader_stages;
    if (config.vertex_shader) {
        shader_stages.push_back(config.vertex_shader->get_stage_create_info());
    }
    if (config.fragment_shader) {
        shader_stages.push_back(config.fragment_shader->get_stage_create_info());
    }
    if (config.geometry_shader) {
        shader_stages.push_back(config.geometry_shader->get_stage_create_info());
    }
    if (config.tess_control_shader) {
        shader_stages.push_back(config.tess_control_shader->get_stage_create_info());
    }
    if (config.tess_evaluation_shader) {
        shader_stages.push_back(config.tess_evaluation_shader->get_stage_create_info());
    }

    std::vector<vk::VertexInputBindingDescription> vertex_bindings;
    std::vector<vk::VertexInputAttributeDescription> vertex_attributes;
    auto vertex_input_state = build_vertex_input_state(config, vertex_bindings, vertex_attributes);

    auto input_assembly_state = build_input_assembly_state(config);
    auto tessellation_state = build_tessellation_state(config);

    std::vector<vk::Viewport> viewports;
    std::vector<vk::Rect2D> scissors;
    auto viewport_state = build_viewport_state(config, viewports, scissors);

    auto rasterization_state = build_rasterization_state(config);
    auto multisample_state = build_multisample_state(config);
    auto depth_stencil_state = build_depth_stencil_state(config);

    std::vector<vk::PipelineColorBlendAttachmentState> blend_attachments;
    auto color_blend_state = build_color_blend_state(config, blend_attachments);

    auto dynamic_state = build_dynamic_state(config);

    vk::GraphicsPipelineCreateInfo pipeline_info;
    pipeline_info.stageCount = static_cast<uint32_t>(shader_stages.size());
    pipeline_info.pStages = shader_stages.data();
    pipeline_info.pVertexInputState = &vertex_input_state;
    pipeline_info.pInputAssemblyState = &input_assembly_state;
    pipeline_info.pTessellationState = (config.tess_control_shader || config.tess_evaluation_shader) ? &tessellation_state : nullptr;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterization_state;
    pipeline_info.pMultisampleState = &multisample_state;
    pipeline_info.pDepthStencilState = &depth_stencil_state;
    pipeline_info.pColorBlendState = &color_blend_state;
    pipeline_info.pDynamicState = config.dynamic_states.empty() ? nullptr : &dynamic_state;
    pipeline_info.layout = m_layout;
    pipeline_info.renderPass = config.render_pass;
    pipeline_info.subpass = config.subpass;
    pipeline_info.basePipelineHandle = nullptr;
    pipeline_info.basePipelineIndex = -1;

    try {
        auto result = device.createGraphicsPipeline(config.cache, pipeline_info);
        if (result.result != vk::Result::eSuccess) {
            MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
                "Failed to create graphics pipeline: {}", vk::to_string(result.result));
            device.destroyPipelineLayout(m_layout);
            m_layout = nullptr;
            return false;
        }
        m_pipeline = result.value;
    } catch (const vk::SystemError& e) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to create graphics pipeline: {}", e.what());
        device.destroyPipelineLayout(m_layout);
        m_layout = nullptr;
        return false;
    }

    MF_INFO(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Graphics pipeline created ({} shader stages)", shader_stages.size());

    return true;
}

vk::PipelineLayout VKGraphicsPipeline::create_pipeline_layout(
    vk::Device device,
    const GraphicsPipelineConfig& config)
{
    vk::PipelineLayoutCreateInfo layout_info;
    layout_info.setLayoutCount = static_cast<uint32_t>(config.descriptor_set_layouts.size());
    layout_info.pSetLayouts = config.descriptor_set_layouts.empty() ? nullptr : config.descriptor_set_layouts.data();
    layout_info.pushConstantRangeCount = static_cast<uint32_t>(config.push_constant_ranges.size());
    layout_info.pPushConstantRanges = config.push_constant_ranges.empty() ? nullptr : config.push_constant_ranges.data();

    vk::PipelineLayout layout;
    try {
        layout = device.createPipelineLayout(layout_info);
    } catch (const vk::SystemError& e) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to create pipeline layout: {}", e.what());
        return nullptr;
    }

    MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Graphics pipeline layout created ({} sets, {} push constant ranges)",
        config.descriptor_set_layouts.size(), config.push_constant_ranges.size());

    return layout;
}

bool VKGraphicsPipeline::validate_shaders(const GraphicsPipelineConfig& config)
{
    if (!config.vertex_shader) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Graphics pipeline requires a vertex shader");
        return false;
    }

    if (!config.vertex_shader->is_valid()) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Vertex shader is not valid");
        return false;
    }

    if (config.vertex_shader->get_stage() != vk::ShaderStageFlagBits::eVertex) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Vertex shader has wrong stage: {}", vk::to_string(config.vertex_shader->get_stage()));
        return false;
    }

    if (config.fragment_shader && config.fragment_shader->get_stage() != vk::ShaderStageFlagBits::eFragment) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Fragment shader has wrong stage: {}", vk::to_string(config.fragment_shader->get_stage()));
        return false;
    }

    if (config.geometry_shader && config.geometry_shader->get_stage() != vk::ShaderStageFlagBits::eGeometry) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Geometry shader has wrong stage");
        return false;
    }

    if (config.tess_control_shader && config.tess_control_shader->get_stage() != vk::ShaderStageFlagBits::eTessellationControl) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Tessellation control shader has wrong stage");
        return false;
    }

    if (config.tess_evaluation_shader && config.tess_evaluation_shader->get_stage() != vk::ShaderStageFlagBits::eTessellationEvaluation) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Tessellation evaluation shader has wrong stage");
        return false;
    }

    return true;
}

// ============================================================================
// Pipeline State Builders
// ============================================================================

vk::PipelineVertexInputStateCreateInfo VKGraphicsPipeline::build_vertex_input_state(
    const GraphicsPipelineConfig& config,
    std::vector<vk::VertexInputBindingDescription>& bindings,
    std::vector<vk::VertexInputAttributeDescription>& attributes)
{
    if (!config.vertex_bindings.empty() || !config.vertex_attributes.empty()) {
        MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Using explicit vertex bindings/attributes from config "
            "({} bindings, {} attributes)",
            config.vertex_bindings.size(), config.vertex_attributes.size());

        for (const auto& binding : config.vertex_bindings) {
            vk::VertexInputBindingDescription vk_binding;
            vk_binding.binding = binding.binding;
            vk_binding.stride = binding.stride;
            vk_binding.inputRate = binding.input_rate;
            bindings.push_back(vk_binding);
        }

        for (const auto& attribute : config.vertex_attributes) {
            vk::VertexInputAttributeDescription vk_attr;
            vk_attr.location = attribute.location;
            vk_attr.binding = attribute.binding;
            vk_attr.format = attribute.format;
            vk_attr.offset = attribute.offset;
            attributes.push_back(vk_attr);
        }
    } else if (
        config.use_vertex_shader_reflection
        && config.vertex_shader
        && config.vertex_shader->has_vertex_input()) {

        MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Using vertex input from shader reflection");
        const auto& vertex_input = config.vertex_shader->get_vertex_input();

        for (const auto& binding : vertex_input.bindings) {
            vk::VertexInputBindingDescription vk_binding;
            vk_binding.binding = binding.binding;
            vk_binding.stride = binding.stride;
            vk_binding.inputRate = binding.rate;
            bindings.push_back(vk_binding);
        }

        for (const auto& attribute : vertex_input.attributes) {
            vk::VertexInputAttributeDescription vk_attr;
            vk_attr.location = attribute.location;
            vk_attr.binding = 0;
            vk_attr.format = attribute.format;
            vk_attr.offset = attribute.offset;
            attributes.push_back(vk_attr);
        }
    } else {
        MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "No vertex input: using empty vertex state (full-screen quad or compute)");
    }

    vk::PipelineVertexInputStateCreateInfo vertex_input;
    vertex_input.vertexBindingDescriptionCount = static_cast<uint32_t>(bindings.size());
    vertex_input.pVertexBindingDescriptions = bindings.empty() ? nullptr : bindings.data();
    vertex_input.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
    vertex_input.pVertexAttributeDescriptions = attributes.empty() ? nullptr : attributes.data();

    return vertex_input;
}

vk::PipelineInputAssemblyStateCreateInfo VKGraphicsPipeline::build_input_assembly_state(
    const GraphicsPipelineConfig& config)
{
    vk::PipelineInputAssemblyStateCreateInfo input_assembly;
    input_assembly.topology = config.topology;
    input_assembly.primitiveRestartEnable = config.primitive_restart_enable;
    return input_assembly;
}

vk::PipelineTessellationStateCreateInfo VKGraphicsPipeline::build_tessellation_state(
    const GraphicsPipelineConfig& config)
{
    vk::PipelineTessellationStateCreateInfo tessellation;
    tessellation.patchControlPoints = config.patch_control_points;
    return tessellation;
}

vk::PipelineViewportStateCreateInfo VKGraphicsPipeline::build_viewport_state(
    const GraphicsPipelineConfig& config,
    std::vector<vk::Viewport>& viewports,
    std::vector<vk::Rect2D>& scissors)
{
    if (!config.dynamic_viewport) {
        viewports.push_back(config.static_viewport);
    } else {
        viewports.emplace_back();
    }

    if (!config.dynamic_scissor) {
        scissors.push_back(config.static_scissor);
    } else {
        scissors.emplace_back();
    }

    vk::PipelineViewportStateCreateInfo viewport_state;
    viewport_state.viewportCount = static_cast<uint32_t>(viewports.size());
    viewport_state.pViewports = config.dynamic_viewport ? nullptr : viewports.data();
    viewport_state.scissorCount = static_cast<uint32_t>(scissors.size());
    viewport_state.pScissors = config.dynamic_scissor ? nullptr : scissors.data();

    return viewport_state;
}

vk::PipelineRasterizationStateCreateInfo VKGraphicsPipeline::build_rasterization_state(
    const GraphicsPipelineConfig& config)
{
    vk::PipelineRasterizationStateCreateInfo rasterization;
    rasterization.depthClampEnable = config.depth_clamp_enable;
    rasterization.rasterizerDiscardEnable = config.rasterizer_discard_enable;
    rasterization.polygonMode = config.polygon_mode;
    rasterization.cullMode = config.cull_mode;
    rasterization.frontFace = config.front_face;
    rasterization.depthBiasEnable = config.depth_bias_enable;
    rasterization.depthBiasConstantFactor = config.depth_bias_constant_factor;
    rasterization.depthBiasClamp = config.depth_bias_clamp;
    rasterization.depthBiasSlopeFactor = config.depth_bias_slope_factor;
    rasterization.lineWidth = config.line_width;

    return rasterization;
}

vk::PipelineMultisampleStateCreateInfo VKGraphicsPipeline::build_multisample_state(
    const GraphicsPipelineConfig& config)
{
    vk::PipelineMultisampleStateCreateInfo multisample;
    multisample.rasterizationSamples = config.rasterization_samples;
    multisample.sampleShadingEnable = config.sample_shading_enable;
    multisample.minSampleShading = config.min_sample_shading;
    multisample.pSampleMask = config.sample_mask.empty() ? nullptr : config.sample_mask.data();
    multisample.alphaToCoverageEnable = config.alpha_to_coverage_enable;
    multisample.alphaToOneEnable = config.alpha_to_one_enable;

    return multisample;
}

vk::PipelineDepthStencilStateCreateInfo VKGraphicsPipeline::build_depth_stencil_state(
    const GraphicsPipelineConfig& config)
{
    vk::PipelineDepthStencilStateCreateInfo depth_stencil;
    depth_stencil.depthTestEnable = config.depth_test_enable;
    depth_stencil.depthWriteEnable = config.depth_write_enable;
    depth_stencil.depthCompareOp = config.depth_compare_op;
    depth_stencil.depthBoundsTestEnable = config.depth_bounds_test_enable;
    depth_stencil.minDepthBounds = config.min_depth_bounds;
    depth_stencil.maxDepthBounds = config.max_depth_bounds;
    depth_stencil.stencilTestEnable = config.stencil_test_enable;
    depth_stencil.front = config.front_stencil;
    depth_stencil.back = config.back_stencil;

    return depth_stencil;
}

vk::PipelineColorBlendStateCreateInfo VKGraphicsPipeline::build_color_blend_state(
    const GraphicsPipelineConfig& config,
    std::vector<vk::PipelineColorBlendAttachmentState>& attachments)
{
    for (const auto& config_attachment : config.color_blend_attachments) {
        vk::PipelineColorBlendAttachmentState attachment;
        attachment.blendEnable = config_attachment.blend_enable;
        attachment.srcColorBlendFactor = config_attachment.src_color_blend_factor;
        attachment.dstColorBlendFactor = config_attachment.dst_color_blend_factor;
        attachment.colorBlendOp = config_attachment.color_blend_op;
        attachment.srcAlphaBlendFactor = config_attachment.src_alpha_blend_factor;
        attachment.dstAlphaBlendFactor = config_attachment.dst_alpha_blend_factor;
        attachment.alphaBlendOp = config_attachment.alpha_blend_op;
        attachment.colorWriteMask = config_attachment.color_write_mask;
        attachments.push_back(attachment);
    }

    vk::PipelineColorBlendStateCreateInfo color_blend;
    color_blend.logicOpEnable = config.logic_op_enable;
    color_blend.logicOp = config.logic_op;
    color_blend.attachmentCount = static_cast<uint32_t>(attachments.size());
    color_blend.pAttachments = attachments.empty() ? nullptr : attachments.data();
    color_blend.blendConstants[0] = config.blend_constants[0];
    color_blend.blendConstants[1] = config.blend_constants[1];
    color_blend.blendConstants[2] = config.blend_constants[2];
    color_blend.blendConstants[3] = config.blend_constants[3];

    return color_blend;
}

vk::PipelineDynamicStateCreateInfo VKGraphicsPipeline::build_dynamic_state(
    const GraphicsPipelineConfig& config)
{
    vk::PipelineDynamicStateCreateInfo dynamic_state;
    dynamic_state.dynamicStateCount = static_cast<uint32_t>(config.dynamic_states.size());
    dynamic_state.pDynamicStates = config.dynamic_states.data();
    return dynamic_state;
}

// ============================================================================
// Pipeline Binding
// ============================================================================

void VKGraphicsPipeline::bind(vk::CommandBuffer cmd)
{
    if (!m_pipeline) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot bind invalid graphics pipeline");
        return;
    }

    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline);
}

void VKGraphicsPipeline::bind_descriptor_sets(
    vk::CommandBuffer cmd,
    std::span<vk::DescriptorSet> sets,
    uint32_t first_set)
{
    if (!m_layout) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot bind descriptor sets without pipeline layout");
        return;
    }

    if (sets.empty()) {
        MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Binding empty descriptor sets");
        return;
    }

    cmd.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        m_layout,
        first_set,
        static_cast<uint32_t>(sets.size()),
        sets.data(),
        0, nullptr);
}

void VKGraphicsPipeline::push_constants(
    vk::CommandBuffer cmd,
    vk::ShaderStageFlags stages,
    uint32_t offset,
    uint32_t size,
    const void* data)
{
    if (!m_layout) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot push constants without pipeline layout");
        return;
    }

    if (!data) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot push null data");
        return;
    }

    cmd.pushConstants(m_layout, stages, offset, size, data);
}

// ============================================================================
// Vertex/Index Buffer Binding
// ============================================================================

void VKGraphicsPipeline::bind_vertex_buffers(
    vk::CommandBuffer cmd,
    uint32_t first_binding,
    std::span<vk::Buffer> buffers,
    std::span<vk::DeviceSize> offsets)
{
    if (buffers.empty()) {
        MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Binding empty vertex buffers");
        return;
    }

    if (buffers.size() != offsets.size()) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Buffer count ({}) does not match offset count ({})",
            buffers.size(), offsets.size());
        return;
    }

    cmd.bindVertexBuffers(
        first_binding,
        static_cast<uint32_t>(buffers.size()),
        buffers.data(),
        offsets.data());
}

void VKGraphicsPipeline::bind_vertex_buffer(
    vk::CommandBuffer cmd,
    vk::Buffer buffer,
    vk::DeviceSize offset,
    uint32_t binding)
{
    if (!buffer) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot bind null vertex buffer");
        return;
    }

    cmd.bindVertexBuffers(binding, 1, &buffer, &offset);
}

void VKGraphicsPipeline::bind_index_buffer(
    vk::CommandBuffer cmd,
    vk::Buffer buffer,
    vk::DeviceSize offset,
    vk::IndexType index_type)
{
    if (!buffer) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot bind null index buffer");
        return;
    }

    cmd.bindIndexBuffer(buffer, offset, index_type);
}

// ============================================================================
// Dynamic State
// ============================================================================

void VKGraphicsPipeline::set_viewport(vk::CommandBuffer cmd, const vk::Viewport& viewport)
{
    cmd.setViewport(0, 1, &viewport);
}

void VKGraphicsPipeline::set_scissor(vk::CommandBuffer cmd, const vk::Rect2D& scissor)
{
    cmd.setScissor(0, 1, &scissor);
}

void VKGraphicsPipeline::set_line_width(vk::CommandBuffer cmd, float width)
{
    cmd.setLineWidth(width);
}

void VKGraphicsPipeline::set_depth_bias(
    vk::CommandBuffer cmd,
    float constant_factor,
    float clamp,
    float slope_factor)
{
    cmd.setDepthBias(constant_factor, clamp, slope_factor);
}

void VKGraphicsPipeline::set_blend_constants(vk::CommandBuffer cmd, const float constants[4])
{
    cmd.setBlendConstants(constants);
}

// ============================================================================
// Draw Commands
// ============================================================================

void VKGraphicsPipeline::draw(
    vk::CommandBuffer cmd,
    uint32_t vertex_count,
    uint32_t instance_count,
    uint32_t first_vertex,
    uint32_t first_instance)
{
    if (!m_pipeline) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot draw with invalid pipeline");
        return;
    }

    if (vertex_count == 0) {
        MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Drawing with zero vertices");
        return;
    }

    cmd.draw(vertex_count, instance_count, first_vertex, first_instance);
}

void VKGraphicsPipeline::draw_indexed(
    vk::CommandBuffer cmd,
    uint32_t index_count,
    uint32_t instance_count,
    uint32_t first_index,
    int32_t vertex_offset,
    uint32_t first_instance)
{
    if (!m_pipeline) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot draw indexed with invalid pipeline");
        return;
    }

    if (index_count == 0) {
        MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Drawing with zero indices");
        return;
    }

    cmd.drawIndexed(index_count, instance_count, first_index, vertex_offset, first_instance);
}

void VKGraphicsPipeline::draw_indirect(
    vk::CommandBuffer cmd,
    vk::Buffer buffer,
    vk::DeviceSize offset,
    uint32_t draw_count,
    uint32_t stride)
{
    if (!m_pipeline) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot draw indirect with invalid pipeline");
        return;
    }

    if (!buffer) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot draw indirect with null buffer");
        return;
    }

    if (draw_count == 0) {
        MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Drawing with zero draw count");
        return;
    }

    cmd.drawIndirect(buffer, offset, draw_count, stride);
}

void VKGraphicsPipeline::draw_indexed_indirect(
    vk::CommandBuffer cmd,
    vk::Buffer buffer,
    vk::DeviceSize offset,
    uint32_t draw_count,
    uint32_t stride)
{
    if (!m_pipeline) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot draw indexed indirect with invalid pipeline");
        return;
    }

    if (!buffer) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot draw indexed indirect with null buffer");
        return;
    }

    if (draw_count == 0) {
        MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Drawing with zero draw count");
        return;
    }

    cmd.drawIndexedIndirect(buffer, offset, draw_count, stride);
}

} // namespace MayaFlux::Core
