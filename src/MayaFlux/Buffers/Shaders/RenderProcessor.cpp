#include "RenderProcessor.hpp"

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKGraphicsPipeline.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"
#include "MayaFlux/Core/Backends/Windowing/Window.hpp"

#include "MayaFlux/Portal/Graphics/ShaderFoundry.hpp"
#include "MayaFlux/Portal/Graphics/TextureLoom.hpp"
#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/DisplayService.hpp"

namespace MayaFlux::Buffers {

RenderProcessor::BufferState* RenderProcessor::get_or_cache_buffer_state(const std::shared_ptr<VKBuffer>& buffer)
{
    auto info_it = m_buffer_info.find(buffer);
    if (info_it == m_buffer_info.end()) {
        const auto layout = buffer->get_vertex_layout();
        if (!layout)
            return nullptr;
        info_it = m_buffer_info.try_emplace(buffer).first;
        info_it->second.semantic_layout = *layout;
    }
    return &info_it->second;
}

RenderProcessor::RenderProcessor(const ShaderConfig& config)
    : ShaderProcessor(config)
{
    m_engine_owns_set_zero = true;
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

void RenderProcessor::set_target_window(const std::shared_ptr<Core::Window>& window, const std::shared_ptr<VKBuffer>& buffer)
{
    m_target_window = window;
    window->register_rendering_buffer(buffer);
}

void RenderProcessor::set_visible(bool visible, const std::shared_ptr<VKBuffer>& buffer)
{
    if (!buffer)
        return;

    const auto it = std::ranges::find(m_hidden_buffers, buffer.get());
    if (visible) {
        if (it == m_hidden_buffers.end())
            return;
        m_hidden_buffers.erase(it);
    } else {
        if (it != m_hidden_buffers.end())
            return;
        m_hidden_buffers.push_back(buffer.get());
    }
    buffer->request_presentation_refresh(m_target_window);
}

bool RenderProcessor::is_visible(const std::shared_ptr<VKBuffer>& buffer) const
{
    return std::ranges::find(m_hidden_buffers, buffer.get()) == m_hidden_buffers.end();
}

void RenderProcessor::enable_alpha_blending()
{
    set_blend_attachment(Portal::Graphics::BlendAttachmentConfig::alpha_blend());
}

void RenderProcessor::disable_alpha_blending()
{
    Portal::Graphics::BlendAttachmentConfig opaque;
    opaque.blend_enable = false;
    set_blend_attachment(opaque);
}

void RenderProcessor::set_alpha_blending(bool enabled)
{
    if (enabled) {
        enable_alpha_blending();
    } else {
        disable_alpha_blending();
    }
}

void RenderProcessor::disable_depth_test()
{
    m_depth_stencil.depth_test_enable = false;
    m_depth_stencil.depth_write_enable = false;
    m_needs_pipeline_rebuild = true;
}

void RenderProcessor::enable_depth_test(Portal::Graphics::CompareOp compare_op)
{
    m_depth_stencil.depth_test_enable = true;
    m_depth_stencil.depth_write_enable = true;
    m_depth_stencil.depth_compare_op = compare_op;
    m_depth_enabled = true;
    m_needs_pipeline_rebuild = true;
}

void RenderProcessor::set_view_transform(
    const Kinesis::ViewTransform& vt,
    bool enable_depth,
    Portal::Graphics::CullMode cull)
{
    m_view_transform = vt;
    m_view_transform_source = nullptr;
    m_view_transform_active = true;

    if (enable_depth && !m_depth_enabled) {
        enable_depth_test();
    }

    set_cull_mode(cull);
}

void RenderProcessor::set_view_transform_source(
    std::function<Kinesis::ViewTransform()> fn,
    bool enable_depth,
    Portal::Graphics::CullMode cull)
{
    m_view_transform_source = std::move(fn);
    m_view_transform.reset();
    m_view_transform_active = true;

    if (enable_depth && !m_depth_enabled) {
        enable_depth_test();
    }

    set_cull_mode(cull);
}

void RenderProcessor::bind_texture(
    uint32_t binding,
    const std::shared_ptr<Core::VKImage>& texture,
    vk::Sampler sampler)
{
    if (!texture || !texture->is_initialized() || !texture->get_image_view()) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Cannot bind null texture to binding {}", binding);
        return;
    }

    if (!sampler) {
        auto& loom = Portal::Graphics::get_texture_manager();
        sampler = loom.get_default_sampler();
    }

    m_texture_bindings[binding] = { .texture = texture, .sampler = sampler };
    m_needs_descriptor_rebuild = true;

    if (m_pipeline_id != Portal::Graphics::INVALID_RENDER_PIPELINE && !m_descriptor_set_ids.empty()) {

        auto& foundry = Portal::Graphics::get_shader_foundry();
        auto cfg_it = std::ranges::find_if(m_config.bindings,
            [binding](const auto& pair) {
                return binding >= pair.second.binding
                    && binding < pair.second.binding + pair.second.count;
            });

        if (cfg_it != m_config.bindings.end() && !m_descriptor_set_ids.empty()) {
            const uint32_t cfg_set = cfg_it->second.set;
            if (cfg_set == 0) {
                if (m_view_transform_descriptor_set_id != Portal::Graphics::INVALID_DESCRIPTOR_SET) {
                    uint32_t array_idx = 0;
                    if (cfg_it->second.count > 1 && binding >= cfg_it->second.binding) {
                        array_idx = binding - cfg_it->second.binding;
                    }
                    foundry.update_descriptor_image(
                        m_view_transform_descriptor_set_id,
                        cfg_it->second.binding,
                        texture->get_image_view(),
                        sampler,
                        vk::ImageLayout::eShaderReadOnlyOptimal,
                        array_idx);
                }
            } else {
                auto ds_index = resolve_ds_index(cfg_set);
                if (ds_index && *ds_index < m_descriptor_set_ids.size()) {
                    foundry.update_descriptor_image(
                        m_descriptor_set_ids[*ds_index],
                        cfg_it->second.binding,
                        texture->get_image_view(),
                        sampler,
                        vk::ImageLayout::eShaderReadOnlyOptimal);
                }
            }
        }
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

    pipeline_config.topology = pipeline_topology();
    pipeline_config.rasterization.polygon_mode = m_polygon_mode;
    pipeline_config.rasterization.cull_mode = m_cull_mode;

    if (m_blend_attachment.has_value()) {
        pipeline_config.blend_attachments.push_back(m_blend_attachment.value());
    } else {
        pipeline_config.blend_attachments.emplace_back();
    }

    pipeline_config.depth_stencil = m_depth_stencil;

    const auto* state = get_or_cache_buffer_state(buffer);
    if (!state) {
        MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "initialize_pipeline: layout not yet available, deferring");
        return;
    }

    pipeline_config.semantic_vertex_layout = state->semantic_layout;
    pipeline_config.use_vertex_shader_reflection = state->use_reflection;

    pipeline_config.push_constant_size = resolve_push_constant_size(buffer);

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
                .name = name,
                .count = binding.count
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

    vk::Format depth_format = m_depth_enabled
        ? vk::Format::eD32Sfloat
        : vk::Format::eUndefined;

    m_pipeline_id = flow.create_pipeline(pipeline_config, { swapchain_format }, depth_format);

    if (m_pipeline_id == Portal::Graphics::INVALID_RENDER_PIPELINE) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Failed to create render pipeline");
        return;
    }

    if (m_depth_enabled) {
        buffer->set_needs_depth_attachment(true);
    }

    m_needs_descriptor_rebuild = m_descriptor_set_ids.empty();
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
    auto& foundry = Portal::Graphics::get_shader_foundry();

    auto vt_layout = flow.get_view_transform_layout(m_pipeline_id);
    if (vt_layout) {
        m_view_transform_descriptor_set_id = foundry.allocate_descriptor_set(vt_layout);

        m_view_transform_ubo = std::make_shared<VKBuffer>(
            sizeof(Kinesis::ViewTransform),
            VKBuffer::Usage::UNIFORM,
            Kakshya::DataModality::UNKNOWN);
        ensure_initialized(m_view_transform_ubo);

        foundry.update_descriptor_buffer(
            m_view_transform_descriptor_set_id,
            0,
            vk::DescriptorType::eUniformBuffer,
            m_view_transform_ubo->get_buffer(),
            0,
            sizeof(Kinesis::ViewTransform));

        MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "ViewTransform UBO allocated (pipeline {})", m_pipeline_id);
    }

    m_descriptor_set_ids = flow.allocate_pipeline_descriptors(m_pipeline_id, 1);

    for (const auto& [binding, tex_binding] : m_texture_bindings) {
        if (!tex_binding.texture
            || !tex_binding.texture->is_initialized()
            || !tex_binding.texture->get_image_view()) {
            continue;
        }

        auto config_it = std::ranges::find_if(m_config.bindings,
            [binding](const auto& pair) {
                return binding >= pair.second.binding
                    && binding < pair.second.binding + pair.second.count;
            });

        if (config_it == m_config.bindings.end()) {
            MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "No config for binding {}", binding);
            continue;
        }

        const uint32_t set_index = config_it->second.set;

        if (set_index == 0) {
            if (m_view_transform_descriptor_set_id == Portal::Graphics::INVALID_DESCRIPTOR_SET) {
                MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                    "Engine descriptor set not allocated for set=0 texture binding {}", binding);
                continue;
            }
            foundry.update_descriptor_image(
                m_view_transform_descriptor_set_id,
                config_it->second.binding,
                tex_binding.texture->get_image_view(),
                tex_binding.sampler,
                vk::ImageLayout::eShaderReadOnlyOptimal,
                binding - config_it->second.binding);
            continue;
        }

        auto ds_index = resolve_ds_index(set_index);
        if (!ds_index) {
            MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "Descriptor set index {} out of range", binding);
            continue;
        }

        foundry.update_descriptor_image(
            m_descriptor_set_ids[*ds_index],
            config_it->second.binding,
            tex_binding.texture->get_image_view(),
            tex_binding.sampler,
            vk::ImageLayout::eShaderReadOnlyOptimal,
            binding - config_it->second.binding);
    }

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Allocated {} descriptor sets and updated {} texture bindings",
        m_descriptor_set_ids.size(), m_texture_bindings.size());

    update_descriptors(buffer);
    on_descriptors_created();
}

void RenderProcessor::set_vertex_range(uint32_t first_vertex, uint32_t vertex_count)
{
    m_first_vertex = first_vertex;
    m_vertex_count = vertex_count;

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "RenderProcessor: Set vertex range [offset={}, count={}]",
        first_vertex, vertex_count);
}

void RenderProcessor::set_buffer_vertex_layout(
    const std::shared_ptr<VKBuffer>& buffer,
    const Kakshya::VertexLayout& layout)
{
    auto& state = m_buffer_info[buffer];
    state.semantic_layout = layout;
    state.use_reflection = false;
    m_needs_pipeline_rebuild = true;
}

const Kinesis::ViewTransform& RenderProcessor::publish_view_transform()
{
    Kinesis::ViewTransform vt;
    if (m_view_transform_active) {
        vt = m_view_transform_source
            ? m_view_transform_source()
            : m_view_transform.value_or(Kinesis::ViewTransform {});
    }

    m_published_view_transform = vt;

    if (m_view_transform_ubo && m_view_transform_ubo->get_mapped_ptr()) {
        std::memcpy(
            m_view_transform_ubo->get_mapped_ptr(),
            &vt,
            sizeof(Kinesis::ViewTransform));
    }

    return m_published_view_transform;
}

void RenderProcessor::set_triangulate(bool enabled)
{
    if (m_triangulate == enabled) {
        return;
    }

    m_triangulate = enabled;
    m_needs_pipeline_rebuild = true;
}

vk::Rect2D RenderProcessor::resolve_scissor_rect(uint32_t width, uint32_t height) const noexcept
{
    if (!m_scissor) {
        return { { 0, 0 }, { width, height } };
    }

    const auto& bounds = m_scissor->bounds;
    const auto fw = static_cast<float>(width);
    const auto fh = static_cast<float>(height);

    const float x0 = std::clamp((bounds.min.x * 0.5F + 0.5F) * fw, 0.F, fw);
    const float x1 = std::clamp((bounds.max.x * 0.5F + 0.5F) * fw, 0.F, fw);
    const float y0 = std::clamp((1.F - bounds.max.y) * 0.5F * fh, 0.F, fh);
    const float y1 = std::clamp((1.F - bounds.min.y) * 0.5F * fh, 0.F, fh);

    const auto px = static_cast<int32_t>(x0);
    const auto py = static_cast<int32_t>(y0);
    const auto pw = static_cast<uint32_t>(x1 - x0);
    const auto ph = static_cast<uint32_t>(y1 - y0);

    return { { px, py }, { pw, ph } };
}

void RenderProcessor::set_runs(std::vector<Portal::Graphics::DrawRun> runs)
{
    m_runs = std::move(runs);
}

void RenderProcessor::set_mill_spec(const Portal::Graphics::MillSpec& spec)
{
    m_mill_spec = spec;
    for (auto& [buffer, state] : m_buffer_info) {
        if (state.mill)
            state.mill->set_spec(spec);
    }
}

uint32_t RenderProcessor::milled_vertex_count() const
{
    return m_last_milled_vertex_count;
}

Portal::Graphics::PrimitiveTopology RenderProcessor::pipeline_topology() const
{
    return m_triangulate
        ? Portal::Graphics::PrimitiveTopology::TRIANGLE_LIST
        : m_primitive_topology;
}

uint32_t RenderProcessor::mill_runs(const std::shared_ptr<VKBuffer>& buffer, Portal::Graphics::PrimitiveMill& mill)
{
    std::span<const Portal::Graphics::DrawRun> runs = m_runs;

    Portal::Graphics::DrawRun whole {};
    if (runs.empty()) {
        const auto layout = buffer->get_vertex_layout();
        const uint32_t count = m_vertex_count > 0
            ? m_vertex_count
            : (layout.has_value() ? layout->vertex_count : 0U);

        if (count == 0) {
            return 0;
        }

        whole = { .topology = m_primitive_topology,
            .vertex_offset = m_first_vertex,
            .vertex_count = count };
        runs = { &whole, 1 };
    }

    const Portal::Graphics::MillView view {
        .eye = m_view_transform_active
            ? glm::vec3(glm::inverse(published_view_transform().view)[3])
            : glm::vec3(0.0F, 0.0F, 1.0e4F)
    };

    return mill.mill(buffer, runs, view);
}

void RenderProcessor::prepare_geometry(const std::shared_ptr<VKBuffer>& buffer, BufferState& state)
{
    state.geometry_prepared = false;
    state.first_vertex = m_first_vertex;
    state.vertex_count = 0;
    state.index_count = 0;

    if (m_triangulate) {
        if (!state.mill)
            state.mill = std::make_unique<Portal::Graphics::PrimitiveMill>(m_mill_spec);
        state.vertex_count = mill_runs(buffer, *state.mill);
        m_last_milled_vertex_count = state.vertex_count;
        state.first_vertex = 0;
        if (state.vertex_count > 0) {
            state.draw_source = state.mill->output();
        } else if (state.draw_source != buffer) {
            state.draw_source = buffer;
        }
    } else {
        if (state.draw_source != buffer)
            state.draw_source = buffer;
        if (m_vertex_count > 0) {
            state.vertex_count = m_vertex_count;
        } else {
            const auto layout = buffer->get_vertex_layout();
            state.vertex_count = layout ? layout->vertex_count : 0U;
        }
    }

    if (state.vertex_count > 0 && state.draw_source->has_index_buffer())
        state.index_count = static_cast<uint32_t>(state.draw_source->get_index_buffer_size() / sizeof(uint32_t));
    state.geometry_prepared = true;
}

bool RenderProcessor::on_before_execute(Portal::Graphics::CommandBufferID /*cmd_id*/, const std::shared_ptr<VKBuffer>& buffer)
{
    if (!is_visible(buffer))
        return false;

    if (!m_target_window) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Target window not set");
        return false;
    }
    return m_target_window->is_graphics_registered();
}

void RenderProcessor::execute_shader(const std::shared_ptr<VKBuffer>& buffer)
{
    if (!is_visible(buffer) || !m_target_window
        || !m_target_window->is_graphics_registered()) {
        return;
    }

    if (m_pipeline_id == Portal::Graphics::INVALID_RENDER_PIPELINE) {
        return;
    }

    auto* state = get_or_cache_buffer_state(buffer);
    if (!state) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "VKBuffer has no vertex layout set. Use buffer->set_vertex_layout()");
        return;
    }

    publish_view_transform();
    prepare_geometry(buffer, *state);
    record_draw(buffer, *state);
}

void RenderProcessor::record_draw(const std::shared_ptr<VKBuffer>& buffer)
{
    if (!m_initialized || !is_visible(buffer) || !m_target_window
        || !m_target_window->is_graphics_registered()
        || m_pipeline_id == Portal::Graphics::INVALID_RENDER_PIPELINE)
        return;

    const auto it = m_buffer_info.find(buffer);
    if (it == m_buffer_info.end() || !it->second.geometry_prepared)
        return;
    record_draw(buffer, it->second);
}

void RenderProcessor::record_draw(const std::shared_ptr<VKBuffer>& buffer, const BufferState& state)
{
    buffer->set_pipeline_window(m_pipeline_id, m_target_window);

    auto& foundry = Portal::Graphics::get_shader_foundry();
    auto& flow = Portal::Graphics::get_render_flow();

    vk::Format color_format = static_cast<vk::Format>(
        m_display_service->get_swapchain_format(m_target_window));

    vk::Format depth_format = m_depth_enabled
        ? vk::Format::eD32Sfloat
        : vk::Format::eUndefined;

    auto cmd_id = foundry.begin_secondary_commands(color_format, depth_format);
    auto cmd = foundry.get_command_buffer(cmd_id);

    uint32_t width = 0, height = 0;
    m_display_service->get_swapchain_extent(m_target_window, width, height);

    if (width > 0 && height > 0) {
        auto cmd = foundry.get_command_buffer(cmd_id);

        vk::Viewport viewport {
            0.0F,
            static_cast<float>(height),
            static_cast<float>(width),
            -static_cast<float>(height),
            0.0F,
            1.0F
        };
        cmd.setViewport(0, 1, &viewport);

        const vk::Rect2D scissor = resolve_scissor_rect(width, height);
        cmd.setScissor(0, 1, &scissor);
    }

    flow.bind_pipeline(cmd_id, m_pipeline_id);

    auto& engine_bindings = buffer->get_engine_context().ssbo_bindings;
    for (const auto& binding : engine_bindings) {
        if (binding.set == 0 && binding.binding == 0) {
            MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "Engine SSBO at binding=0 is reserved for ViewTransform UBO");
            continue;
        }
        if (m_view_transform_descriptor_set_id == Portal::Graphics::INVALID_DESCRIPTOR_SET) {
            MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "Engine SSBO binding {} skipped: engine descriptor set not allocated", binding.binding);
            continue;
        }
        foundry.update_descriptor_buffer(
            m_view_transform_descriptor_set_id,
            binding.binding,
            binding.type,
            binding.buffer_info.buffer,
            binding.buffer_info.offset,
            binding.buffer_info.range);
    }

    auto& descriptor_bindings = buffer->get_pipeline_context().descriptor_buffer_bindings;
    if (!descriptor_bindings.empty()) {
        for (const auto& binding : descriptor_bindings) {
            auto ds_index = resolve_ds_index(binding.set);
            if (!ds_index) {
                MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                    "Descriptor set index {} out of range or reserved", binding.set);
                continue;
            }

            foundry.update_descriptor_buffer(
                m_descriptor_set_ids[*ds_index],
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

    if (m_view_transform_descriptor_set_id != Portal::Graphics::INVALID_DESCRIPTOR_SET) {
        flow.bind_descriptor_sets(
            cmd_id, m_pipeline_id,
            { m_view_transform_descriptor_set_id },
            0);
    }

    if (!m_descriptor_set_ids.empty()) {
        flow.bind_descriptor_sets(
            cmd_id, m_pipeline_id,
            m_descriptor_set_ids,
            1);
    }

    const auto push_data = resolve_push_constants(buffer);
    if (!push_data.empty()) {
        flow.push_constants(
            cmd_id, m_pipeline_id, push_data.data(), push_data.size());
    }

    on_before_execute(cmd_id, buffer);

    flow.bind_vertex_buffers(cmd_id, { state.draw_source });

    if (state.vertex_count > 0) {
        if (state.draw_source->has_index_buffer()) {
            flow.bind_index_buffer(cmd_id, state.draw_source);
            flow.draw_indexed(cmd_id, state.index_count, m_instance_count, 0, 0, 0);
        } else {
            flow.draw(cmd_id, state.vertex_count, m_instance_count, state.first_vertex, 0);
        }
    }

    foundry.end_commands(cmd_id);

    buffer->set_pipeline_command(m_pipeline_id, cmd_id);
    m_target_window->track_frame_command(cmd_id);

    MF_RT_TRACE(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Recorded secondary command buffer {} for window '{}'",
        cmd_id, m_target_window->get_create_info().title);
}

void RenderProcessor::on_attach(const std::shared_ptr<Buffer>& buffer)
{
    auto vk_buffer = std::dynamic_pointer_cast<VKBuffer>(buffer);
    if (!vk_buffer)
        return;

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "RenderProcessor attached to VKBuffer (size: {} bytes, modality: {})",
        vk_buffer->get_size_bytes(),
        static_cast<int>(vk_buffer->get_modality()));

    if (vk_buffer && vk_buffer->has_vertex_layout()) {
        auto vertex_layout = vk_buffer->get_vertex_layout();
        if (vertex_layout.has_value()) {
            MF_INFO(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "RenderProcessor: Auto-injecting vertex layout "
                "({} vertices, {} attributes)",
                vertex_layout->vertex_count,
                vertex_layout->attributes.size());

            m_needs_pipeline_rebuild = true;
            auto& state = m_buffer_info[vk_buffer];
            state.semantic_layout = vertex_layout.value();
            state.use_reflection = false;
        }
    }

    if (m_depth_enabled) {
        vk_buffer->set_needs_depth_attachment(true);
    }

    if (!m_display_service) {
        m_display_service = Registry::BackendRegistry::instance()
                                .get_service<Registry::Service::DisplayService>();
    }
}

void RenderProcessor::on_detach(const std::shared_ptr<Buffer>& buffer)
{
    auto vk_buffer = std::dynamic_pointer_cast<VKBuffer>(buffer);
    if (!vk_buffer)
        return;

    vk_buffer->remove_pipeline_window(m_pipeline_id);

    if (m_target_window) {
        bool has_other_pipeline = false;
        for (const auto& [id, window] : vk_buffer->get_render_pipelines()) {
            if (window == m_target_window) {
                has_other_pipeline = true;
                break;
            }
        }

        if (!has_other_pipeline) {
            auto primary = vk_buffer->get_render_processor();
            has_other_pipeline = primary && primary.get() != this
                && primary->get_target_window() == m_target_window;

            if (!has_other_pipeline) {
                for (const auto& render : vk_buffer->get_additional_render_processors()) {
                    if (render.get() != this && render->get_target_window() == m_target_window) {
                        has_other_pipeline = true;
                        break;
                    }
                }
            }
        }

        if (!has_other_pipeline)
            m_target_window->unregister_rendering_buffer(vk_buffer);
    }

    std::erase(m_hidden_buffers, vk_buffer.get());
    m_buffer_info.erase(vk_buffer);
    ShaderProcessor::on_detach(buffer);
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

    for (auto& [buffer, state] : m_buffer_info) {
        state.geometry_prepared = false;
        state.draw_source.reset();
        state.mill.reset();
        state.first_vertex = 0;
        state.vertex_count = 0;
        state.index_count = 0;
    }
    m_last_milled_vertex_count = 0;
    m_runs.clear();

    m_view_transform_ubo.reset();
    m_view_transform_descriptor_set_id = Portal::Graphics::INVALID_DESCRIPTOR_SET;
    m_view_transform_active = false;

    if (m_target_window) {
        flow.unregister_window(m_target_window);
        m_target_window.reset();
    }

    ShaderProcessor::cleanup();

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "RenderProcessor cleanup complete");
}

} // namespace MayaFlux::Buffers
