#include "RenderFlow.hpp"

#include "LayoutTranslator.hpp"

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKGraphicsPipeline.hpp"

#include "MayaFlux/Core/Backends/Windowing/Window.hpp"
#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/DisplayService.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Portal::Graphics {

bool RenderFlow::s_initialized = false;

namespace {

    /**
     * @brief Translate semantic VertexLayout to Vulkan bindings/attributes
     * @param layout Semantic vertex layout
     * @return Tuple of (bindings, attributes)
     */
    static std::pair<
        std::vector<Core::VertexBinding>,
        std::vector<Core::VertexAttribute>>
    translate_semantic_layout(const Kakshya::VertexLayout& layout)
    {
        using namespace MayaFlux::Portal::Graphics;

        auto [vk_bindings, vk_attributes] = VertexLayoutTranslator::translate_layout(layout);

        MF_DEBUG(Journal::Component::Portal, Journal::Context::Rendering,
            "Translated semantic vertex layout: {} bindings, {} attributes",
            vk_bindings.size(), vk_attributes.size());

        return { vk_bindings, vk_attributes };
    }

} // anonymous namespace

//==============================================================================
// Initialization
//==============================================================================

bool RenderFlow::initialize()
{
    if (s_initialized) {
        MF_WARN(Journal::Component::Portal, Journal::Context::Rendering,
            "RenderFlow already initialized (static flag)");
        return true;
    }

    if (m_shader_foundry) {
        MF_WARN(Journal::Component::Portal, Journal::Context::Rendering,
            "RenderFlow already initialized");
        return true;
    }

    m_shader_foundry = &ShaderFoundry::instance();
    if (!m_shader_foundry->is_initialized()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "ShaderFoundry must be initialized before RenderFlow");
        return false;
    }

    m_display_service = Registry::BackendRegistry::instance()
                            .get_service<Registry::Service::DisplayService>();

    if (!m_display_service) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "DisplayService not found in BackendRegistry");
        return false;
    }

    s_initialized = true;

    MF_INFO(Journal::Component::Portal, Journal::Context::Rendering,
        "RenderFlow initialized");
    return true;
}

void RenderFlow::shutdown()
{
    if (!s_initialized) {
        return;
    }

    MF_INFO(Journal::Component::Portal, Journal::Context::Rendering,
        "Shutting down RenderFlow...");

    if (!m_shader_foundry || !m_shader_foundry->is_initialized()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Cannot shutdown RenderFlow: ShaderFoundry not initialized");
        return;
    }

    auto device = m_shader_foundry->get_device();

    if (!device) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Cannot shutdown RenderFlow: Vulkan device is null");
        return;
    }

    for (auto& [id, state] : m_pipelines) {
        if (state.pipeline) {
            state.pipeline->cleanup(device);
        }

        if (state.layout) {
            device.destroyPipelineLayout(state.layout);
        }
    }
    m_pipelines.clear();

    m_window_associations.clear();

    m_shader_foundry = nullptr;
    m_display_service = nullptr;

    s_initialized = false;

    MF_INFO(Journal::Component::Portal, Journal::Context::Rendering,
        "RenderFlow shutdown complete");
}

//==============================================================================
// Utility Conversions (Portal enums → Vulkan enums)
//==============================================================================

namespace {

    vk::PrimitiveTopology to_vk_topology(PrimitiveTopology topology)
    {
        switch (topology) {
        case PrimitiveTopology::POINT_LIST:
            return vk::PrimitiveTopology::ePointList;
        case PrimitiveTopology::LINE_LIST:
            return vk::PrimitiveTopology::eLineList;
        case PrimitiveTopology::LINE_STRIP:
            return vk::PrimitiveTopology::eLineStrip;
        case PrimitiveTopology::TRIANGLE_LIST:
            return vk::PrimitiveTopology::eTriangleList;
        case PrimitiveTopology::TRIANGLE_STRIP:
            return vk::PrimitiveTopology::eTriangleStrip;
        case PrimitiveTopology::TRIANGLE_FAN:
            return vk::PrimitiveTopology::eTriangleFan;
        default:
            return vk::PrimitiveTopology::eTriangleList;
        }
    }

    vk::PolygonMode to_vk_polygon_mode(PolygonMode mode)
    {
        switch (mode) {
        case PolygonMode::FILL:
            return vk::PolygonMode::eFill;
        case PolygonMode::LINE:
            return vk::PolygonMode::eLine;
        case PolygonMode::POINT:
            return vk::PolygonMode::ePoint;
        default:
            return vk::PolygonMode::eFill;
        }
    }

    vk::CullModeFlags to_vk_cull_mode(CullMode mode)
    {
        switch (mode) {
        case CullMode::NONE:
            return vk::CullModeFlagBits::eNone;
        case CullMode::FRONT:
            return vk::CullModeFlagBits::eFront;
        case CullMode::BACK:
            return vk::CullModeFlagBits::eBack;
        case CullMode::FRONT_AND_BACK:
            return vk::CullModeFlagBits::eFrontAndBack;
        default:
            return vk::CullModeFlagBits::eBack;
        }
    }

    vk::CompareOp to_vk_compare_op(CompareOp op)
    {
        switch (op) {
        case CompareOp::NEVER:
            return vk::CompareOp::eNever;
        case CompareOp::LESS:
            return vk::CompareOp::eLess;
        case CompareOp::EQUAL:
            return vk::CompareOp::eEqual;
        case CompareOp::LESS_OR_EQUAL:
            return vk::CompareOp::eLessOrEqual;
        case CompareOp::GREATER:
            return vk::CompareOp::eGreater;
        case CompareOp::NOT_EQUAL:
            return vk::CompareOp::eNotEqual;
        case CompareOp::GREATER_OR_EQUAL:
            return vk::CompareOp::eGreaterOrEqual;
        case CompareOp::ALWAYS:
            return vk::CompareOp::eAlways;
        default:
            return vk::CompareOp::eLess;
        }
    }

    vk::BlendFactor to_vk_blend_factor(BlendFactor factor)
    {
        switch (factor) {
        case BlendFactor::ZERO:
            return vk::BlendFactor::eZero;
        case BlendFactor::ONE:
            return vk::BlendFactor::eOne;
        case BlendFactor::SRC_COLOR:
            return vk::BlendFactor::eSrcColor;
        case BlendFactor::ONE_MINUS_SRC_COLOR:
            return vk::BlendFactor::eOneMinusSrcColor;
        case BlendFactor::DST_COLOR:
            return vk::BlendFactor::eDstColor;
        case BlendFactor::ONE_MINUS_DST_COLOR:
            return vk::BlendFactor::eOneMinusDstColor;
        case BlendFactor::SRC_ALPHA:
            return vk::BlendFactor::eSrcAlpha;
        case BlendFactor::ONE_MINUS_SRC_ALPHA:
            return vk::BlendFactor::eOneMinusSrcAlpha;
        case BlendFactor::DST_ALPHA:
            return vk::BlendFactor::eDstAlpha;
        case BlendFactor::ONE_MINUS_DST_ALPHA:
            return vk::BlendFactor::eOneMinusDstAlpha;
        default:
            return vk::BlendFactor::eOne;
        }
    }

    vk::BlendOp to_vk_blend_op(BlendOp op)
    {
        switch (op) {
        case BlendOp::ADD:
            return vk::BlendOp::eAdd;
        case BlendOp::SUBTRACT:
            return vk::BlendOp::eSubtract;
        case BlendOp::REVERSE_SUBTRACT:
            return vk::BlendOp::eReverseSubtract;
        case BlendOp::MIN:
            return vk::BlendOp::eMin;
        case BlendOp::MAX:
            return vk::BlendOp::eMax;
        default:
            return vk::BlendOp::eAdd;
        }
    }
} // anonymous namespace

//==============================================================================
// Pipeline Creation
//==============================================================================

RenderPipelineID RenderFlow::create_pipeline(
    const RenderPipelineConfig& config,
    const std::vector<vk::Format>& color_formats,
    vk::Format depth_format)
{
    if (!is_initialized()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "RenderFlow not initialized");
        return INVALID_RENDER_PIPELINE;
    }

    if (config.vertex_shader == INVALID_SHADER) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Vertex shader required for graphics pipeline");
        return INVALID_RENDER_PIPELINE;
    }

    if (color_formats.empty()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "At least one color format required for dynamic rendering pipeline");
        return INVALID_RENDER_PIPELINE;
    }

    Core::GraphicsPipelineConfig vk_config;

    vk_config.vertex_shader = m_shader_foundry->get_vk_shader_module(config.vertex_shader);
    if (config.fragment_shader != INVALID_SHADER) {
        vk_config.fragment_shader = m_shader_foundry->get_vk_shader_module(config.fragment_shader);
    }
    if (config.geometry_shader != INVALID_SHADER) {
        vk_config.geometry_shader = m_shader_foundry->get_vk_shader_module(config.geometry_shader);
    }
    if (config.tess_control_shader != INVALID_SHADER) {
        vk_config.tess_control_shader = m_shader_foundry->get_vk_shader_module(config.tess_control_shader);
    }
    if (config.tess_eval_shader != INVALID_SHADER) {
        vk_config.tess_evaluation_shader = m_shader_foundry->get_vk_shader_module(config.tess_eval_shader);
    }

    if (config.semantic_vertex_layout.has_value()) {
        MF_INFO(Journal::Component::Portal, Journal::Context::Rendering,
            "Pipeline using semantic VertexLayout ({} vertices, {} attributes)",
            config.semantic_vertex_layout->vertex_count,
            config.semantic_vertex_layout->attributes.size());

        auto [vk_bindings, vk_attributes] = translate_semantic_layout(
            config.semantic_vertex_layout.value());

        vk_config.vertex_bindings = vk_bindings;
        vk_config.vertex_attributes = vk_attributes;
        vk_config.use_vertex_shader_reflection = false;

    } else if (!config.vertex_bindings.empty() || !config.vertex_attributes.empty()) {
        MF_INFO(Journal::Component::Portal, Journal::Context::Rendering,
            "Pipeline using explicit vertex config ({} bindings, {} attributes)",
            config.vertex_bindings.size(), config.vertex_attributes.size());

        for (const auto& binding : config.vertex_bindings) {
            Core::VertexBinding vk_binding {};
            vk_binding.binding = binding.binding;
            vk_binding.stride = binding.stride;
            vk_binding.input_rate = binding.per_instance ? vk::VertexInputRate::eInstance : vk::VertexInputRate::eVertex;
            vk_config.vertex_bindings.push_back(vk_binding);
        }

        for (const auto& attr : config.vertex_attributes) {
            Core::VertexAttribute vk_attr {};
            vk_attr.location = attr.location;
            vk_attr.binding = attr.binding;
            vk_attr.format = attr.format;
            vk_attr.offset = attr.offset;
            vk_config.vertex_attributes.push_back(vk_attr);
        }

        vk_config.use_vertex_shader_reflection = false;
    } else {
        MF_DEBUG(Journal::Component::Portal, Journal::Context::Rendering,
            "Pipeline will use shader reflection for vertex input");
        vk_config.use_vertex_shader_reflection = config.use_vertex_shader_reflection;
    }

    vk_config.topology = to_vk_topology(config.topology);
    vk_config.primitive_restart_enable = false;

    vk_config.polygon_mode = to_vk_polygon_mode(config.rasterization.polygon_mode);
    vk_config.cull_mode = to_vk_cull_mode(config.rasterization.cull_mode);
    vk_config.front_face = config.rasterization.front_face_ccw ? vk::FrontFace::eCounterClockwise : vk::FrontFace::eClockwise;
    vk_config.line_width = config.rasterization.line_width;
    vk_config.depth_clamp_enable = config.rasterization.depth_clamp;
    vk_config.depth_bias_enable = config.rasterization.depth_bias;

    vk_config.depth_test_enable = config.depth_stencil.depth_test_enable;
    vk_config.depth_write_enable = config.depth_stencil.depth_write_enable;
    vk_config.depth_compare_op = to_vk_compare_op(config.depth_stencil.depth_compare_op);
    vk_config.stencil_test_enable = config.depth_stencil.stencil_test_enable;

    for (const auto& blend : config.blend_attachments) {
        Core::ColorBlendAttachment vk_blend;
        vk_blend.blend_enable = blend.blend_enable;
        vk_blend.src_color_blend_factor = to_vk_blend_factor(blend.src_color_factor);
        vk_blend.dst_color_blend_factor = to_vk_blend_factor(blend.dst_color_factor);
        vk_blend.color_blend_op = to_vk_blend_op(blend.color_blend_op);
        vk_blend.src_alpha_blend_factor = to_vk_blend_factor(blend.src_alpha_factor);
        vk_blend.dst_alpha_blend_factor = to_vk_blend_factor(blend.dst_alpha_factor);
        vk_blend.alpha_blend_op = to_vk_blend_op(blend.alpha_blend_op);
        vk_config.color_blend_attachments.push_back(vk_blend);
    }

    std::vector<vk::DescriptorSetLayout> layouts;
    for (const auto& desc_set : config.descriptor_sets) {
        std::vector<vk::DescriptorSetLayoutBinding> bindings;
        for (const auto& binding : desc_set) {
            vk::DescriptorSetLayoutBinding vk_binding;
            vk_binding.binding = binding.binding;
            vk_binding.descriptorType = binding.type;
            vk_binding.descriptorCount = 1;
            vk_binding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
            bindings.push_back(vk_binding);
        }

        vk::DescriptorSetLayoutCreateInfo layout_info;
        layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
        layout_info.pBindings = bindings.data();

        auto layout = m_shader_foundry->get_device().createDescriptorSetLayout(layout_info);
        layouts.push_back(layout);
    }
    vk_config.descriptor_set_layouts = layouts;

    if (config.push_constant_size > 0) {
        vk::PushConstantRange range;
        range.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
        range.offset = 0;
        range.size = static_cast<uint32_t>(config.push_constant_size);
        vk_config.push_constant_ranges.push_back(range);
    }

    vk_config.color_attachment_formats = color_formats;
    vk_config.depth_attachment_format = depth_format;

    vk_config.dynamic_states = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor
    };

    auto pipeline = std::make_shared<Core::VKGraphicsPipeline>();
    if (!pipeline->create(m_shader_foundry->get_device(), vk_config)) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Failed to create VKGraphicsPipeline for dynamic rendering");

        for (auto layout : layouts) {
            m_shader_foundry->get_device().destroyDescriptorSetLayout(layout);
        }
        return INVALID_RENDER_PIPELINE;
    }

    auto pipeline_id = m_next_pipeline_id.fetch_add(1);
    PipelineState state;
    state.shader_ids = { config.vertex_shader, config.fragment_shader };
    state.pipeline = pipeline;
    state.layouts = layouts;
    state.layout = pipeline->get_layout();
    m_pipelines[pipeline_id] = std::move(state);

    MF_INFO(Journal::Component::Portal, Journal::Context::Rendering,
        "Dynamic rendering pipeline created (ID: {}, {} color attachments)",
        pipeline_id, color_formats.size());

    return pipeline_id;
}

void RenderFlow::destroy_pipeline(RenderPipelineID pipeline_id)
{
    auto it = m_pipelines.find(pipeline_id);
    if (it == m_pipelines.end()) {
        return;
    }

    auto device = m_shader_foundry->get_device();

    if (it->second.pipeline) {
        it->second.pipeline->cleanup(device);
    }

    if (it->second.layout) {
        device.destroyPipelineLayout(it->second.layout);
    }

    for (auto layout : it->second.layouts) {
        if (layout) {
            device.destroyDescriptorSetLayout(layout);
        }
    }

    m_pipelines.erase(it);

    MF_DEBUG(Journal::Component::Portal, Journal::Context::Rendering,
        "Destroyed graphics pipeline (ID: {})", pipeline_id);
}

//==============================================================================
// Dynamic Rendering
//==============================================================================

void RenderFlow::begin_rendering(
    CommandBufferID cmd_id,
    const std::shared_ptr<Core::Window>& window,
    vk::Image swapchain_image,
    const std::array<float, 4>& clear_color)
{
    auto cmd = m_shader_foundry->get_command_buffer(cmd_id);
    if (!cmd) {
        MF_RT_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Invalid command buffer ID: {}", cmd_id);
        return;
    }

    if (!window) {
        MF_RT_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Cannot begin rendering for null window");
        return;
    }

    if (!swapchain_image) {
        MF_RT_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Cannot begin rendering with null swapchain image");
        return;
    }

    auto it = m_window_associations.find(window);
    if (it == m_window_associations.end()) {
        MF_WARN(Journal::Component::Portal, Journal::Context::Rendering,
            "Window '{}' not registered for rendering. "
            "Call register_window_for_rendering() first.",
            window->get_create_info().title);
        m_window_associations.emplace(window, WindowRenderAssociation { .window = window, .swapchain_image = swapchain_image });
    } else {
        it->second.swapchain_image = swapchain_image;
    }

    uint32_t width = 0, height = 0;
    m_display_service->get_swapchain_extent(window, width, height);

    if (width == 0 || height == 0) {
        MF_RT_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Invalid swapchain extent for window '{}': {}x{}",
            window->get_create_info().title, width, height);
        return;
    }

    vk::ImageView image_view = get_current_image_view(window);
    if (!image_view) {
        MF_RT_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Failed to get image view for window '{}'",
            window->get_create_info().title);
        return;
    }

    vk::ImageMemoryBarrier pre_barrier {};
    pre_barrier.oldLayout = vk::ImageLayout::eUndefined;
    pre_barrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
    pre_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    pre_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    pre_barrier.image = swapchain_image;
    pre_barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    pre_barrier.subresourceRange.baseMipLevel = 0;
    pre_barrier.subresourceRange.levelCount = 1;
    pre_barrier.subresourceRange.baseArrayLayer = 0;
    pre_barrier.subresourceRange.layerCount = 1;
    pre_barrier.srcAccessMask = vk::AccessFlagBits::eNone;
    pre_barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

    cmd.pipelineBarrier(
        vk::PipelineStageFlagBits::eTopOfPipe,
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::DependencyFlags {},
        0, nullptr,
        0, nullptr,
        1, &pre_barrier);

    vk::RenderingAttachmentInfo color_attachment {};
    color_attachment.sType = vk::StructureType::eRenderingAttachmentInfo;
    color_attachment.pNext = nullptr;
    color_attachment.imageView = image_view;
    color_attachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    color_attachment.resolveMode = vk::ResolveModeFlagBits::eNone;
    color_attachment.resolveImageView = nullptr;
    color_attachment.resolveImageLayout = vk::ImageLayout::eUndefined;
    color_attachment.loadOp = vk::AttachmentLoadOp::eClear;
    color_attachment.storeOp = vk::AttachmentStoreOp::eStore;
    color_attachment.clearValue.color = vk::ClearColorValue(clear_color);

    vk::RenderingInfo rendering_info {};
    rendering_info.sType = vk::StructureType::eRenderingInfo;
    rendering_info.pNext = nullptr;
    rendering_info.flags = {};
    rendering_info.renderArea.offset = vk::Offset2D { 0, 0 };
    rendering_info.renderArea.extent = vk::Extent2D { width, height };
    rendering_info.layerCount = 1;
    rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachments = &color_attachment;
    rendering_info.pDepthAttachment = nullptr;
    rendering_info.pStencilAttachment = nullptr;

    cmd.beginRendering(rendering_info);

    MF_TRACE(Journal::Component::Portal, Journal::Context::Rendering,
        "Began dynamic rendering for window '{}' ({}x{})",
        window->get_create_info().title, width, height);
}

void RenderFlow::end_rendering(
    CommandBufferID cmd_id,
    const std::shared_ptr<Core::Window>& window)
{
    auto cmd = m_shader_foundry->get_command_buffer(cmd_id);
    if (!cmd) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Invalid command buffer ID: {}", cmd_id);
        return;
    }

    cmd.endRendering();

    auto it = m_window_associations.find(window);
    if (it == m_window_associations.end() || !it->second.swapchain_image) {
        MF_RT_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "No swapchain image tracked for window '{}'",
            window->get_create_info().title);
        return;
    }

    vk::ImageMemoryBarrier post_barrier {};
    post_barrier.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
    post_barrier.newLayout = vk::ImageLayout::ePresentSrcKHR;
    post_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    post_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    post_barrier.image = it->second.swapchain_image;
    post_barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    post_barrier.subresourceRange.baseMipLevel = 0;
    post_barrier.subresourceRange.levelCount = 1;
    post_barrier.subresourceRange.baseArrayLayer = 0;
    post_barrier.subresourceRange.layerCount = 1;
    post_barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
    post_barrier.dstAccessMask = vk::AccessFlagBits::eNone;

    cmd.pipelineBarrier(
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::PipelineStageFlagBits::eBottomOfPipe,
        vk::DependencyFlags {},
        0, nullptr,
        0, nullptr,
        1, &post_barrier);

    it->second.swapchain_image = nullptr;

    MF_TRACE(Journal::Component::Portal, Journal::Context::Rendering,
        "Ended dynamic rendering for window '{}'",
        window->get_create_info().title);
}

//==============================================================================
// Command Recording
//==============================================================================

void RenderFlow::bind_pipeline(CommandBufferID cmd_id, RenderPipelineID pipeline_id)
{
    auto pipeline_it = m_pipelines.find(pipeline_id);
    if (pipeline_it == m_pipelines.end()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Invalid pipeline ID: {}", pipeline_id);
        return;
    }

    auto cmd = m_shader_foundry->get_command_buffer(cmd_id);
    if (!cmd) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Invalid command buffer ID: {}", cmd_id);
        return;
    }

    pipeline_it->second.pipeline->bind(cmd);
}

void RenderFlow::bind_vertex_buffers(
    CommandBufferID cmd_id,
    const std::vector<std::shared_ptr<Buffers::VKBuffer>>& buffers,
    uint32_t first_binding)
{
    auto cmd = m_shader_foundry->get_command_buffer(cmd_id);
    if (!cmd) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Invalid command buffer ID: {}", cmd_id);
        return;
    }

    std::vector<vk::Buffer> vk_buffers;
    std::vector<vk::DeviceSize> offsets(buffers.size(), 0);

    vk_buffers.reserve(buffers.size());
    for (const auto& buf : buffers) {
        vk_buffers.push_back(buf->get_buffer());

        /* void* mapped = buf->get_mapped_ptr();
        if (mapped) {
            float* data = reinterpret_cast<float*>(mapped);
            MF_PRINT(Journal::Component::Portal, Journal::Context::Rendering,
                "BIND_VERTEX: All vertex data:");
            for (int v = 0; v < 3; ++v) {
                int offset = v * 6; // 24 bytes / 4 bytes per float = 6 floats per vertex
                MF_PRINT(Journal::Component::Portal, Journal::Context::Rendering,
                    "  Vertex {}: pos=({}, {}, {}), color=({}, {}, {})",
                    v,
                    data[offset], data[offset + 1], data[offset + 2],
                    data[offset + 3], data[offset + 4], data[offset + 5]);
            }
        } else {
            MF_RT_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
                "BIND_VERTEX: Buffer not host-mapped!");
        } */
    }

    cmd.bindVertexBuffers(first_binding, vk_buffers, offsets);
}

void RenderFlow::bind_index_buffer(
    CommandBufferID cmd_id,
    const std::shared_ptr<Buffers::VKBuffer>& buffer,
    vk::IndexType index_type)
{
    auto cmd = m_shader_foundry->get_command_buffer(cmd_id);
    if (!cmd) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Invalid command buffer ID: {}", cmd_id);
        return;
    }

    cmd.bindIndexBuffer(buffer->get_buffer(), 0, index_type);
}

void RenderFlow::bind_descriptor_sets(
    CommandBufferID cmd_id,
    RenderPipelineID pipeline_id,
    const std::vector<DescriptorSetID>& descriptor_sets)
{
    auto pipeline_it = m_pipelines.find(pipeline_id);
    if (pipeline_it == m_pipelines.end()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Invalid pipeline ID: {}", pipeline_id);
        return;
    }

    auto cmd = m_shader_foundry->get_command_buffer(cmd_id);
    if (!cmd) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Invalid command buffer ID: {}", cmd_id);
        return;
    }

    std::vector<vk::DescriptorSet> vk_sets;
    vk_sets.reserve(descriptor_sets.size());
    for (auto ds_id : descriptor_sets) {
        vk_sets.push_back(m_shader_foundry->get_descriptor_set(ds_id));
    }

    cmd.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        pipeline_it->second.layout,
        0,
        vk_sets,
        nullptr);
}

void RenderFlow::push_constants(
    CommandBufferID cmd_id,
    RenderPipelineID pipeline_id,
    const void* data,
    size_t size)
{
    auto pipeline_it = m_pipelines.find(pipeline_id);
    if (pipeline_it == m_pipelines.end()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Invalid pipeline ID: {}", pipeline_id);
        return;
    }

    auto cmd = m_shader_foundry->get_command_buffer(cmd_id);
    if (!cmd) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Invalid command buffer ID: {}", cmd_id);
        return;
    }

    cmd.pushConstants(
        pipeline_it->second.layout,
        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
        0,
        static_cast<uint32_t>(size),
        data);
}

void RenderFlow::draw(
    CommandBufferID cmd_id,
    uint32_t vertex_count,
    uint32_t instance_count,
    uint32_t first_vertex,
    uint32_t first_instance)
{
    auto cmd = m_shader_foundry->get_command_buffer(cmd_id);
    if (!cmd) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Invalid command buffer ID: {}", cmd_id);
        return;
    }

    cmd.draw(vertex_count, instance_count, first_vertex, first_instance);
}

void RenderFlow::draw_indexed(
    CommandBufferID cmd_id,
    uint32_t index_count,
    uint32_t instance_count,
    uint32_t first_index,
    int32_t vertex_offset,
    uint32_t first_instance)
{
    auto cmd = m_shader_foundry->get_command_buffer(cmd_id);
    if (!cmd) {
        return;
    }

    cmd.drawIndexed(index_count, instance_count, first_index,
        vertex_offset, first_instance);
}

//==========================================================================
// Window Rendering Registration
//==========================================================================

void RenderFlow::register_window_for_rendering(const std::shared_ptr<Core::Window>& window)
{
    if (!window) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Cannot register null window");
        return;
    }

    if (!window->is_graphics_registered()) {
        MF_WARN(Journal::Component::Portal, Journal::Context::Rendering,
            "Window '{}' not registered with graphics backend yet.",
            window->get_create_info().title);
    }

    if (!m_window_associations.contains(window)) {
        WindowRenderAssociation association;
        association.window = window;
        m_window_associations[window] = std::move(association);

        MF_INFO(Journal::Component::Portal, Journal::Context::Rendering,
            "Registered window '{}' for dynamic rendering",
            window->get_create_info().title);
    }
}

void RenderFlow::unregister_window(const std::shared_ptr<Core::Window>& window)
{
    if (!window) {
        return;
    }

    auto it = m_window_associations.find(window);

    if (it != m_window_associations.end()) {
        m_window_associations.erase(it);

        MF_DEBUG(Journal::Component::Portal, Journal::Context::Rendering,
            "Unregistered window '{}' from rendering",
            window->get_create_info().title);
    }
}

bool RenderFlow::is_window_registered(const std::shared_ptr<Core::Window>& window) const
{
    return window && m_window_associations.contains(window);
}

std::vector<std::shared_ptr<Core::Window>> RenderFlow::get_registered_windows() const
{
    std::vector<std::shared_ptr<Core::Window>> windows;
    windows.reserve(m_window_associations.size());

    for (const auto& [key, association] : m_window_associations) {
        if (auto window = association.window.lock()) {
            windows.push_back(window);
        }
    }

    return windows;
}

vk::ImageView RenderFlow::get_current_image_view(const std::shared_ptr<Core::Window>& window)
{
    auto view_ptr = m_display_service->get_current_image_view(window);
    if (!view_ptr) {
        return nullptr;
    }
    return *static_cast<vk::ImageView*>(view_ptr);
}

//==============================================================================
// Convenience Methods
//==============================================================================

std::vector<DescriptorSetID> RenderFlow::allocate_pipeline_descriptors(
    RenderPipelineID pipeline_id)
{
    auto pipeline_it = m_pipelines.find(pipeline_id);
    if (pipeline_it == m_pipelines.end()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Invalid pipeline ID: {}", pipeline_id);
        return {};
    }

    std::vector<DescriptorSetID> descriptor_set_ids;
    for (const auto& layout : pipeline_it->second.layouts) {
        auto ds_id = m_shader_foundry->allocate_descriptor_set(layout);
        if (ds_id == INVALID_DESCRIPTOR_SET) {
            MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
                "Failed to allocate descriptor set for pipeline {}", pipeline_id);
            return {};
        }
        descriptor_set_ids.push_back(ds_id);
    }

    MF_DEBUG(Journal::Component::Portal, Journal::Context::Rendering,
        "Allocated {} descriptor sets for pipeline {}",
        descriptor_set_ids.size(), pipeline_id);

    return descriptor_set_ids;
}

} // namespace MayaFlux::Portal::Graphics
