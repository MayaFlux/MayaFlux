#include "RenderProcessor.hpp"

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKGraphicsPipeline.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"
#include "MayaFlux/Core/Backends/Windowing/Window.hpp"

#include "MayaFlux/Portal/Graphics/ImageTarget.hpp"
#include "MayaFlux/Portal/Graphics/ShaderFoundry.hpp"
#include "MayaFlux/Portal/Graphics/SamplerForge.hpp"
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
    m_set_zero_is_push = true;
    m_lanes.push_back(std::make_unique<Lane>());
    m_lane = m_lanes.front().get();
    m_shader_id = Portal::Graphics::get_shader_foundry().load_shader(config.shader_path, Portal::Graphics::ShaderStage::VERTEX, config.entry_point);
}

void RenderProcessor::set_fragment_shader(const std::string& fragment_path)
{
    auto& foundry = Portal::Graphics::get_shader_foundry();
    m_fragment_shader_id = foundry.load_shader(fragment_path, Portal::Graphics::ShaderStage::FRAGMENT);
    invalidate_pipelines();
}

void RenderProcessor::set_geometry_shader(const std::string& geometry_path)
{
    auto& foundry = Portal::Graphics::get_shader_foundry();
    m_geometry_shader_id = foundry.load_shader(geometry_path, Portal::Graphics::ShaderStage::GEOMETRY);
    invalidate_pipelines();
}

void RenderProcessor::set_tess_control_shader(const std::string& tess_control_path)
{
    auto& foundry = Portal::Graphics::get_shader_foundry();
    m_tess_control_shader_id = foundry.load_shader(tess_control_path, Portal::Graphics::ShaderStage::TESS_CONTROL);
    invalidate_pipelines();
}

void RenderProcessor::set_tess_eval_shader(const std::string& tess_eval_path)
{
    auto& foundry = Portal::Graphics::get_shader_foundry();
    m_tess_eval_shader_id = foundry.load_shader(tess_eval_path, Portal::Graphics::ShaderStage::TESS_EVALUATION);
    invalidate_pipelines();
}

RenderProcessor::ActiveLane::ActiveLane(RenderProcessor& owner, Lane& lane)
    : m_owner(owner)
    , m_previous(owner.m_lane)
    , m_swapped(&lane != owner.m_lanes.front().get())
{
    if (m_swapped) {
        m_owner.exchange_shader_state(lane);
    }
    m_owner.m_lane = &lane;
}

RenderProcessor::ActiveLane::~ActiveLane()
{
    if (m_swapped) {
        m_owner.exchange_shader_state(*m_owner.m_lane);
    }
    m_owner.m_lane = m_previous;
}

void RenderProcessor::exchange_shader_state(Lane& lane)
{
    std::swap(m_descriptor_set_ids, lane.descriptor_sets);
    std::swap(m_created_layout, lane.created_layout);
    std::swap(m_initialized, lane.initialized);
    std::swap(m_descriptors_ready, lane.descriptors_ready);
    std::swap(m_needs_pipeline_rebuild, lane.needs_pipeline);
    std::swap(m_needs_descriptor_rebuild, lane.needs_descriptors);
}

void RenderProcessor::clear_destinations(const std::shared_ptr<VKBuffer>& buffer)
{
    for (const auto& image : get_target_images()) {
        remove_target_image(image, buffer);
    }
    while (const auto window = get_target_window()) {
        remove_target_window(window, buffer);
    }
}

RenderProcessor::Lane& RenderProcessor::claim_lane()
{
    auto& primary = *m_lanes.front();
    if (!primary.window && !primary.image) {
        m_needs_pipeline_rebuild = true;
        m_needs_descriptor_rebuild = true;
        return primary;
    }

    auto lane = std::make_unique<Lane>();
    lane->initialized = true;
    m_lanes.push_back(std::move(lane));
    return *m_lanes.back();
}

void RenderProcessor::release_window(
    const std::shared_ptr<Core::Window>& window,
    const std::shared_ptr<VKBuffer>& buffer)
{
    for (const auto& [id, other] : buffer->get_render_pipelines()) {
        if (other == window) {
            return;
        }
    }

    const auto uses = [this, &window](const std::shared_ptr<RenderProcessor>& render) {
        return render && render.get() != this && render->find_lane(window) != nullptr;
    };

    if (uses(buffer->get_render_processor())) {
        return;
    }
    for (const auto& render : buffer->get_additional_render_processors()) {
        if (uses(render)) {
            return;
        }
    }

    window->unregister_rendering_buffer(buffer);
}

void RenderProcessor::drop_lane(Lane& lane, const std::shared_ptr<VKBuffer>& buffer)
{
    const auto window = lane.window;
    const auto image = lane.image;
    const bool primary = &lane == m_lanes.front().get();

    {
        ActiveLane scope(*this, lane);
        retire_current_pipeline();
    }

    if (window && buffer) {
        buffer->request_presentation_refresh(window);
        release_window(window, buffer);
    }
    if (image) {
        image->request_clear();
    }

    if (!primary) {
        std::erase_if(m_lanes, [&lane](const auto& held) { return held.get() == &lane; });
        return;
    }

    if (m_lanes.size() > 1) {
        m_lanes.erase(m_lanes.begin());
        m_lane = m_lanes.front().get();
        exchange_shader_state(*m_lane);
        return;
    }

    lane.window.reset();
    lane.image.reset();
    lane.view.reset();
    lane.view_source = nullptr;
    lane.enabled = true;
    m_descriptor_set_ids.clear();
    m_descriptors_ready = false;
    m_created_layout.reset();
    m_needs_pipeline_rebuild = true;
    m_needs_descriptor_rebuild = true;
}

void RenderProcessor::set_target_window(const std::shared_ptr<Core::Window>& window, const std::shared_ptr<VKBuffer>& buffer)
{
    if (!window) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "set_target_window: window is null");
        return;
    }

    if (m_lanes.size() == 1 && m_lanes.front()->window == window) {
        window->register_rendering_buffer(buffer);
        return;
    }

    clear_destinations(buffer);
    m_lane->window = window;
    window->register_rendering_buffer(buffer);
}

void RenderProcessor::set_target_image(
    const std::shared_ptr<Portal::Graphics::ImageTarget>& image,
    const std::shared_ptr<VKBuffer>& buffer,
    const Portal::Graphics::TargetArea& area)
{
    if (!image) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "set_target_image: image target is null");
        return;
    }

    if (area.layer >= image->layers()) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "set_target_image: layer {} of a target with {}", area.layer, image->layers());
        return;
    }

    if (m_lanes.size() == 1 && m_lanes.front()->image == image) {
        const bool moved = !(m_lanes.front()->area == area);
        m_lanes.front()->area = area;
        if (moved) {
            image->request_clear();
        } else {
            image->request_redraw();
        }
        return;
    }

    clear_destinations(buffer);
    m_lane->image = image;
    m_lane->area = area;
    image->request_clear();
}

void RenderProcessor::set_destinations(
    const std::shared_ptr<Core::Window>& window,
    const std::vector<std::shared_ptr<Portal::Graphics::ImageTarget>>& images,
    const std::shared_ptr<VKBuffer>& buffer)
{
    if (window) {
        set_target_window(window, buffer);
        for (const auto& image : images) {
            add_target_image(image, buffer);
        }
        return;
    }

    if (images.empty()) {
        set_target_window(window, buffer);
        return;
    }

    set_target_image(images.front(), buffer);
    for (size_t i = 1; i < images.size(); ++i) {
        add_target_image(images[i], buffer);
    }
}

void RenderProcessor::add_target_window(const std::shared_ptr<Core::Window>& window, const std::shared_ptr<VKBuffer>& buffer)
{
    if (!window) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "add_target_window: window is null");
        return;
    }

    if (!find_lane(window)) {
        claim_lane().window = window;
    }
    window->register_rendering_buffer(buffer);
}

void RenderProcessor::add_target_image(
    const std::shared_ptr<Portal::Graphics::ImageTarget>& image,
    const std::shared_ptr<VKBuffer>& /*buffer*/,
    const Portal::Graphics::TargetArea& area)
{
    if (!image) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "add_target_image: image target is null");
        return;
    }

    if (area.layer >= image->layers()) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "add_target_image: layer {} of a target with {}", area.layer, image->layers());
        return;
    }

    if (auto* existing = find_lane(image)) {
        existing->area = area;
    } else {
        auto& lane = claim_lane();
        lane.image = image;
        lane.area = area;
    }
    image->request_clear();
}

void RenderProcessor::set_target_area(
    const std::shared_ptr<Portal::Graphics::ImageTarget>& image,
    const Portal::Graphics::TargetArea& area)
{
    auto* lane = find_lane(image);
    if (!lane || area.layer >= image->layers()) {
        return;
    }
    lane->area = area;
    image->request_clear();
}

Portal::Graphics::TargetArea RenderProcessor::get_target_area(
    const std::shared_ptr<Portal::Graphics::ImageTarget>& image) const
{
    const auto* lane = find_lane(image);
    return lane ? lane->area : Portal::Graphics::TargetArea {};
}

void RenderProcessor::remove_target_window(const std::shared_ptr<Core::Window>& window, const std::shared_ptr<VKBuffer>& buffer)
{
    if (auto* lane = find_lane(window)) {
        drop_lane(*lane, buffer);
    }
}

void RenderProcessor::remove_target_image(
    const std::shared_ptr<Portal::Graphics::ImageTarget>& image,
    const std::shared_ptr<VKBuffer>& buffer)
{
    if (auto* lane = find_lane(image)) {
        drop_lane(*lane, buffer);
    }
}

void RenderProcessor::set_window_enabled(bool enabled)
{
    for (const auto& lane : m_lanes) {
        if (lane->window && lane->enabled != enabled) {
            lane->enabled = enabled;
            for (const auto& entry : m_buffer_info) {
                request_refresh(entry.first);
            }
        }
    }
}

void RenderProcessor::set_target_enabled(const std::shared_ptr<Portal::Graphics::ImageTarget>& image, bool enabled)
{
    if (auto* lane = find_lane(image); lane && lane->enabled != enabled) {
        lane->enabled = enabled;
        for (const auto& entry : m_buffer_info) {
            request_refresh(entry.first);
        }
    }
}

std::shared_ptr<Core::Window> RenderProcessor::get_target_window() const
{
    for (const auto& lane : m_lanes) {
        if (lane->window) {
            return lane->window;
        }
    }
    return nullptr;
}

std::shared_ptr<Portal::Graphics::ImageTarget> RenderProcessor::get_target_image() const
{
    for (const auto& lane : m_lanes) {
        if (lane->image) {
            return lane->image;
        }
    }
    return nullptr;
}

std::vector<std::shared_ptr<Portal::Graphics::ImageTarget>> RenderProcessor::get_target_images() const
{
    std::vector<std::shared_ptr<Portal::Graphics::ImageTarget>> images;
    for (const auto& lane : m_lanes) {
        if (lane->image) {
            images.push_back(lane->image);
        }
    }
    return images;
}

bool RenderProcessor::owns_pipeline(Portal::Graphics::RenderPipelineID id) const
{
    return id != Portal::Graphics::INVALID_RENDER_PIPELINE
        && std::ranges::any_of(m_lanes, [id](const auto& lane) { return lane->pipeline == id; });
}

void RenderProcessor::invalidate_pipelines()
{
    m_needs_pipeline_rebuild = true;
    for (size_t i = 1; i < m_lanes.size(); ++i) {
        m_lanes[i]->needs_pipeline = true;
    }
}

bool RenderProcessor::destination_ready(const Lane& lane) const
{
    if (!lane.enabled) {
        return false;
    }
    if (lane.window) {
        return lane.window->is_graphics_registered();
    }
    return lane.image && lane.image->active();
}

RenderProcessor::DestinationInfo RenderProcessor::describe() const
{
    DestinationInfo info;

    if (m_lane->image) {
        info.colors = m_lane->image->color_formats();
        info.width = m_lane->image->width();
        info.height = m_lane->image->height();
        info.depth = m_depth_enabled ? m_lane->image->depth_format() : vk::Format::eUndefined;
        return info;
    }

    info.colors = { vk::Format {} };
    if (m_lane->window && m_display_service) {
        info.colors.front() = static_cast<vk::Format>(m_display_service->get_swapchain_format(m_lane->window));
        m_display_service->get_swapchain_extent(m_lane->window, info.width, info.height);
    }
    info.depth = m_depth_enabled ? vk::Format::eD32Sfloat : vk::Format::eUndefined;
    return info;
}

std::string RenderProcessor::destination_name() const
{
    if (m_lane->window) {
        return "window '" + m_lane->window->get_create_info().title + "'";
    }
    return "an image target";
}

void RenderProcessor::request_refresh(const std::shared_ptr<VKBuffer>& buffer) const
{
    for (const auto& lane : m_lanes) {
        if (lane->image) {
            lane->image->request_clear();
        }
        if (lane->window) {
            buffer->request_presentation_refresh(lane->window);
        }
    }
}

void RenderProcessor::register_pipeline(const std::shared_ptr<VKBuffer>& buffer) const
{
    if (m_lane->image) {
        buffer->set_pipeline_image(m_lane->pipeline, m_lane->image, m_lane->area.layer);
        return;
    }
    buffer->set_pipeline_window(m_lane->pipeline, m_lane->window);
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
    request_refresh(buffer);
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
    auto next = m_depth_stencil;
    next.depth_test_enable = false;
    next.depth_write_enable = false;

    if (next == m_depth_stencil) {
        return;
    }

    m_depth_stencil = next;
    invalidate_pipelines();
}

void RenderProcessor::enable_depth_test(Portal::Graphics::CompareOp compare_op)
{
    auto next = m_depth_stencil;
    next.depth_test_enable = true;
    next.depth_write_enable = true;
    next.depth_compare_op = compare_op;

    const bool was_enabled = m_depth_enabled;
    m_depth_enabled = true;

    if (next == m_depth_stencil && was_enabled) {
        return;
    }

    m_depth_stencil = next;
    invalidate_pipelines();
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

RenderProcessor::Lane* RenderProcessor::find_view_lane(const DestinationView& destination) const
{
    if (destination.window && destination.target) {
        return nullptr;
    }
    if (destination.window) {
        return find_lane(destination.window);
    }
    if (destination.target) {
        return find_lane(destination.target);
    }
    return nullptr;
}

bool RenderProcessor::set_destination_view(const DestinationView& view)
{
    if (view.transform.has_value() == static_cast<bool>(view.source)) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "set_destination_view: give either a transform or a source");
        return false;
    }

    auto* lane = find_view_lane(view);
    if (!lane) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "set_destination_view: name one window or image target this processor draws into");
        return false;
    }

    lane->view = view.transform;
    lane->view_source = view.source;

    if (view.enable_depth && !m_depth_enabled) {
        enable_depth_test();
    }
    set_cull_mode(view.cull);

    if (lane->image) {
        lane->image->request_redraw();
    }
    return true;
}

void RenderProcessor::clear_destination_view(const DestinationView& destination)
{
    auto* lane = find_view_lane(destination);
    if (!lane || !has_view(*lane)) {
        return;
    }
    lane->view.reset();
    lane->view_source = nullptr;
    if (lane->image) {
        lane->image->request_redraw();
    }
}

bool RenderProcessor::has_destination_view(const DestinationView& destination) const
{
    const auto* lane = find_view_lane(destination);
    return lane && has_view(*lane);
}

void RenderProcessor::apply_lane_view()
{
    if (!has_view(*m_lane)) {
        return;
    }

    if (!m_lane->view_source) {
        m_published_render_transform.view_transform = *m_lane->view;
        return;
    }

    const DestinationInfo destination = describe();
    const vk::Rect2D filled = resolve_area_rect(m_lane->area, destination.width, destination.height);
    m_published_render_transform.view_transform = m_lane->view_source(
        glm::uvec2 { filled.extent.width, filled.extent.height });
}

void RenderProcessor::set_geometry_transform(const glm::mat4& transform)
{
    m_geometry_transform = transform;
    m_geometry_transform_source = nullptr;
}

void RenderProcessor::set_geometry_transform_source(std::function<glm::mat4()> fn)
{
    m_geometry_transform_source = std::move(fn);
    m_geometry_transform.reset();
}

void RenderProcessor::bind_image(uint32_t binding, const std::shared_ptr<Core::VKImage>& texture)
{
    if (!texture || !texture->is_initialized() || !texture->get_image_view()) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Cannot bind null texture to binding {}", binding);
        return;
    }

    const auto chosen = m_binding_samplers.find(binding);
    const vk::Sampler sampler = chosen != m_binding_samplers.end()
        ? chosen->second
        : Portal::Graphics::SamplerForge::instance().get_or_create(
              { .enable_mipmaps = true, .border_color = Portal::Graphics::BorderColor::OPAQUE_BLACK });

    m_texture_bindings[binding] = { .texture = texture, .sampler = sampler };
    m_target_bindings.erase(binding);
    m_needs_descriptor_rebuild = true;

    if (m_lane->pipeline != Portal::Graphics::INVALID_RENDER_PIPELINE && !m_descriptor_set_ids.empty()) {

        auto& foundry = Portal::Graphics::get_shader_foundry();
        auto cfg_it = std::ranges::find_if(m_config.bindings,
            [binding](const auto& pair) {
                return binding >= pair.second.binding
                    && binding < pair.second.binding + pair.second.count;
            });

        if (cfg_it != m_config.bindings.end() && !m_descriptor_set_ids.empty()) {
            const uint32_t cfg_set = cfg_it->second.set;
            if (cfg_set != 0) {
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

bool RenderProcessor::samples(const std::shared_ptr<Portal::Graphics::ImageTarget>& target) const
{
    if (!target) {
        return false;
    }

    const bool by_target = std::ranges::any_of(m_target_bindings,
        [&target](const auto& entry) { return entry.second == target; });
    if (by_target) {
        return true;
    }

    return std::ranges::any_of(m_texture_bindings, [&target](const auto& entry) {
        const auto& bound = entry.second.texture;
        if (bound == target->depth_image()) {
            return static_cast<bool>(bound);
        }
        for (size_t index = 0; index < target->attachment_count(); ++index) {
            if (bound == target->image(index)) {
                return true;
            }
        }
        return false;
    });
}

void RenderProcessor::follow_target_images()
{
    std::vector<std::pair<uint32_t, std::shared_ptr<Portal::Graphics::ImageTarget>>> stale;
    for (const auto& [binding, target] : m_target_bindings) {
        const auto bound = m_texture_bindings.find(binding);
        if (bound == m_texture_bindings.end() || bound->second.texture != target->image()) {
            stale.emplace_back(binding, target);
        }
    }

    for (const auto& [binding, target] : stale) {
        bind_image(binding, target->image());
        m_target_bindings[binding] = target;
    }
}

void RenderProcessor::bind_texture(const Portal::Graphics::SampledTexture& texture)
{
    if (texture.target && texture.image) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Cannot bind both an image and an image target to '{}'", texture.name);
        return;
    }

    uint32_t binding {};
    if (texture.binding) {
        binding = *texture.binding;
    } else {
        const auto binding_it = m_config.bindings.find(texture.name);
        if (binding_it == m_config.bindings.end()) {
            MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "No binding configured for descriptor '{}'", texture.name);
            return;
        }
        binding = binding_it->second.binding;
    }

    if (texture.sampler) {
        set_binding_sampler(binding, *texture.sampler);
    }

    if (texture.target) {
        bind_image(binding, texture.target->image());
        m_target_bindings[binding] = texture.target;
        return;
    }
    bind_image(binding, texture.image);
}

void RenderProcessor::set_binding_sampler(uint32_t binding, const Portal::Graphics::SamplerConfig& config)
{
    const auto sampler = Portal::Graphics::SamplerForge::instance().get_or_create(config);
    if (!sampler) {
        return;
    }

    m_binding_samplers[binding] = sampler;

    const auto bound = m_texture_bindings.find(binding);
    if (bound != m_texture_bindings.end()) {
        bound->second.sampler = sampler;
        m_needs_descriptor_rebuild = true;
    }
}

void RenderProcessor::set_binding_sampler(const std::string& descriptor_name, const Portal::Graphics::SamplerConfig& config)
{
    const auto binding_it = m_config.bindings.find(descriptor_name);
    if (binding_it == m_config.bindings.end()) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "No binding configured for descriptor '{}'", descriptor_name);
        return;
    }

    set_binding_sampler(binding_it->second.binding, config);
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

    if (!has_destination()) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Target window or image not set");
        return;
    }

    if (!destination_ready(*m_lane)) {
        m_needs_pipeline_rebuild = true;
        return;
    }

    const auto* state = get_or_cache_buffer_state(buffer);
    if (!state) {
        MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "initialize_pipeline: layout not yet available, deferring");
        m_needs_pipeline_rebuild = true;
        return;
    }

    auto& flow = Portal::Graphics::get_render_flow();

    if (m_lane->window) {
        flow.register_window_for_rendering(m_lane->window);
    }

    Portal::Graphics::RenderPipelineConfig pipeline_config;
    pipeline_config.vertex_shader = m_shader_id;
    pipeline_config.fragment_shader = m_fragment_shader_id;
    pipeline_config.geometry_shader = m_triangulate
        ? Portal::Graphics::INVALID_SHADER : m_geometry_shader_id;
    pipeline_config.tess_control_shader = m_tess_control_shader_id;
    pipeline_config.tess_eval_shader = m_tess_eval_shader_id;

    pipeline_config.topology = pipeline_topology();
    const bool fill_rasterization = m_triangulate
        || m_primitive_topology == Portal::Graphics::PrimitiveTopology::POINT_LIST;
    pipeline_config.rasterization.polygon_mode = fill_rasterization
        ? Portal::Graphics::PolygonMode::FILL
        : m_polygon_mode;
    pipeline_config.rasterization.cull_mode = m_cull_mode;

    if (m_blend_attachment.has_value()) {
        pipeline_config.blend_attachments.push_back(m_blend_attachment.value());
    } else {
        pipeline_config.blend_attachments.emplace_back();
    }
    const size_t attachment_count = describe().colors.size();
    for (size_t extra = 1; extra < attachment_count; ++extra) {
        pipeline_config.blend_attachments.emplace_back();
    }

    pipeline_config.depth_stencil = m_depth_stencil;

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

    std::vector<Portal::Graphics::DescriptorBindingInfo> unregistered;
    for (const Portal::Graphics::ShaderID shader_id : { m_shader_id, m_fragment_shader_id, m_geometry_shader_id, m_tess_control_shader_id, m_tess_eval_shader_id }) {
        if (shader_id == Portal::Graphics::INVALID_SHADER) {
            continue;
        }
        for (const auto& used : Portal::Graphics::get_shader_foundry().get_active_descriptor_bindings(shader_id)) {
            if (used.set == 0 || unified_bindings.contains({ used.set, used.binding })) {
                continue;
            }
            unregistered.push_back(used);
        }
    }

    for (auto it = m_awaited_bindings.begin(); it != m_awaited_bindings.end();) {
        const auto slot = *it;
        const bool pending = std::ranges::any_of(unregistered, [&slot](const auto& used) {
            return used.set == slot.first && used.binding == slot.second;
        });
        if (pending) {
            ++it;
            continue;
        }
        MF_INFO(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Set {} binding {} is now registered, the shader's binding is supplied",
            slot.first, slot.second);
        it = m_awaited_bindings.erase(it);
    }

    if (!unregistered.empty()) {
        constexpr uint32_t k_binding_grace_ticks = 30;
        const bool waiting = m_lane->pipeline == Portal::Graphics::INVALID_RENDER_PIPELINE
            && m_binding_wait_ticks < k_binding_grace_ticks;

        for (const auto& used : unregistered) {
            const bool first_seen = m_awaited_bindings.emplace(used.set, used.binding).second;
            if (waiting && first_seen) {
                MF_INFO(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                    "Waiting for set {} binding {} ({}), which the shader uses, to be registered before creating the pipeline",
                    used.set, used.binding, vk::to_string(used.type));
            }
        }

        if (waiting) {
            ++m_binding_wait_ticks;
            m_needs_pipeline_rebuild = true;
            return;
        }

        for (const auto& used : unregistered) {
            MF_WARN(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "Set {} binding {} ({}), which the shader uses, is not registered. "
                "Creating the pipeline with an empty slot that is filled once a processor registers it",
                used.set, used.binding, vk::to_string(used.type));
            unified_bindings.emplace(std::make_pair(used.set, used.binding),
                Portal::Graphics::DescriptorBindingInfo {
                    .set = used.set,
                    .binding = used.binding,
                    .type = used.type,
                    .buffer_info = {},
                    .name = used.name });
        }
    }
    m_binding_wait_ticks = 0;

    std::map<uint32_t, std::vector<Portal::Graphics::DescriptorBindingInfo>> bindings_by_set;
    for (const auto& [key, binding] : unified_bindings) {
        bindings_by_set[binding.set].push_back(binding);
    }

    for (const auto& [set_index, set_bindings] : bindings_by_set) {
        pipeline_config.descriptor_sets.push_back(set_bindings);
    }

    const DestinationInfo destination = describe();
    const vk::Format depth_format = destination.depth;

    retire_current_pipeline();

    m_lane->pipeline = flow.create_pipeline(pipeline_config, destination.colors, depth_format, 0U,
        { { 0U, m_triangulate ? 1U : 0U },
            { 1U, m_triangulate && m_mill_spec.billboard_instances
                    && m_mill_spec.ribbon == Portal::Graphics::MillSpec::Ribbon::WorldFacing
                ? 1U
                : 0U } });

    if (m_lane->pipeline == Portal::Graphics::INVALID_RENDER_PIPELINE) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Failed to create render pipeline");
        return;
    }

    if (m_depth_enabled) {
        buffer->set_needs_depth_attachment(true);
    }

    m_needs_descriptor_rebuild = m_descriptor_set_ids.empty();
    m_needs_pipeline_rebuild = false;

    on_pipeline_created(m_lane->pipeline);
}

void RenderProcessor::initialize_descriptors(const std::shared_ptr<VKBuffer>& buffer)
{
    if (m_lane->pipeline == Portal::Graphics::INVALID_RENDER_PIPELINE) {
        if (!m_needs_pipeline_rebuild) {
            MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "Cannot allocate descriptor sets without pipeline");
        }
        return;
    }

    on_before_descriptors_create();

    auto& flow = Portal::Graphics::get_render_flow();
    auto& foundry = Portal::Graphics::get_shader_foundry();

    auto transform_layout = flow.get_view_transform_layout(m_lane->pipeline);
    if (transform_layout) {
        m_lane->transform_arena.initialize(sizeof(Kinesis::RenderTransform), Portal::Graphics::BufferUsageHint::UNIFORM);
        m_lane->transform_slot = 0;
        m_lane->written_transform.reset();

        MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "RenderTransform UBO allocated (pipeline {})", m_lane->pipeline);
    }

    m_descriptor_set_ids = flow.allocate_pipeline_descriptors(m_lane->pipeline, 1);

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
    invalidate_all_mills();

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "RenderProcessor: Set vertex range [offset={}, count={}]",
        first_vertex, vertex_count);
}

void RenderProcessor::set_buffer_vertex_layout(
    const std::shared_ptr<VKBuffer>& buffer,
    const Kakshya::VertexLayout& layout)
{
    const bool known = m_buffer_info.contains(buffer);
    auto& state = m_buffer_info[buffer];

    const bool same_input = state.semantic_layout.stride_bytes == layout.stride_bytes
        && std::ranges::equal(state.semantic_layout.attributes, layout.attributes,
            [](const auto& a, const auto& b) {
                return a.component_modality == b.component_modality
                    && a.offset_in_vertex == b.offset_in_vertex;
            });
    const bool changed = !known || state.use_reflection || !same_input;

    state.semantic_layout = layout;
    state.use_reflection = false;

    if (changed) {
        invalidate_pipelines();
    }
}

const Kinesis::RenderTransform& RenderProcessor::publish_render_transform()
{
    resolve_render_transform();
    apply_lane_view();
    write_render_transform();
    return m_published_render_transform;
}

const Kinesis::RenderTransform& RenderProcessor::resolve_render_transform()
{
    Kinesis::RenderTransform transform;
    if (m_view_transform_active) {
        transform.view_transform = m_view_transform_source
            ? m_view_transform_source()
            : m_view_transform.value_or(Kinesis::ViewTransform {});
    }
    transform.geometry = m_geometry_transform_source
        ? m_geometry_transform_source()
        : m_geometry_transform.value_or(glm::mat4 { 1.0F });

    m_published_render_transform = transform;
    return m_published_render_transform;
}

void RenderProcessor::write_render_transform()
{
    if (m_lane->transform_arena.is_initialized()) {
        const auto& next = m_published_render_transform;
        const auto& written = m_lane->written_transform;
        if (!written
            || written->view_transform.view != next.view_transform.view
            || written->view_transform.projection != next.view_transform.projection
            || written->geometry != next.geometry) {
            m_lane->transform_slot = m_lane->transform_arena.acquire();
            m_lane->transform_arena.write(m_lane->transform_slot, &next, sizeof(Kinesis::RenderTransform));
            m_lane->written_transform = next;
        }
    }
}

void RenderProcessor::set_triangulate(bool enabled)
{
    if (m_triangulate == enabled) {
        return;
    }

    m_triangulate = enabled;
    invalidate_pipelines();
    invalidate_all_mills();
}

namespace {

    vk::Rect2D pixel_rect(const Kinesis::AABB2D& bounds, uint32_t width, uint32_t height) noexcept;

}

vk::Rect2D RenderProcessor::resolve_area_rect(
    const Portal::Graphics::TargetArea& area, uint32_t width, uint32_t height) noexcept
{
    if (!area.area) {
        return { { 0, 0 }, { width, height } };
    }
    return pixel_rect(area.area->bounds, width, height);
}

vk::Rect2D RenderProcessor::resolve_scissor_rect(uint32_t width, uint32_t height) const noexcept
{
    if (!m_scissor) {
        return { { 0, 0 }, { width, height } };
    }
    return pixel_rect(m_scissor->bounds, width, height);
}

namespace {

vk::Rect2D pixel_rect(const Kinesis::AABB2D& bounds, uint32_t width, uint32_t height) noexcept
{
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

}

void RenderProcessor::set_runs(std::vector<Portal::Graphics::DrawRun> runs)
{
    m_runs = std::move(runs);
    invalidate_all_mills();
}

void RenderProcessor::set_mill_spec(const Portal::Graphics::MillSpec& spec)
{
    const auto billboards = [](const Portal::Graphics::MillSpec& value) {
        return value.billboard_instances
            && value.ribbon == Portal::Graphics::MillSpec::Ribbon::WorldFacing;
    };
    if (billboards(m_mill_spec) != billboards(spec)) {
        invalidate_pipelines();
    }

    m_mill_spec = spec;
    for (auto& [buffer, state] : m_buffer_info) {
        if (state.mill)
            state.mill->set_spec(spec);
    }
    invalidate_all_mills();
}

void RenderProcessor::set_mill_on_change(bool enabled)
{
    m_mill_on_change = enabled;
    invalidate_all_mills();
}

void RenderProcessor::invalidate_mill(const std::shared_ptr<VKBuffer>& buffer)
{
    if (auto it = m_buffer_info.find(buffer); it != m_buffer_info.end()) {
        it->second.mill_current = false;
    }
}

void RenderProcessor::invalidate_all_mills()
{
    for (auto& [buffer, state] : m_buffer_info) {
        state.mill_current = false;
    }
}

uint32_t RenderProcessor::mill_passes() const
{
    return m_mill_passes;
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
    std::optional<Kakshya::VertexLayout> mill_layout;
    if (buffer->prefers_semantic_layout()) {
        if (auto* state = get_or_cache_buffer_state(buffer))
            mill_layout = state->semantic_layout;
    }

    std::span<const Portal::Graphics::DrawRun> runs = m_runs;

    Portal::Graphics::DrawRun whole {};
    if (runs.empty()) {
        const auto layout = mill_layout ? mill_layout : buffer->get_vertex_layout();
        const uint32_t count = buffer->has_index_buffer()
            ? static_cast<uint32_t>(buffer->get_index_buffer_size() / sizeof(uint32_t))
            : (m_vertex_count > 0
                      ? m_vertex_count
                      : (layout.has_value() ? layout->vertex_count : 0U));

        if (count == 0) {
            return 0;
        }

        whole = { .topology = m_primitive_topology,
            .vertex_offset = m_first_vertex,
            .vertex_count = count };
        runs = { &whole, 1 };
    }

    if (mill_layout) {
        for (const auto& run : runs) {
            const uint32_t vertex_end = run.vertex_offset + run.vertex_count;
            if (vertex_end >= run.vertex_offset)
                mill_layout->vertex_count = std::max(mill_layout->vertex_count, vertex_end);
        }
    }

    if (m_polygon_mode != Portal::Graphics::PolygonMode::FILL) {
        m_mill_fill_runs.assign(runs.begin(), runs.end());
        for (auto& run : m_mill_fill_runs) {
            run.fill = m_polygon_mode;
        }
        runs = m_mill_fill_runs;
    }

    glm::vec3 eye = m_view_transform_active || has_view(*m_lane)
        ?glm::vec3(glm::inverse(published_render_transform().view_transform.view)[3])
        : glm::vec3(0.0F, 0.0F, 1.0e4F);

    if (m_mill_spec.ribbon == Portal::Graphics::MillSpec::Ribbon::WorldFacing
        && (m_geometry_transform || m_geometry_transform_source)) {
        const auto& geometry = published_render_transform().geometry;
        const float determinant = glm::determinant(geometry);
        if (std::isfinite(determinant) && determinant != 0.0F) {
            const glm::vec4 local_eye = glm::inverse(geometry) * glm::vec4(eye, 1.0F);
            if (std::isfinite(local_eye.x) && std::isfinite(local_eye.y)
                && std::isfinite(local_eye.z) && std::isfinite(local_eye.w)
                && local_eye.w != 0.0F) {
                eye = glm::vec3(local_eye) / local_eye.w;
            }
        }
    }

    const Portal::Graphics::MillView view { .eye = eye };

    return mill.mill(buffer, runs, view, std::move(mill_layout));
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
        const bool reuse = m_mill_on_change && state.mill_current && state.milled_count > 0
            && m_mill_spec.ribbon != Portal::Graphics::MillSpec::Ribbon::WorldFacing
            && state.mill->output();
        if (reuse) {
            state.vertex_count = state.milled_count;
            state.draw_source = state.mill->output();
        } else {
            state.vertex_count = mill_runs(buffer, *state.mill);
            state.milled_count = state.vertex_count;
            state.mill_current = state.vertex_count > 0;
            if (state.vertex_count > 0) {
                ++m_mill_passes;
                state.draw_source = state.mill->output();
            } else if (state.draw_source != buffer) {
                state.draw_source = buffer;
            }
        }
        m_last_milled_vertex_count = state.vertex_count;
        state.first_vertex = 0;
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

    if (m_lane == m_lanes.front().get()) {
        follow_target_images();
        if (m_needs_descriptor_rebuild) {
            for (size_t i = 1; i < m_lanes.size(); ++i) {
                m_lanes[i]->needs_descriptors = true;
            }
        }
    }

    if (!std::ranges::any_of(m_lanes, [](const auto& lane) { return lane->window || lane->image; })) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Target window or image not set");
        return false;
    }
    return std::ranges::any_of(m_lanes, [this](const auto& lane) { return destination_ready(*lane); });
}

void RenderProcessor::execute_shader(const std::shared_ptr<VKBuffer>& buffer)
{
    collect_retired_pipelines();

    if (!is_visible(buffer) || !std::ranges::any_of(m_lanes, [this](const auto& lane) { return destination_ready(*lane); })) {
        return;
    }

    auto* state = get_or_cache_buffer_state(buffer);
    if (!state) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "VKBuffer has no vertex layout set. Use buffer->set_vertex_layout()");
        return;
    }

    const Kinesis::RenderTransform shared_transform = resolve_render_transform();

    bool geometry_ready = false;
    for (size_t i = 0; i < m_lanes.size(); ++i) {
        ActiveLane scope(*this, *m_lanes[i]);

        if (!destination_ready(*m_lane)) {
            continue;
        }
        if (i > 0) {
            prepare_for(buffer);
        }
        if (m_lane->pipeline == Portal::Graphics::INVALID_RENDER_PIPELINE) {
            continue;
        }

        m_published_render_transform = shared_transform;
        apply_lane_view();
        write_render_transform();
        if (!geometry_ready) {
            prepare_geometry(buffer, *state);
            geometry_ready = true;
        }
        record_draw(buffer, *state);
    }
}

void RenderProcessor::retire_current_pipeline()
{
    if (m_lane->pipeline == Portal::Graphics::INVALID_RENDER_PIPELINE) {
        return;
    }

    for (const auto& entry : m_buffer_info) {
        entry.first->remove_pipeline_window(m_lane->pipeline);
    }

    if (!m_lane->window) {
        auto retired = std::make_shared<RetiredPipeline>(RetiredPipeline {
            .pipeline = m_lane->pipeline,
            .window = nullptr,
            .transform_ubo = m_lane->transform_arena.detach(),
            .after_frame = 0 });
        Portal::Graphics::get_shader_foundry().release_after_submit([retired] {
            Portal::Graphics::get_render_flow().destroy_pipeline(retired->pipeline);
        });
        m_lane->written_transform.reset();
        m_lane->pipeline = Portal::Graphics::INVALID_RENDER_PIPELINE;
        return;
    }

    uint64_t after_frame = std::numeric_limits<uint64_t>::max();
    if (m_display_service && m_display_service->get_submitted_frame_count && m_lane->window) {
        after_frame = m_display_service->get_submitted_frame_count(m_lane->window);
    }

    m_retired.push_back({ .pipeline = m_lane->pipeline,
        .window = m_lane->window,
        .transform_ubo = m_lane->transform_arena.detach(),
        .after_frame = after_frame });
    m_lane->written_transform.reset();

    m_lane->pipeline = Portal::Graphics::INVALID_RENDER_PIPELINE;
}

void RenderProcessor::collect_retired_pipelines()
{
    if (m_retired.empty() || !m_display_service || !m_display_service->get_completed_frame_count) {
        return;
    }

    auto& flow = Portal::Graphics::get_render_flow();

    std::erase_if(m_retired, [&](const RetiredPipeline& retired) {
        if (retired.after_frame > m_display_service->get_completed_frame_count(retired.window)) {
            return false;
        }
        flow.destroy_pipeline(retired.pipeline);
        return true;
    });
}

void RenderProcessor::record_draw(const std::shared_ptr<VKBuffer>& buffer, Portal::Graphics::RenderPipelineID pipeline)
{
    const auto lane = std::ranges::find_if(m_lanes, [pipeline](const auto& held) {
        return held->pipeline == pipeline;
    });
    if (lane == m_lanes.end()) {
        return;
    }

    ActiveLane scope(*this, **lane);
    if (!m_initialized || !is_visible(buffer) || !destination_ready(*m_lane)
        || m_lane->pipeline == Portal::Graphics::INVALID_RENDER_PIPELINE)
        return;

    const auto it = m_buffer_info.find(buffer);
    if (it == m_buffer_info.end() || !it->second.geometry_prepared)
        return;
    record_draw(buffer, it->second);
}

void RenderProcessor::collect_engine_writes(
    const std::shared_ptr<VKBuffer>& buffer,
    std::vector<vk::WriteDescriptorSet>& writes,
    std::vector<vk::DescriptorBufferInfo>& buffer_infos,
    std::vector<vk::DescriptorImageInfo>& image_infos,
    std::vector<Portal::Graphics::SlotLease>& leases) const
{
    const auto& engine_bindings = buffer->get_engine_context().ssbo_bindings;
    buffer_infos.reserve(1 + engine_bindings.size());
    image_infos.reserve(m_texture_bindings.size());
    writes.reserve(1 + engine_bindings.size() + m_texture_bindings.size());

    if (m_lane->transform_arena.is_initialized()) {
        buffer_infos.emplace_back(m_lane->transform_arena.info(m_lane->transform_slot));
        leases.push_back(m_lane->transform_arena.lease(m_lane->transform_slot));
        vk::WriteDescriptorSet write {};
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = vk::DescriptorType::eUniformBuffer;
        write.pBufferInfo = &buffer_infos.back();
        writes.push_back(write);
    }

    for (const auto& binding : engine_bindings) {
        if (binding.set == 0 && binding.binding == 0) {
            MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "Engine SSBO at binding=0 is reserved for RenderTransform UBO");
            continue;
        }
        buffer_infos.emplace_back(binding.buffer_info.buffer, binding.buffer_info.offset, binding.buffer_info.range);
        if (binding.arena) {
            leases.push_back(binding.arena->lease(binding.slot));
        }
        vk::WriteDescriptorSet write {};
        write.dstBinding = binding.binding;
        write.descriptorCount = 1;
        write.descriptorType = binding.type;
        write.pBufferInfo = &buffer_infos.back();
        writes.push_back(write);
    }

    for (const auto& [binding, tex_binding] : m_texture_bindings) {
        if (!tex_binding.texture
            || !tex_binding.texture->is_initialized()
            || !tex_binding.texture->get_image_view()) {
            continue;
        }

        const auto config_it = std::ranges::find_if(m_config.bindings,
            [binding](const auto& pair) {
                return binding >= pair.second.binding
                    && binding < pair.second.binding + pair.second.count;
            });
        if (config_it == m_config.bindings.end() || config_it->second.set != 0) {
            continue;
        }

        image_infos.emplace_back(tex_binding.sampler, tex_binding.texture->get_image_view(),
            vk::ImageLayout::eShaderReadOnlyOptimal);
        vk::WriteDescriptorSet write {};
        write.dstBinding = config_it->second.binding;
        write.dstArrayElement = binding - config_it->second.binding;
        write.descriptorCount = 1;
        write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        write.pImageInfo = &image_infos.back();
        writes.push_back(write);
    }

}

void RenderProcessor::record_draw(const std::shared_ptr<VKBuffer>& buffer, const BufferState& state)
{
    std::vector<vk::WriteDescriptorSet> engine_writes;
    std::vector<vk::DescriptorBufferInfo> engine_buffer_infos;
    std::vector<vk::DescriptorImageInfo> engine_image_infos;
    std::vector<Portal::Graphics::SlotLease> engine_leases;
    collect_engine_writes(buffer, engine_writes, engine_buffer_infos, engine_image_infos, engine_leases);

    auto& foundry = Portal::Graphics::get_shader_foundry();
    auto& flow = Portal::Graphics::get_render_flow();

    if (const auto missing = flow.first_unwritten_push_binding(m_lane->pipeline, engine_writes)) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Set 0 binding {} is used by the shaders but has nothing to write; draw skipped", *missing);
        return;
    }

    register_pipeline(buffer);

    const DestinationInfo destination = describe();
    const vk::Format depth_format = destination.depth;

    auto cmd_id = foundry.begin_secondary_commands(std::span<const vk::Format> { destination.colors }, depth_format);
    auto cmd = foundry.get_command_buffer(cmd_id);

    for (const auto& [binding, texture] : m_texture_bindings) {
        foundry.hold_image(cmd_id, texture.texture);
    }

    const uint32_t width = destination.width;
    const uint32_t height = destination.height;

    const vk::Rect2D region = resolve_area_rect(m_lane->area, width, height);
    if (region.extent.width > 0 && region.extent.height > 0) {
        auto cmd = foundry.get_command_buffer(cmd_id);

        vk::Viewport viewport {
            static_cast<float>(region.offset.x),
            static_cast<float>(region.offset.y) + static_cast<float>(region.extent.height),
            static_cast<float>(region.extent.width),
            -static_cast<float>(region.extent.height),
            0.0F,
            1.0F
        };
        cmd.setViewport(0, 1, &viewport);

        vk::Rect2D scissor = resolve_scissor_rect(region.extent.width, region.extent.height);
        scissor.offset.x += region.offset.x;
        scissor.offset.y += region.offset.y;
        cmd.setScissor(0, 1, &scissor);
    }

    flow.bind_pipeline(cmd_id, m_lane->pipeline);

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

    flow.push_descriptors(cmd_id, m_lane->pipeline, engine_writes, engine_leases);

    if (!m_descriptor_set_ids.empty()) {
        flow.bind_descriptor_sets(
            cmd_id, m_lane->pipeline,
            m_descriptor_set_ids,
            1);
    }

    const auto push_data = resolve_push_constants(buffer);
    if (!push_data.empty()) {
        flow.push_constants(
            cmd_id, m_lane->pipeline, push_data.data(), push_data.size());
    }

    on_before_execute(cmd_id, buffer);

    flow.bind_vertex_buffers(cmd_id, { state.draw_source });

    if (state.vertex_count > 0) {
        if (state.draw_source->has_index_buffer()) {
            flow.bind_index_buffer(cmd_id, state.draw_source);
            flow.draw_indexed(cmd_id, state.index_count, m_instance_count, 0, 0, 0);
        } else if (!m_triangulate && m_runs.size() > 1
            && std::ranges::all_of(m_runs, [this](const Portal::Graphics::DrawRun& run) {
                   return run.topology == m_primitive_topology;
               })) {
            for (const auto& run : m_runs) {
                if (run.vertex_count > 0) {
                    flow.draw(cmd_id, run.vertex_count, m_instance_count, run.vertex_offset, 0);
                }
            }
        } else {
            flow.draw(cmd_id, state.vertex_count, m_instance_count, state.first_vertex, 0);
        }
    }

    foundry.end_commands(cmd_id);

    buffer->set_pipeline_command(m_lane->pipeline, cmd_id);
    if (m_lane->window) {
        m_lane->window->track_frame_command(cmd_id);
    }

    MF_RT_TRACE(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Recorded secondary command buffer {} for {}",
        cmd_id, destination_name());
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

            invalidate_pipelines();
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

    for (const auto& lane : m_lanes) {
        vk_buffer->remove_pipeline_window(lane->pipeline);
        if (lane->image) {
            lane->image->request_clear();
        }
    }
    for (const auto& lane : m_lanes) {
        if (lane->window) {
            release_window(lane->window, vk_buffer);
        }
    }

    std::erase(m_hidden_buffers, vk_buffer.get());
    m_buffer_info.erase(vk_buffer);
    ShaderProcessor::on_detach(buffer);
}

void RenderProcessor::cleanup()
{
    auto& foundry = Portal::Graphics::get_shader_foundry();
    auto& flow = Portal::Graphics::get_render_flow();

    for (const auto& lane : m_lanes) {
        if (lane->pipeline != Portal::Graphics::INVALID_RENDER_PIPELINE) {
            flow.destroy_pipeline(lane->pipeline);
            lane->pipeline = Portal::Graphics::INVALID_RENDER_PIPELINE;
        }
    }

    for (const auto& retired : m_retired) {
        flow.destroy_pipeline(retired.pipeline);
    }
    m_retired.clear();

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
        state.mill_current = false;
        state.milled_count = 0;
        state.first_vertex = 0;
        state.vertex_count = 0;
        state.index_count = 0;
    }
    m_last_milled_vertex_count = 0;
    m_mill_passes = 0;
    m_runs.clear();

    m_view_transform_active = false;

    for (const auto& lane : m_lanes) {
        lane->transform_arena.release();
        lane->written_transform.reset();
        if (lane->window) {
            flow.unregister_window(lane->window);
            lane->window.reset();
        }
        lane->image.reset();
    }
    m_lanes.resize(1);
    m_lane = m_lanes.front().get();

    ShaderProcessor::cleanup();

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "RenderProcessor cleanup complete");
}

} // namespace MayaFlux::Buffers
