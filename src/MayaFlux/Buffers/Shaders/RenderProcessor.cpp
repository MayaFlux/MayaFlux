#include "RenderProcessor.hpp"

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKGraphicsPipeline.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Portal/Graphics/ShaderFoundry.hpp"
#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/DisplayService.hpp"

namespace MayaFlux::Buffers {

RenderProcessor::RenderProcessor(const ShaderProcessorConfig& config)
    : ShaderProcessor(config)
{
    m_processing_token = ProcessingToken::GRAPHICS_BACKEND;
    m_shader_id = Portal::Graphics::get_shader_foundry().load_shader(config.shader_path, Portal::Graphics::ShaderStage::VERTEX, config.entry_point);
}

void RenderProcessor::set_fragment_shader(const std::string& fragment_path)
{
    auto& foundry = Portal::Graphics::get_shader_foundry();
    m_fragment_shader_id = foundry.load_shader(fragment_path, Portal::Graphics::ShaderStage::FRAGMENT);
    m_needs_pipeline_rebuild = true;
}

void RenderProcessor::set_geometry_shader(const std::string& geometry_path)
{
    auto& foundry = Portal::Graphics::get_shader_foundry();
    m_geometry_shader_id = foundry.load_shader(geometry_path, Portal::Graphics::ShaderStage::GEOMETRY);
    m_needs_pipeline_rebuild = true;
}

void RenderProcessor::set_tess_control_shader(const std::string& tess_control_path)
{
    auto& foundry = Portal::Graphics::get_shader_foundry();
    m_tess_control_shader_id = foundry.load_shader(tess_control_path, Portal::Graphics::ShaderStage::TESS_CONTROL);
    m_needs_pipeline_rebuild = true;
}

void RenderProcessor::set_tess_eval_shader(const std::string& tess_eval_path)
{
    auto& foundry = Portal::Graphics::get_shader_foundry();
    m_tess_eval_shader_id = foundry.load_shader(tess_eval_path, Portal::Graphics::ShaderStage::TESS_EVALUATION);
    m_needs_pipeline_rebuild = true;
}

void RenderProcessor::set_render_pass(Portal::Graphics::RenderPassID render_pass_id)
{
    m_render_pass_id = render_pass_id;
    m_needs_pipeline_rebuild = true;
}

void RenderProcessor::set_target_window(std::shared_ptr<Core::Window> window)
{
    m_target_window = std::move(window);
}

void RenderProcessor::initialize_pipeline(const std::shared_ptr<Buffer>& buffer)
{
    if (m_shader_id == Portal::Graphics::INVALID_SHADER) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Vertex shader not loaded");
        return;
    }

    if (m_fragment_shader_id == Portal::Graphics::INVALID_SHADER) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Fragment shader not loaded");
        return;
    }

    auto& flow = Portal::Graphics::get_render_flow();

    if (!m_target_window) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Target window not set");
        return;
    }

    // vk::Format swapchain_format = vk::Format { m_display_service->get_swapchain_format(m_target_window) };
    // m_render_pass_id = flow.create_simple_render_pass(swapchain_format);

    if (m_render_pass_id == Portal::Graphics::INVALID_RENDER_PASS) {
        vk::Format swapchain_format = vk::Format { m_display_service->get_swapchain_format(m_target_window) };
        m_render_pass_id = flow.create_simple_render_pass(swapchain_format);

        if (m_render_pass_id == Portal::Graphics::INVALID_RENDER_PASS) {
            MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "Render pass not set");
            return;
        }
    }

    Portal::Graphics::get_render_flow().register_window_for_rendering(m_target_window, m_render_pass_id);

    if (!buffer) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Cannot initialize pipeline: buffer is null");
        return;
    }
    auto vk_buffer = std::dynamic_pointer_cast<VKBuffer>(buffer);

    Portal::Graphics::RenderPipelineConfig pipeline_config;
    pipeline_config.vertex_shader = m_shader_id;
    pipeline_config.fragment_shader = m_fragment_shader_id;
    pipeline_config.geometry_shader = m_geometry_shader_id;
    pipeline_config.tess_control_shader = m_tess_control_shader_id;
    pipeline_config.tess_eval_shader = m_tess_eval_shader_id;
    pipeline_config.render_pass = m_render_pass_id;

    pipeline_config.topology = m_primitive_topology;
    pipeline_config.rasterization.polygon_mode = m_polygon_mode;
    pipeline_config.rasterization.cull_mode = m_cull_mode;

    pipeline_config.blend_attachments.emplace_back();

    auto it = m_buffer_info.find(vk_buffer);
    if (it != m_buffer_info.end()) {
        const auto& vertex_info = it->second;
        pipeline_config.semantic_vertex_layout = vertex_info.semantic_layout;
        pipeline_config.use_vertex_shader_reflection = vertex_info.use_reflection;
    }

    m_render_pipeline_id = flow.create_pipeline(pipeline_config);

    if (m_render_pipeline_id == Portal::Graphics::INVALID_RENDER_PIPELINE) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Failed to create render pipeline");
        return;
    }

    m_needs_pipeline_rebuild = false;
}

void RenderProcessor::processing_function(std::shared_ptr<Buffer> buffer)
{
    auto vk_buffer = std::dynamic_pointer_cast<VKBuffer>(buffer);
    if (!vk_buffer || !m_target_window) {
        return;
    }

    if (m_buffer_info.find(vk_buffer) == m_buffer_info.end()) {
        if (vk_buffer->has_vertex_layout()) {
            auto vertex_layout = vk_buffer->get_vertex_layout();
            if (vertex_layout.has_value()) {
                m_buffer_info[vk_buffer] = {
                    .semantic_layout = vertex_layout.value(),
                    .use_reflection = false
                };
            }
        }
    }

    if (m_needs_pipeline_rebuild) {
        initialize_pipeline(buffer);
    }

    if (m_render_pipeline_id == Portal::Graphics::INVALID_RENDER_PIPELINE) {
        return;
    }

    auto vertex_layout = vk_buffer->get_vertex_layout();
    if (!vertex_layout.has_value()) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "VKBuffer has no vertex layout set. Use buffer->set_vertex_layout()");
        return;
    }

    if (vertex_layout->vertex_count == 0) {
        MF_RT_WARN(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Vertex layout has zero vertices, skipping draw");
        return;
    }

    if (vertex_layout->attributes.empty()) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Vertex layout has no attributes");
        return;
    }

    vk_buffer->set_pipeline_window(m_render_pipeline_id, m_target_window);

    auto& foundry = Portal::Graphics::get_shader_foundry();
    auto& flow = Portal::Graphics::get_render_flow();

    auto cmd_id = foundry.begin_commands(Portal::Graphics::ShaderFoundry::CommandBufferType::GRAPHICS);

    flow.begin_render_pass(cmd_id, m_target_window);

    uint32_t width = 0, height = 0;
    m_display_service->get_swapchain_extent(m_target_window, width, height);

    if (width > 0 && height > 0) {
        auto cmd = foundry.get_command_buffer(cmd_id);

        vk::Viewport viewport { 0.0F, 0.0F, static_cast<float>(width), static_cast<float>(height), 0.0F, 1.0F };
        cmd.setViewport(0, 1, &viewport);

        vk::Rect2D scissor { { 0, 0 }, { width, height } };
        cmd.setScissor(0, 1, &scissor);
    }

    flow.bind_pipeline(cmd_id, m_render_pipeline_id);

    flow.bind_vertex_buffers(cmd_id, { vk_buffer });

    flow.draw(cmd_id, vertex_layout->vertex_count);

    flow.end_render_pass(cmd_id);

    vk_buffer->set_pipeline_command(m_render_pipeline_id, cmd_id);
}

void RenderProcessor::on_attach(std::shared_ptr<Buffer> buffer)
{
    ShaderProcessor::on_attach(buffer);

    auto vk_buffer = std::dynamic_pointer_cast<VKBuffer>(buffer);
    if (vk_buffer && vk_buffer->has_vertex_layout()) {
        auto vertex_layout = vk_buffer->get_vertex_layout();
        if (vertex_layout.has_value()) {
            MF_INFO(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "RenderProcessor: Auto-injecting vertex layout "
                "({} vertices, {} attributes)",
                vertex_layout->vertex_count,
                vertex_layout->attributes.size());

            m_needs_pipeline_rebuild = true;
            m_buffer_info[vk_buffer] = { .semantic_layout = vertex_layout.value(), .use_reflection = false };
        }
    }

    if (!m_display_service) {
        m_display_service = Registry::BackendRegistry::instance()
                                .get_service<Registry::Service::DisplayService>();
    }
}

void RenderProcessor::cleanup()
{
    auto& foundry = Portal::Graphics::get_shader_foundry();
    auto& flow = Portal::Graphics::get_render_flow();

    if (m_render_pipeline_id != Portal::Graphics::INVALID_RENDER_PIPELINE) {
        flow.destroy_pipeline(m_render_pipeline_id);
        m_render_pipeline_id = Portal::Graphics::INVALID_RENDER_PIPELINE;
    }

    if (m_render_pass_id != Portal::Graphics::INVALID_RENDER_PASS) {
        flow.destroy_render_pass(m_render_pass_id);
        m_render_pass_id = Portal::Graphics::INVALID_RENDER_PASS;
    }

    if (m_geometry_shader_id != Portal::Graphics::INVALID_SHADER) {
        foundry.destroy_shader(m_geometry_shader_id);
        m_geometry_shader_id = Portal::Graphics::INVALID_SHADER;
    }

    if (m_tess_control_shader_id != Portal::Graphics::INVALID_SHADER) {
        foundry.destroy_shader(m_tess_control_shader_id);
        m_tess_control_shader_id = Portal::Graphics::INVALID_SHADER;
    }

    if (m_tess_eval_shader_id != Portal::Graphics::INVALID_SHADER) {
        foundry.destroy_shader(m_tess_eval_shader_id);
        m_tess_eval_shader_id = Portal::Graphics::INVALID_SHADER;
    }

    if (m_fragment_shader_id != Portal::Graphics::INVALID_SHADER) {
        foundry.destroy_shader(m_fragment_shader_id);
        m_fragment_shader_id = Portal::Graphics::INVALID_SHADER;
    }

    if (m_target_window) {
        flow.unregister_window(m_target_window);
        m_target_window.reset();
    }

    ShaderProcessor::cleanup();

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "RenderProcessor cleanup complete");
}

} // namespace MayaFlux::Buffers
