#include "RenderProcessor.hpp"

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKGraphicsPipeline.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"
#include "MayaFlux/Core/Backends/Windowing/Window.hpp"

#include "MayaFlux/Portal/Graphics/ShaderFoundry.hpp"
#include "MayaFlux/Portal/Graphics/TextureLoom.hpp"
#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/DisplayService.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

RenderProcessor::RenderProcessor(const ShaderConfig& config)
    : ShaderProcessor(config)
{
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

void RenderProcessor::set_target_window(std::shared_ptr<Core::Window> window)
{
    m_target_window = std::move(window);
}

void RenderProcessor::bind_texture(
    uint32_t binding,
    const std::shared_ptr<Core::VKImage>& texture,
    vk::Sampler sampler)
{
    if (!texture) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Cannot bind null texture to binding {}", binding);
        return;
    }

    if (!sampler) {
        auto& loom = Portal::Graphics::get_texture_manager();
        sampler = loom.get_default_sampler();
    }

    m_texture_bindings[binding] = { .texture = texture, .sampler = sampler };

    if (m_pipeline_id != Portal::Graphics::INVALID_RENDER_PIPELINE && !m_descriptor_set_ids.empty()) {

        auto& foundry = Portal::Graphics::get_shader_foundry();
        foundry.update_descriptor_image(
            m_descriptor_set_ids[0],
            binding,
            texture->get_image_view(),
            sampler,
            vk::ImageLayout::eShaderReadOnlyOptimal);
    }

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Bound texture to binding {}", binding);
}

void RenderProcessor::bind_texture(
    const std::string& descriptor_name,
    const std::shared_ptr<Core::VKImage>& texture,
    vk::Sampler sampler)
{
    auto binding_it = m_config.bindings.find(descriptor_name);
    if (binding_it == m_config.bindings.end()) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "No binding configured for descriptor '{}'", descriptor_name);
        return;
    }

    bind_texture(binding_it->second.binding, texture, sampler);
}

void RenderProcessor::initialize_pipeline(const std::shared_ptr<VKBuffer>& buffer)
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

    if (!m_target_window) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Target window not set");
        return;
    }

    auto& flow = Portal::Graphics::get_render_flow();

    Portal::Graphics::get_render_flow().register_window_for_rendering(m_target_window);

    Portal::Graphics::RenderPipelineConfig pipeline_config;
    pipeline_config.vertex_shader = m_shader_id;
    pipeline_config.fragment_shader = m_fragment_shader_id;
    pipeline_config.geometry_shader = m_geometry_shader_id;
    pipeline_config.tess_control_shader = m_tess_control_shader_id;
    pipeline_config.tess_eval_shader = m_tess_eval_shader_id;

    pipeline_config.topology = m_primitive_topology;
    pipeline_config.rasterization.polygon_mode = m_polygon_mode;
    pipeline_config.rasterization.cull_mode = m_cull_mode;

    pipeline_config.blend_attachments.emplace_back();

    if (m_buffer_info.find(buffer) == m_buffer_info.end()) {
        if (buffer->has_vertex_layout()) {
            auto vertex_layout = buffer->get_vertex_layout();
            if (vertex_layout.has_value()) {
                m_buffer_info[buffer] = {
                    .semantic_layout = vertex_layout.value(),
                    .use_reflection = false
                };
            }
        }
    }
    if (m_buffer_info.find(buffer) != m_buffer_info.end()) {
        const auto& vertex_info = m_buffer_info.find(buffer)->second;
        pipeline_config.semantic_vertex_layout = vertex_info.semantic_layout;
        pipeline_config.use_vertex_shader_reflection = vertex_info.use_reflection;
    }

    pipeline_config.push_constant_size = buffer->get_pipeline_context().push_constant_staging.size();

    auto& descriptor_bindings = buffer->get_pipeline_context().descriptor_buffer_bindings;
    std::map<std::pair<uint32_t, uint32_t>, Portal::Graphics::DescriptorBindingInfo> unified_bindings;

    for (const auto& binding : descriptor_bindings) {
        unified_bindings[{ binding.set, binding.binding }] = binding;
    }

    for (const auto& [name, binding] : m_config.bindings) {
        auto key = std::make_pair(binding.set, binding.binding);
        if (unified_bindings.find(key) == unified_bindings.end()) {
            unified_bindings[key] = Portal::Graphics::DescriptorBindingInfo {
                .set = binding.set,
                .binding = binding.binding,
                .type = binding.type,
                .buffer_info = {},
                .name = name
            };
        }
    }

    std::map<uint32_t, std::vector<Portal::Graphics::DescriptorBindingInfo>> bindings_by_set;
    for (const auto& [key, binding] : unified_bindings) {
        bindings_by_set[binding.set].push_back(binding);
    }

    for (const auto& [set_index, set_bindings] : bindings_by_set) {
        pipeline_config.descriptor_sets.push_back(set_bindings);
    }

    vk::Format swapchain_format = static_cast<vk::Format>(
        m_display_service->get_swapchain_format(m_target_window));

    m_pipeline_id = flow.create_pipeline(pipeline_config, { swapchain_format });

    if (m_pipeline_id == Portal::Graphics::INVALID_RENDER_PIPELINE) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Failed to create render pipeline");
        return;
    }

    m_needs_descriptor_rebuild = !pipeline_config.descriptor_sets.empty() && m_descriptor_set_ids.empty();
    m_needs_pipeline_rebuild = false;

    on_pipeline_created(m_pipeline_id);
}

void RenderProcessor::initialize_descriptors(const std::shared_ptr<VKBuffer>& buffer)
{
    if (m_pipeline_id == Portal::Graphics::INVALID_RENDER_PIPELINE) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Cannot allocate descriptor sets without pipeline");
        return;
    }

    on_before_descriptors_create();

    auto& flow = Portal::Graphics::get_render_flow();

    m_descriptor_set_ids = flow.allocate_pipeline_descriptors(m_pipeline_id);

    if (m_descriptor_set_ids.empty()) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Failed to allocate descriptor sets for pipeline");
        return;
    }

    for (const auto& [binding, tex_binding] : m_texture_bindings) {
        auto config_it = std::ranges::find_if(m_config.bindings,
            [binding](const auto& pair) {
                return pair.second.binding == binding;
            });

        if (config_it == m_config.bindings.end()) {
            MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "No config for binding {}", binding);
            continue;
        }
        uint32_t set_index = config_it->second.set;

        if (set_index >= m_descriptor_set_ids.size()) {
            MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "Descriptor set index {} out of range", binding);
            continue;
        }

        auto& foundry = Portal::Graphics::get_shader_foundry();
        foundry.update_descriptor_image(
            m_descriptor_set_ids[set_index],
            config_it->second.binding,
            tex_binding.texture->get_image_view(),
            tex_binding.sampler,
            vk::ImageLayout::eShaderReadOnlyOptimal);
    }

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Allocated {} descriptor sets and updated {} texture bindings",
        m_descriptor_set_ids.size(), m_texture_bindings.size());

    update_descriptors(buffer);
    on_descriptors_created();
}

bool RenderProcessor::on_before_execute(Portal::Graphics::CommandBufferID /*cmd_id*/, const std::shared_ptr<VKBuffer>& /*buffer*/)
{
    if (!m_target_window) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Target window not set");
        return false;
    }
    return m_target_window->is_graphics_registered();
}

void RenderProcessor::execute_shader(const std::shared_ptr<VKBuffer>& buffer)
{
    if (m_buffer_info.find(buffer) == m_buffer_info.end()) {
        if (buffer->has_vertex_layout()) {
            auto vertex_layout = buffer->get_vertex_layout();
            if (vertex_layout.has_value()) {
                m_buffer_info[buffer] = {
                    .semantic_layout = vertex_layout.value(),
                    .use_reflection = false
                };
            }
        }
    }

    if (!m_target_window->is_graphics_registered()) {
        return;
    }

    if (m_pipeline_id == Portal::Graphics::INVALID_RENDER_PIPELINE) {
        return;
    }

    auto vertex_layout = buffer->get_vertex_layout();
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

    buffer->set_pipeline_window(m_pipeline_id, m_target_window);

    auto& foundry = Portal::Graphics::get_shader_foundry();
    auto& flow = Portal::Graphics::get_render_flow();

    vk::Format color_format = static_cast<vk::Format>(
        m_display_service->get_swapchain_format(m_target_window));

    auto cmd_id = foundry.begin_secondary_commands(color_format);
    auto cmd = foundry.get_command_buffer(cmd_id);

    uint32_t width = 0, height = 0;
    m_display_service->get_swapchain_extent(m_target_window, width, height);

    if (width > 0 && height > 0) {
        auto cmd = foundry.get_command_buffer(cmd_id);

        vk::Viewport viewport { 0.0F, 0.0F, static_cast<float>(width), static_cast<float>(height), 0.0F, 1.0F };
        cmd.setViewport(0, 1, &viewport);

        vk::Rect2D scissor { { 0, 0 }, { width, height } };
        cmd.setScissor(0, 1, &scissor);
    }

    flow.bind_pipeline(cmd_id, m_pipeline_id);

    auto& descriptor_bindings = buffer->get_pipeline_context().descriptor_buffer_bindings;
    if (!descriptor_bindings.empty()) {
        for (const auto& binding : descriptor_bindings) {
            if (binding.set >= m_descriptor_set_ids.size()) {
                MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                    "Descriptor set index {} out of range", binding.set);
                continue;
            }

            foundry.update_descriptor_buffer(
                m_descriptor_set_ids[binding.set],
                binding.binding,
                binding.type,
                binding.buffer_info.buffer,
                binding.buffer_info.offset,
                binding.buffer_info.range);
        }
    }

    if (!m_descriptor_set_ids.empty()) {
        flow.bind_descriptor_sets(cmd_id, m_pipeline_id, m_descriptor_set_ids);
    }

    const auto& staging = buffer->get_pipeline_context();
    if (!staging.push_constant_staging.empty()) {
        flow.push_constants(
            cmd_id,
            m_pipeline_id,
            staging.push_constant_staging.data(),
            staging.push_constant_staging.size());
    } else if (!m_push_constant_data.empty()) {
        flow.push_constants(
            cmd_id,
            m_pipeline_id,
            m_push_constant_data.data(),
            m_push_constant_data.size());
    }

    on_before_execute(cmd_id, buffer);

    flow.bind_vertex_buffers(cmd_id, { buffer });

    flow.draw(cmd_id, vertex_layout->vertex_count);

    foundry.end_commands(cmd_id);

    buffer->set_pipeline_command(m_pipeline_id, cmd_id);
    m_target_window->track_frame_command(cmd_id);

    MF_RT_TRACE(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Recorded secondary command buffer {} for window '{}'",
        cmd_id, m_target_window->get_create_info().title);
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

    if (m_pipeline_id != Portal::Graphics::INVALID_RENDER_PIPELINE) {
        flow.destroy_pipeline(m_pipeline_id);
        m_pipeline_id = Portal::Graphics::INVALID_RENDER_PIPELINE;
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
