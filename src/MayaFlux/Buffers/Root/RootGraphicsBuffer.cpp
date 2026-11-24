#include "RootGraphicsBuffer.hpp"

#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"

#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/DisplayService.hpp"

#include "MayaFlux/Core/Backends/Windowing/Window.hpp"
#include "MayaFlux/Portal/Graphics/ImageTarget.hpp"
#include "MayaFlux/Portal/Graphics/RenderFlow.hpp"

namespace MayaFlux::Buffers {

namespace {

using ImageTargetPtr = std::shared_ptr<Portal::Graphics::ImageTarget>;
using RenderableInfo = RootGraphicsBuffer::RenderableBufferInfo;
using Children = std::vector<std::shared_ptr<VKBuffer>>;

/**
 * Active targets that buffers draw into, each once, taking the redraw request
 * a changed buffer implies.
 */
std::vector<ImageTargetPtr> collect_targets(const Children& children)
{
    std::vector<ImageTargetPtr> targets;
    for (const auto& child : children) {
        if (!child || child->needs_removal())
            continue;

        const bool changed = child->take_content_dirty();
        for (const auto& [id, image] : child->get_image_pipelines()) {
            if (!image || !image->active())
                continue;
            if (changed)
                image->request_redraw();
            if (std::ranges::find(targets, image) == targets.end())
                targets.push_back(image);
        }
    }
    return targets;
}

/**
 * Which targets redraw: those that asked, and any whose buffers sample the
 * image of one that does, until no more are added.
 */
std::vector<bool> redrawing(const Children& children, const std::vector<ImageTargetPtr>& targets)
{
    std::vector<bool> drawing;
    drawing.reserve(targets.size());
    for (const auto& target : targets)
        drawing.push_back(target->take_redraw());

    const auto samples_a_redrawing_source = [&](size_t consumer) {
        for (const auto& child : children) {
            if (!child || child->needs_removal())
                continue;
            for (const auto& [id, target] : child->get_image_pipelines()) {
                if (target != targets[consumer])
                    continue;
                const auto render = child->get_render_processor(id);
                if (!render || !render->is_visible(child))
                    continue;
                for (const auto& texture : render->bound_textures()) {
                    for (size_t source = 0; source < targets.size(); ++source) {
                        if (drawing[source] && source != consumer && targets[source]->image() == texture)
                            return true;
                    }
                }
            }
        }
        return false;
    };

    for (bool grew = true; grew;) {
        grew = false;
        for (size_t consumer = 0; consumer < targets.size(); ++consumer) {
            if (!drawing[consumer] && samples_a_redrawing_source(consumer)) {
                drawing[consumer] = true;
                grew = true;
            }
        }
    }
    return drawing;
}

/**
 * Targets in the order they draw: one whose image another target's buffers
 * bind as a texture draws before that target. A cycle keeps its given order.
 */
std::vector<ImageTargetPtr> order_image_targets(
    const std::vector<ImageTargetPtr>& targets,
    const std::unordered_map<ImageTargetPtr, std::vector<const RenderableInfo*>>& groups)
{
    std::unordered_map<ImageTargetPtr, std::vector<ImageTargetPtr>> producers;
    for (const auto& consumer : targets) {
        for (const auto* info : groups.at(consumer)) {
            const auto render = info->buffer->get_render_processor(info->pipeline_id);
            if (!render)
                continue;
            for (const auto& texture : render->bound_textures()) {
                for (const auto& producer : targets) {
                    if (producer != consumer && producer->image() == texture)
                        producers[consumer].push_back(producer);
                }
            }
        }
    }

    std::vector<ImageTargetPtr> ordered;
    std::vector<ImageTargetPtr> remaining = targets;
    while (!remaining.empty()) {
        auto next = std::ranges::find_if(remaining, [&](const ImageTargetPtr& candidate) {
            const auto found = producers.find(candidate);
            if (found == producers.end())
                return true;
            return std::ranges::all_of(found->second, [&](const ImageTargetPtr& producer) {
                return std::ranges::find(ordered, producer) != ordered.end();
            });
        });
        if (next == remaining.end())
            next = remaining.begin();
        ordered.push_back(*next);
        remaining.erase(next);
    }
    return ordered;
}

std::vector<RenderableInfo> record_image_targets(const Children& children)
{
    const auto targets = collect_targets(children);
    const auto drawing = redrawing(children, targets);

    std::vector<RenderableInfo> recorded;
    std::vector<Portal::Graphics::CommandBufferID> skipped;
    for (size_t index = 0; index < targets.size(); ++index) {
        const auto& image = targets[index];

        if (!drawing[index]) {
            for (const auto& child : children) {
                if (!child)
                    continue;
                for (const auto& [id, target] : child->get_image_pipelines()) {
                    const auto command = child->get_pipeline_command(id);
                    if (target == image && command != Portal::Graphics::INVALID_COMMAND_BUFFER)
                        skipped.push_back(command);
                }
            }
            continue;
        }

        for (const auto& child : children) {
            if (!child || child->needs_removal())
                continue;

            for (const auto& [id, target] : child->get_image_pipelines()) {
                if (target != image)
                    continue;

                auto render = child->get_render_processor(id);
                if (!render || !render->is_visible(child))
                    continue;

                try {
                    auto command = child->get_pipeline_command(id);
                    if (command == Portal::Graphics::INVALID_COMMAND_BUFFER) {
                        render->record_draw(child, id);
                        command = child->get_pipeline_command(id);
                    }
                    if (command == Portal::Graphics::INVALID_COMMAND_BUFFER)
                        continue;

                    RenderableInfo info;
                    info.buffer = child;
                    info.target_image = image;
                    info.pipeline_id = id;
                    info.command_buffer_id = command;
                    info.needs_depth = child->needs_depth_attachment();
                    recorded.push_back(info);
                } catch (const std::exception& e) {
                    MF_RT_ERROR(Journal::Component::Core, Journal::Context::BufferProcessing,
                        "Error recording graphics buffer for an image target: {}", e.what());
                }
            }
        }
    }

    if (!skipped.empty()) {
        Portal::Graphics::get_shader_foundry().retire_after_submit(skipped);
    }
    return recorded;
}

/**
 * One scope per image target that has something recorded. A target whose image
 * another target's buffers bind as a texture draws first.
 */
void draw_image_targets(const std::vector<RenderableInfo>& renderables)
{
    std::vector<ImageTargetPtr> targets;
    std::unordered_map<ImageTargetPtr, std::vector<const RenderableInfo*>> groups;
    for (const auto& renderable : renderables) {
        if (!renderable.target_image
            || renderable.command_buffer_id == Portal::Graphics::INVALID_COMMAND_BUFFER) {
            continue;
        }
        auto& group = groups[renderable.target_image];
        if (group.empty())
            targets.push_back(renderable.target_image);
        group.push_back(&renderable);
    }

    auto& flow = Portal::Graphics::get_render_flow();

    for (const auto& target : order_image_targets(targets, groups)) {
        const auto& infos = groups.at(target);

        std::vector<Portal::Graphics::CommandBufferID> secondaries;
        for (const auto* info : infos)
            secondaries.push_back(info->command_buffer_id);

        const bool use_depth = std::ranges::any_of(infos, [](const RenderableInfo* info) {
            return info->needs_depth;
        });

        if (const auto point = flow.draw_image_target(*target, secondaries, use_depth)) {
            for (const auto* info : infos)
                info->buffer->note_use(point.timeline, point.value);
        }

        for (const auto* info : infos)
            info->buffer->clear_pipeline_commands();
    }
}

}

GraphicsBatchProcessor::GraphicsBatchProcessor(std::shared_ptr<Buffer> root_buffer)
    : m_root_buffer(std::dynamic_pointer_cast<RootGraphicsBuffer>(std::move(root_buffer)))
{
    m_processing_token = ProcessingToken::GRAPHICS_BACKEND;
}

void GraphicsBatchProcessor::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    auto root_buf = std::dynamic_pointer_cast<RootGraphicsBuffer>(buffer);
    if (!root_buf || root_buf != m_root_buffer.lock()) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::BufferProcessing,
            "GraphicsBatchProcessor can only process its associated RootGraphicsBuffer");
        return;
    }

    root_buf->clear_renderable_buffers();
    root_buf->cleanup_marked_buffers();

    for (const auto& child : root_buf->get_child_buffers()) {
        if (!child)
            continue;
        child->clear_pipeline_commands();

        try {
            if (!child->needs_removal() && child->has_data_for_cycle()) {
                if (child->needs_default_processing() && child->get_default_processor())
                    child->process_default();

                if (auto chain = child->get_processing_chain()) {
                    if (child->has_data_for_cycle())
                        chain->process_complete(child);
                }

                if (!child->needs_removal()) {
                    for (const auto& [id, window] : child->get_render_pipelines()) {
                        const auto command = child->get_pipeline_command(id);
                        if (command == Portal::Graphics::INVALID_COMMAND_BUFFER)
                            continue;

                        RootGraphicsBuffer::RenderableBufferInfo info;
                        info.buffer = child;
                        info.target_window = window;
                        info.pipeline_id = id;
                        info.command_buffer_id = command;
                        info.needs_depth = child->needs_depth_attachment();
                        root_buf->add_renderable_buffer(info);
                    }
                }
            }
        } catch (const std::exception& e) {
            MF_RT_ERROR(Journal::Component::Core, Journal::Context::BufferProcessing,
                "Error processing graphics buffer: {}", e.what());
        }

        if (child->needs_removal()) {
            for (const auto& [id, window] : child->get_render_pipelines())
                root_buf->request_presentation_refresh(window);
        }
        for (const auto& window : child->take_presentation_refreshes())
            root_buf->request_presentation_refresh(window);
    }

    const auto& refreshes = root_buf->get_pending_presentation_refreshes();
    if (!refreshes.empty()) {
        std::erase_if(root_buf->m_renderable_buffers, [&refreshes](const auto& info) {
            return refreshes.contains(info.target_window);
        });

        for (const auto& child : root_buf->get_child_buffers()) {
            if (!child || child->needs_removal())
                continue;

            for (const auto& [id, window] : child->get_render_pipelines()) {
                if (!refreshes.contains(window) || !window || !window->is_graphics_registered())
                    continue;

                auto render = child->get_render_processor(id);
                if (!render || !render->is_visible(child))
                    continue;

                try {
                    auto command = child->get_pipeline_command(id);
                    if (command == Portal::Graphics::INVALID_COMMAND_BUFFER) {
                        render->record_draw(child, id);
                        command = child->get_pipeline_command(id);
                    }
                    if (command == Portal::Graphics::INVALID_COMMAND_BUFFER)
                        continue;

                    RootGraphicsBuffer::RenderableBufferInfo info;
                    info.buffer = child;
                    info.target_window = window;
                    info.pipeline_id = id;
                    info.command_buffer_id = command;
                    info.needs_depth = child->needs_depth_attachment();
                    root_buf->add_renderable_buffer(info);
                } catch (const std::exception& e) {
                    MF_RT_ERROR(Journal::Component::Core, Journal::Context::BufferProcessing,
                        "Error recording retained graphics buffer: {}", e.what());
                }
            }
        }
    }

    for (const auto& info : record_image_targets(root_buf->get_child_buffers()))
        root_buf->add_renderable_buffer(info);

    root_buf->sort_renderable_buffers();
}

void GraphicsBatchProcessor::on_attach(const std::shared_ptr<Buffer>& buffer)
{
    auto root_graphics_buffer = std::dynamic_pointer_cast<RootGraphicsBuffer>(buffer);
    if (!root_graphics_buffer) {
        error<std::invalid_argument>(
            Journal::Component::Core,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "GraphicsBatchProcessor can only be attached to RootGraphicsBuffer");
    }

    if (!are_tokens_compatible(ProcessingToken::GRAPHICS_BACKEND, m_processing_token)) {
        error<std::runtime_error>(
            Journal::Component::Core,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "GraphicsBatchProcessor token incompatible with RootGraphicsBuffer requirements");
    }
}

bool GraphicsBatchProcessor::is_compatible_with(const std::shared_ptr<Buffer>& buffer) const
{
    return std::dynamic_pointer_cast<RootGraphicsBuffer>(buffer) != nullptr;
}

PresentProcessor::PresentProcessor(RenderCallback callback)
    : m_callback(std::move(callback))
{
    m_processing_token = ProcessingToken::GRAPHICS_BACKEND;
}

PresentProcessor::PresentProcessor()
    : m_callback(nullptr)
{
    m_processing_token = ProcessingToken::GRAPHICS_BACKEND;
}

void PresentProcessor::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    auto root_graphics_buffer = std::dynamic_pointer_cast<RootGraphicsBuffer>(buffer);
    if (!root_graphics_buffer) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "RenderProcessor received non-RootGraphicsBuffer");
        return;
    }

    if (m_attached && root_graphics_buffer != m_root_buffer.lock()) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "RenderProcessor processing buffer that doesn't match attached root");
        return;
    }

    if (m_callback) {
        try {
            m_callback(root_graphics_buffer);
        } catch (const std::exception& e) {
            error_rethrow(
                Journal::Component::Buffers,
                Journal::Context::BufferProcessing,
                std::source_location::current(),
                "RenderProcessor callback threw exception: {}",
                e.what());
        }
    } else {
        fallback_renderer(root_graphics_buffer);
    }
}

void PresentProcessor::on_attach(const std::shared_ptr<Buffer>& buffer)
{
    auto root_graphics_buffer = std::dynamic_pointer_cast<RootGraphicsBuffer>(buffer);
    if (!root_graphics_buffer) {
        error<std::invalid_argument>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "RenderProcessor can only be attached to RootGraphicsBuffer");
    }

    if (!are_tokens_compatible(ProcessingToken::GRAPHICS_BACKEND, m_processing_token)) {
        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "RenderProcessor token incompatible with RootGraphicsBuffer requirements");
    }

    m_root_buffer = root_graphics_buffer;
    m_attached = true;

    MF_INFO(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "RenderProcessor attached to RootGraphicsBuffer (has_callback: {})",
        has_callback());
}

void PresentProcessor::on_detach(const std::shared_ptr<Buffer>& buffer)
{
    if (auto root = std::dynamic_pointer_cast<RootGraphicsBuffer>(buffer)) {
        if (root == m_root_buffer.lock()) {
            m_root_buffer.reset();
            m_attached = false;
        }
    }

    MF_INFO(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "RenderProcessor detached from RootGraphicsBuffer");
}

bool PresentProcessor::is_compatible_with(const std::shared_ptr<Buffer>& buffer) const
{
    return std::dynamic_pointer_cast<RootGraphicsBuffer>(buffer) != nullptr;
}

void PresentProcessor::set_callback(RenderCallback callback)
{
    m_callback = std::move(callback);

    MF_INFO(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "RenderProcessor callback {} (attached: {})",
        m_callback ? "configured" : "cleared",
        m_attached);
}

void PresentProcessor::fallback_renderer(const std::shared_ptr<RootGraphicsBuffer>& root)
{
    const auto& renderable_buffers = root->get_renderable_buffers();
    const auto& pending_refreshes = root->get_pending_presentation_refreshes();
    if (renderable_buffers.empty() && pending_refreshes.empty()) {
        return;
    }

    auto& foundry = Portal::Graphics::get_shader_foundry();

    std::vector<Portal::Graphics::CommandBufferID> retire_ids;
    for (const auto& renderable : renderable_buffers) {
        if (renderable.command_buffer_id != Portal::Graphics::INVALID_COMMAND_BUFFER) {
            retire_ids.push_back(renderable.command_buffer_id);
        }
    }

    draw_image_targets(renderable_buffers);

    std::unordered_map<std::shared_ptr<Core::Window>, std::vector<const RootGraphicsBuffer::RenderableBufferInfo*>> buffers_by_window;
    for (auto it = pending_refreshes.begin(); it != pending_refreshes.end();) {
        const auto& window = *it++;
        if (window->should_close() || !window->is_graphics_registered()) {
            root->clear_presentation_refresh(window);
            continue;
        }
        buffers_by_window.try_emplace(window);
    }

    for (const auto& renderable : renderable_buffers) {
        if (renderable.target_window && renderable.target_window->is_graphics_registered()
            && renderable.command_buffer_id != Portal::Graphics::INVALID_COMMAND_BUFFER) {
            buffers_by_window[renderable.target_window].push_back(&renderable);
        }
    }

    if (buffers_by_window.empty()) {
        foundry.retire_after_submit(retire_ids);
        root->clear_renderable_buffers();
        return;
    }

    MF_RT_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "PresentProcessor submitting to {} windows", buffers_by_window.size());

    auto& flow = Portal::Graphics::get_render_flow();
    auto display_service = Registry::BackendRegistry::instance()
                               .get_service<Registry::Service::DisplayService>();

    if (!display_service) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "DisplayService not available for dynamic rendering");
        foundry.retire_after_submit(retire_ids);
        return;
    }

    for (const auto& [window, buffer_infos] : buffers_by_window) {
        uint64_t image_bits = display_service->acquire_next_swapchain_image(window);
        if (image_bits == 0) {
            MF_RT_WARN(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "Failed to acquire swapchain image for window '{}'",
                window->get_create_info().title);
            continue;
        }
        vk::Image swapchain_image { reinterpret_cast<VkImage>(image_bits) };

        auto primary_cmd_id = foundry.begin_commands(Portal::Graphics::ShaderFoundry::CommandBufferType::GRAPHICS);
        retire_ids.push_back(primary_cmd_id);
        auto primary_cmd = foundry.get_command_buffer(primary_cmd_id);

        if (!primary_cmd) {
            MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "Failed to create primary command buffer for window '{}'",
                window->get_create_info().title);
            for (const auto* info : buffer_infos) {
                info->buffer->clear_pipeline_commands();
            }
            continue;
        }

        try {
            bool window_needs_depth = false;
            for (const auto* info : buffer_infos) {
                if (info->needs_depth) {
                    window_needs_depth = true;
                    break;
                }
            }

            vk::ImageView depth_view = nullptr;
            if (window_needs_depth) {
                display_service->ensure_depth_attachment(window);
                auto* view_ptr = display_service->get_depth_image_view(window);
                if (view_ptr) {
                    depth_view = *static_cast<vk::ImageView*>(view_ptr);
                }
            }

            flow.begin_rendering(primary_cmd_id, window, swapchain_image,
                Portal::Graphics::default_color, depth_view);

            std::vector<vk::CommandBuffer> secondary_buffers;
            for (const auto* info : buffer_infos) {
                auto secondary = foundry.get_command_buffer(info->command_buffer_id);
                if (secondary) {
                    secondary_buffers.push_back(secondary);
                }
            }

            if (!secondary_buffers.empty()) {
                primary_cmd.executeCommands(secondary_buffers);
            }

            flow.end_rendering(primary_cmd_id, window);

            foundry.end_commands(primary_cmd_id);
            uint64_t primary_bits = *reinterpret_cast<uint64_t*>(&primary_cmd);
            display_service->submit_and_present(window, primary_bits);
            root->clear_presentation_refresh(window);

            MF_RT_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "Presented {} buffers to window '{}'",
                secondary_buffers.size(), window->get_create_info().title);

        } catch (const std::exception& e) {
            MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "Failed to submit/present for window '{}': {}",
                window->get_create_info().title,
                e.what());
        }

        for (const auto* info : buffer_infos) {
            info->buffer->clear_pipeline_commands();
        }
    }

    foundry.retire_after_submit(retire_ids);
    root->clear_renderable_buffers();
}

RootGraphicsBuffer::RootGraphicsBuffer()
    : m_final_processor(nullptr)
{
    m_preferred_processing_token = ProcessingToken::GRAPHICS_BACKEND;
    m_token_enforcement_strategy = TokenEnforcementStrategy::STRICT;
}

RootGraphicsBuffer::~RootGraphicsBuffer()
{
    cleanup_marked_buffers();
    m_child_buffers.clear();
}

void RootGraphicsBuffer::initialize()
{
    auto batch_processor = create_default_processor();
    if (batch_processor) {
        set_default_processor(batch_processor);
    }
}

void RootGraphicsBuffer::process_default()
{
    if (this->has_pending_operations()) {
        this->process_pending_buffer_operations();
    }

    get_default_processor()->process(shared_from_this());
}

void RootGraphicsBuffer::cleanup_marked_buffers()
{
    auto it = std::remove_if(
        m_child_buffers.begin(),
        m_child_buffers.end(),
        [this](const std::shared_ptr<VKBuffer>& buf) {
            if (!buf || !buf->needs_removal())
                return false;
            for (const auto& [id, window] : buf->get_render_pipelines())
                request_presentation_refresh(window);
            for (const auto& [id, image] : buf->get_image_pipelines()) {
                if (image)
                    image->request_clear();
            }
            for (const auto& window : buf->take_presentation_refreshes())
                request_presentation_refresh(window);
            return true;
        });

    size_t removed_count = std::distance(it, m_child_buffers.end());

    if (removed_count > 0) {
        m_child_buffers.erase(it, m_child_buffers.end());

        MF_INFO(Journal::Component::Core, Journal::Context::BufferManagement,
            "Cleaned up {} graphics buffers (remaining: {})",
            removed_count, m_child_buffers.size());
    }
}

void RootGraphicsBuffer::set_final_processor(std::shared_ptr<BufferProcessor> processor)
{
    m_final_processor = std::move(processor);
}

std::shared_ptr<BufferProcessor> RootGraphicsBuffer::get_final_processor() const
{
    return m_final_processor;
}

bool RootGraphicsBuffer::has_buffer(const std::shared_ptr<VKBuffer>& buffer) const
{
    return std::ranges::find(m_child_buffers, buffer) != m_child_buffers.end();
}

std::vector<std::shared_ptr<VKBuffer>> RootGraphicsBuffer::get_buffers_by_usage(VKBuffer::Usage usage) const
{
    std::vector<std::shared_ptr<VKBuffer>> filtered_buffers;

    for (const auto& buffer : m_child_buffers) {
        if (buffer && buffer->get_usage() == usage) {
            filtered_buffers.push_back(buffer);
        }
    }

    return filtered_buffers;
}

void RootGraphicsBuffer::detach_window(const std::shared_ptr<Core::Window>& window)
{
    if (!window)
        return;

    std::erase_if(m_renderable_buffers, [&window](const RenderableBufferInfo& info) {
        return info.target_window == window;
    });

    for (const auto& buffer : m_child_buffers) {
        if (!buffer)
            continue;

        auto chain = buffer->get_processing_chain();
        auto remove_render = [&buffer, &chain, &window](const std::shared_ptr<RenderProcessor>& render) {
            if (!render || !render->targets_window(window))
                return;

            if (render->destination_count() > 1) {
                render->remove_target_window(window, buffer);
                return;
            }

            if (buffer->get_default_processor() == render) {
                buffer->set_default_processor(nullptr);
                return;
            }

            if (!chain)
                return;

            chain->remove_processor_anywhere(render, buffer);
        };

        if (auto render = buffer->get_render_processor())
            remove_render(render);

        for (const auto& render : buffer->get_additional_render_processors())
            remove_render(render);
    }

    m_pending_presentation_refreshes.erase(window);
}

std::shared_ptr<BufferProcessor> RootGraphicsBuffer::create_default_processor()
{
    return std::make_shared<GraphicsBatchProcessor>(shared_from_this());
}

} // namespace MayaFlux::Buffers
