#include "RootGraphicsBuffer.hpp"

#include "MayaFlux/Buffers/BufferProcessingChain.hpp"

#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/DisplayService.hpp"

#include "MayaFlux/Core/Backends/Windowing/Window.hpp"
#include "MayaFlux/Portal/Graphics/RenderFlow.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

GraphicsBatchProcessor::GraphicsBatchProcessor(std::shared_ptr<Buffer> root_buffer)
    : m_root_buffer(std::dynamic_pointer_cast<RootGraphicsBuffer>(std::move(root_buffer)))
{
    m_processing_token = ProcessingToken::GRAPHICS_BACKEND;
}

void GraphicsBatchProcessor::processing_function(std::shared_ptr<Buffer> buffer)
{
    auto root_buf = std::dynamic_pointer_cast<RootGraphicsBuffer>(buffer);
    if (!root_buf || root_buf != m_root_buffer) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::BufferProcessing,
            "GraphicsBatchProcessor can only process its associated RootGraphicsBuffer");
        return;
    }

    root_buf->cleanup_marked_buffers();

    for (auto& ch_buffer : root_buf->get_child_buffers()) {
        if (!ch_buffer)
            continue;

        if (ch_buffer->needs_removal()) {
            continue;
        }

        if (!ch_buffer->has_data_for_cycle()) {
            continue;
        }

        try {
            if (ch_buffer->needs_default_processing() && ch_buffer->get_default_processor()) {
                ch_buffer->process_default();
            }

            if (auto chain = ch_buffer->get_processing_chain()) {
                if (ch_buffer->has_data_for_cycle()) {
                    chain->process_complete(ch_buffer);
                }
            }

            auto vk_buffer = std::dynamic_pointer_cast<Buffers::VKBuffer>(ch_buffer);
            if (vk_buffer && vk_buffer->has_render_pipeline()) {
                for (const auto& [id, window] : vk_buffer->get_render_pipelines()) {
                    RootGraphicsBuffer::RenderableBufferInfo info;
                    info.buffer = vk_buffer;
                    info.target_window = window;
                    info.pipeline_id = id;
                    info.command_buffer_id = vk_buffer->get_pipeline_command(id);

                    root_buf->add_renderable_buffer(info);

                    MF_RT_TRACE(Journal::Component::Core, Journal::Context::BufferProcessing,
                        "Registered buffer for rendering to window '{}'",
                        window->get_create_info().title);
                }
            }

        } catch (const std::exception& e) {
            MF_RT_ERROR(Journal::Component::Core, Journal::Context::BufferProcessing,
                "Error processing graphics buffer: {}", e.what());
        }
    }
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

bool GraphicsBatchProcessor::is_compatible_with(std::shared_ptr<Buffer> buffer) const
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

void PresentProcessor::processing_function(std::shared_ptr<Buffer> buffer)
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

bool PresentProcessor::is_compatible_with(std::shared_ptr<Buffer> buffer) const
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
    if (renderable_buffers.empty()) {
        MF_RT_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "No renderable buffers found in fallback renderer");
        return;
    }

    std::unordered_map<Core::Window*, std::vector<const RootGraphicsBuffer::RenderableBufferInfo*>> buffers_by_window;
    for (const auto& renderable : renderable_buffers) {
        if (renderable.target_window && renderable.target_window->is_graphics_registered()
            && renderable.command_buffer_id != Portal::Graphics::INVALID_COMMAND_BUFFER) {
            buffers_by_window[renderable.target_window.get()].push_back(&renderable);
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

    for (const auto& [window_ptr, buffer_infos] : buffers_by_window) {
        auto window = buffer_infos[0]->target_window;

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
            flow.begin_rendering(primary_cmd_id, window, swapchain_image);

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
        [](const std::shared_ptr<Buffer>& buf) {
            return buf && buf->needs_removal();
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
