#include "RootGraphicsBuffer.hpp"

#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"

#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/DisplayService.hpp"

#include "MayaFlux/Core/Backends/Windowing/Window.hpp"
#include "MayaFlux/Portal/Graphics/RenderFlow.hpp"

namespace MayaFlux::Buffers {

GraphicsBatchProcessor::GraphicsBatchProcessor(std::shared_ptr<Buffer> root_buffer)
    : m_root_buffer(std::dynamic_pointer_cast<RootGraphicsBuffer>(std::move(root_buffer)))
{
    m_processing_token = ProcessingToken::GRAPHICS_BACKEND;
}

void GraphicsBatchProcessor::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    auto root_buf = std::dynamic_pointer_cast<RootGraphicsBuffer>(buffer);
    if (!root_buf || root_buf != m_root_buffer) {
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
                        render->record_draw(child);
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
    , m_root_buffer(nullptr)
{
    m_processing_token = ProcessingToken::GRAPHICS_BACKEND;
}

PresentProcessor::PresentProcessor()
    : m_callback(nullptr)
    , m_root_buffer(nullptr)
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

    if (m_root_buffer && root_graphics_buffer != m_root_buffer) {
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

    MF_INFO(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "RenderProcessor attached to RootGraphicsBuffer (has_callback: {})",
        has_callback());
}

void PresentProcessor::on_detach(const std::shared_ptr<Buffer>& buffer)
{
    if (auto root = std::dynamic_pointer_cast<RootGraphicsBuffer>(buffer)) {
        if (root == m_root_buffer) {
            m_root_buffer = nullptr;
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
        m_root_buffer != nullptr);
}

void PresentProcessor::fallback_renderer(const std::shared_ptr<RootGraphicsBuffer>& root)
{
    const auto& renderable_buffers = root->get_renderable_buffers();
    const auto& pending_refreshes = root->get_pending_presentation_refreshes();
    if (renderable_buffers.empty() && pending_refreshes.empty()) {
        return;
    }

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
        root->clear_renderable_buffers();
        return;
    }

    MF_RT_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "PresentProcessor submitting to {} windows", buffers_by_window.size());

    auto& foundry = Portal::Graphics::get_shader_foundry();
    auto& flow = Portal::Graphics::get_render_flow();
    auto display_service = Registry::BackendRegistry::instance()
                               .get_service<Registry::Service::DisplayService>();

    if (!display_service) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "DisplayService not available for dynamic rendering");
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
    m_pending_removal.clear();
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
    if (m_pending_removal.empty()) {
        return;
    }

    auto it = std::remove_if(
        m_child_buffers.begin(),
        m_child_buffers.end(),
        [this](const std::shared_ptr<VKBuffer>& buf) {
            if (!buf || !buf->needs_removal())
                return false;
            for (const auto& [id, window] : buf->get_render_pipelines())
                request_presentation_refresh(window);
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

    m_pending_removal.clear();
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

std::shared_ptr<BufferProcessor> RootGraphicsBuffer::create_default_processor()
{
    return std::make_shared<GraphicsBatchProcessor>(shared_from_this());
}

} // namespace MayaFlux::Buffers
