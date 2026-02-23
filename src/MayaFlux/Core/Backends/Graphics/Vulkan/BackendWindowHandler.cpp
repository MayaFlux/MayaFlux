#include "BackendWindowHandler.hpp"

#include "VKCommandManager.hpp"
#include "VKSwapchain.hpp"

#include "MayaFlux/Core/Backends/Windowing/Window.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Registry/Service/DisplayService.hpp"

namespace MayaFlux::Core {

void WindowRenderContext::cleanup(VKContext& context)
{
    vk::Device device = context.get_device();

    device.waitIdle();

    for (auto& img : image_available) {
        if (img) {
            device.destroySemaphore(img);
        }
    }
    image_available.clear();

    for (auto& render : render_finished) {
        if (render) {
            device.destroySemaphore(render);
        }
    }
    render_finished.clear();

    clear_command_buffers.clear();

    for (auto& fence : in_flight) {
        if (fence) {
            device.destroyFence(fence);
        }
    }
    in_flight.clear();

    if (swapchain) {
        swapchain->cleanup();
        swapchain.reset();
    }

    if (surface) {
        context.destroy_surface(surface);
        surface = nullptr;
    }

    window->set_graphics_registered(false);
}

BackendWindowHandler::BackendWindowHandler(VKContext& context, VKCommandManager& command_manager)
    : m_context(context)
    , m_command_manager(command_manager)
{
}

void BackendWindowHandler::setup_backend_service(const std::shared_ptr<Registry::Service::DisplayService>& display_service)
{
    display_service->submit_and_present = [this](
                                              const std::shared_ptr<void>& window_ptr,
                                              uint64_t primary_cmd_bits) {
        auto window = std::static_pointer_cast<Window>(window_ptr);
        vk::CommandBuffer primary_cmd = *reinterpret_cast<vk::CommandBuffer*>(&primary_cmd_bits);

        this->submit_and_present(window, primary_cmd);
    };

    display_service->wait_idle = [this]() {
        m_context.get_device().waitIdle();
    };

    display_service->resize_surface = [this](const std::shared_ptr<void>& window_ptr, uint32_t width, uint32_t height) {
        auto window = std::static_pointer_cast<Window>(window_ptr);
        window->set_size(width, height);
        for (auto& ctx : m_window_contexts) {
            if (ctx.window == window) {
                ctx.needs_recreation = true;
                break;
            }
        }
    };

    display_service->get_swapchain_image_count = [this](const std::shared_ptr<void>& window_ptr) -> uint32_t {
        auto window = std::static_pointer_cast<Window>(window_ptr);
        for (const auto& ctx : m_window_contexts) {
            if (ctx.window == window) {
                return static_cast<uint32_t>(ctx.swapchain->get_image_count());
            }
        }
        return 0;
    };

    display_service->get_swapchain_format = [this](const std::shared_ptr<void>& window_ptr) -> uint32_t {
        auto window = std::static_pointer_cast<Window>(window_ptr);
        for (const auto& ctx : m_window_contexts) {
            if (ctx.window == window) {
                return static_cast<int>(ctx.swapchain->get_image_format());
            }
        }
        return 0;
    };

    display_service->get_swapchain_extent = [this](
                                                const std::shared_ptr<void>& window_ptr,
                                                uint32_t& out_width,
                                                uint32_t& out_height) {
        auto window = std::static_pointer_cast<Window>(window_ptr);
        auto* context = find_window_context(window);

        if (context && context->swapchain) {
            auto extent = context->swapchain->get_extent();
            out_width = extent.width;
            out_height = extent.height;
        } else {
            out_width = 0;
            out_height = 0;
        }
    };

    display_service->acquire_next_swapchain_image = [this](const std::shared_ptr<void>& window_ptr) -> uint64_t {
        auto window = std::static_pointer_cast<Window>(window_ptr);
        auto* ctx = find_window_context(window);
        if (!ctx) {
            MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
                "Window '{}' not registered for swapchain acquisition",
                window->get_create_info().title);
            return 0;
        }

        auto device = m_context.get_device();
        size_t frame_index = ctx->current_frame;
        auto& in_flight = ctx->in_flight[frame_index];
        auto& image_available = ctx->image_available[frame_index];

        if (device.waitForFences(1, &in_flight, VK_TRUE, UINT64_MAX) == vk::Result::eTimeout) {
            MF_RT_WARN(Journal::Component::Core, Journal::Context::GraphicsCallback,
                "Fence timeout during swapchain acquisition for window '{}'",
                window->get_create_info().title);
            return 0;
        }

        auto image_index_opt = ctx->swapchain->acquire_next_image(image_available);
        if (!image_index_opt.has_value()) {
            ctx->needs_recreation = true;
            return 0;
        }

        ctx->current_image_index = image_index_opt.value();

        if (device.resetFences(1, &in_flight) != vk::Result::eSuccess) {
            MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
                "Failed to reset fence for window '{}'",
                window->get_create_info().title);
            return 0;
        }

        const auto& images = ctx->swapchain->get_images();
        VkImage raw = images[ctx->current_image_index];
        return reinterpret_cast<uint64_t>(raw);
    };

    display_service->get_current_image_view = [this](const std::shared_ptr<void>& window_ptr) -> void* {
        auto window = std::static_pointer_cast<Window>(window_ptr);
        auto* context = find_window_context(window);

        if (!context || !context->swapchain) {
            return nullptr;
        }

        const auto& image_views = context->swapchain->get_image_views();
        if (context->current_image_index >= image_views.size()) {
            MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
                "Invalid current_image_index {} for window '{}' (swapchain has {} images)",
                context->current_image_index,
                window->get_create_info().title,
                image_views.size());
            return nullptr;
        }

        static thread_local vk::ImageView view;
        view = image_views[context->current_image_index];
        return static_cast<void*>(&view);
    };

    display_service->get_current_swapchain_image = [this](const std::shared_ptr<void>& window_ptr) -> uint64_t {
        auto window = std::static_pointer_cast<Window>(window_ptr);
        auto* ctx = find_window_context(window);
        if (!ctx || !ctx->swapchain)
            return 0;

        const auto& images = ctx->swapchain->get_images();
        if (ctx->current_image_index >= images.size())
            return 0;

        return reinterpret_cast<uint64_t>(static_cast<VkImage>(images[ctx->current_image_index]));
    };
}

WindowRenderContext* BackendWindowHandler::find_window_context(const std::shared_ptr<Window>& window)
{
    auto it = std::ranges::find_if(m_window_contexts,
        [window](const auto& config) { return config.window == window; });
    return it != m_window_contexts.end() ? &(*it) : nullptr;
}

const WindowRenderContext* BackendWindowHandler::find_window_context(const std::shared_ptr<Window>& window) const
{
    auto it = std::ranges::find_if(m_window_contexts,
        [window](const auto& config) { return config.window == window; });
    return it != m_window_contexts.end() ? &(*it) : nullptr;
}

bool BackendWindowHandler::register_window(const std::shared_ptr<Window>& window)
{
    if (window->is_graphics_registered() || find_window_context(window)) {
        return false;
    }

    vk::SurfaceKHR surface = m_context.create_surface(window);
    if (!surface) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
            "Failed to create Vulkan surface for window '{}'",
            window->get_create_info().title);
        return false;
    }

    if (!m_context.update_present_family(surface)) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
            "No presentation support for window '{}'", window->get_create_info().title);
        m_context.destroy_surface(surface);
        return false;
    }

    WindowRenderContext config;
    config.window = window;
    config.surface = surface;
    config.swapchain = std::make_unique<VKSwapchain>();

    if (!config.swapchain->create(m_context, surface, window->get_create_info())) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
            "Failed to create swapchain for window '{}'", window->get_create_info().title);
        return false;
    }

    if (!create_sync_objects(config)) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
            "Failed to create sync objects for window '{}'",
            window->get_create_info().title);
        config.swapchain->cleanup();
        m_context.destroy_surface(surface);
        return false;
    }

    m_window_contexts.emplace_back(std::move(config));
    window->set_graphics_registered(true);

    window->set_event_callback([this, window_ptr = window](const WindowEvent& event) {
        if (event.type == WindowEventType::WINDOW_RESIZED) {
            auto* config = find_window_context(window_ptr);
            if (config) {
                config->needs_recreation = true;
            }
        }
    });

    MF_INFO(Journal::Component::Core, Journal::Context::GraphicsSubsystem,
        "Registered window '{}' for graphics processing", window->get_create_info().title);

    return true;
}

bool BackendWindowHandler::create_sync_objects(WindowRenderContext& config)
{
    auto device = m_context.get_device();
    uint32_t image_count = config.swapchain->get_image_count();

    try {
        config.image_available.resize(image_count);
        config.render_finished.resize(image_count);
        config.in_flight.resize(image_count);
        config.clear_command_buffers.resize(image_count);

        vk::SemaphoreCreateInfo semaphore_info {};
        vk::FenceCreateInfo fence_info {};
        fence_info.flags = vk::FenceCreateFlagBits::eSignaled;

        for (uint32_t i = 0; i < image_count; ++i) {
            config.image_available[i] = device.createSemaphore(semaphore_info);
            config.render_finished[i] = device.createSemaphore(semaphore_info);

            config.clear_command_buffers[i] = m_command_manager.allocate_command_buffer(
                vk::CommandBufferLevel::ePrimary);
        }

        for (auto& i : config.in_flight) {
            i = device.createFence(fence_info);
        }

        config.current_frame = 0;

        return true;
    } catch (const vk::SystemError& e) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
            "Failed to create sync objects: {}", e.what());
        return false;
    }
}

void BackendWindowHandler::recreate_swapchain_for_context(WindowRenderContext& context)
{
    m_context.wait_idle();

    if (recreate_swapchain_internal(context)) {
        context.needs_recreation = false;
        MF_RT_WARN(Journal::Component::Core, Journal::Context::GraphicsCallback,
            "Recreated swapchain for window '{}' ({}x{})",
            context.window->get_create_info().title,
            context.window->get_state().current_width,
            context.window->get_state().current_height);
    }
}

bool BackendWindowHandler::recreate_swapchain_internal(WindowRenderContext& context)
{
    const auto& state = context.window->get_state();

    if (!context.swapchain->recreate(state.current_width, state.current_height)) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
            "Failed to recreate swapchain for window '{}'",
            context.window->get_create_info().title);
        return false;
    }

    return true;
}

void BackendWindowHandler::render_window(const std::shared_ptr<Window>& window)
{
    auto ctx = find_window_context(window);
    if (!ctx) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
            "Window '{}' not registered for rendering",
            window->get_create_info().title);
        return;
    }

    if (ctx->window && ctx->window->get_rendering_buffers().empty()) {
        render_empty_window(*ctx);
    }
}

void BackendWindowHandler::render_all_windows()
{
    MF_RT_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "render_all_windows() is not implemented in BackendWindowHandler. Dynamic rendering is expected to be handled externally.");
}

void BackendWindowHandler::render_empty_window(WindowRenderContext& ctx)
{
    auto window = ctx.window;

    if (!window || !window->get_state().is_visible || !window->get_rendering_buffers().empty()) {
        return;
    }

    try {
        auto device = m_context.get_device();

        size_t frame_index = ctx.current_frame;
        auto& in_flight = ctx.in_flight[frame_index];
        auto& image_available = ctx.image_available[frame_index];
        auto& render_finished = ctx.render_finished[frame_index];

        auto cmd_buffer = ctx.clear_command_buffers[frame_index];
        if (!cmd_buffer) {
            MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
                "Clear command buffer not allocated for window '{}'",
                window->get_create_info().title);
            return;
        }
        cmd_buffer.reset({});

        if (device.waitForFences(1, &in_flight, VK_TRUE, UINT64_MAX) == vk::Result::eTimeout) {
            return;
        }

        auto image_index_opt = ctx.swapchain->acquire_next_image(image_available);
        if (!image_index_opt.has_value()) {
            ctx.needs_recreation = true;
            return;
        }
        ctx.current_image_index = image_index_opt.value();

        if (device.resetFences(1, &in_flight) != vk::Result::eSuccess) {
            return;
        }

        vk::CommandBufferBeginInfo begin_info {};
        begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
        cmd_buffer.begin(begin_info);

        const auto& image_views = ctx.swapchain->get_image_views();
        auto extent = ctx.swapchain->get_extent();

        vk::RenderingAttachmentInfo color_attachment {};
        color_attachment.imageView = image_views[ctx.current_image_index];
        color_attachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
        color_attachment.loadOp = vk::AttachmentLoadOp::eClear;
        color_attachment.storeOp = vk::AttachmentStoreOp::eStore;
        color_attachment.clearValue.color = vk::ClearColorValue(
            window->get_create_info().clear_color);

        vk::RenderingInfo rendering_info {};
        rendering_info.renderArea = vk::Rect2D { { 0, 0 }, extent };
        rendering_info.layerCount = 1;
        rendering_info.colorAttachmentCount = 1;
        rendering_info.pColorAttachments = &color_attachment;

        vk::ImageMemoryBarrier barrier {};
        barrier.oldLayout = vk::ImageLayout::eUndefined;
        barrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = ctx.swapchain->get_images()[ctx.current_image_index];
        barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

        cmd_buffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eTopOfPipe,
            vk::PipelineStageFlagBits::eColorAttachmentOutput,
            {}, {}, {}, barrier);

        cmd_buffer.beginRendering(rendering_info);
        cmd_buffer.endRendering();

        barrier.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
        barrier.newLayout = vk::ImageLayout::ePresentSrcKHR;
        barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
        barrier.dstAccessMask = {};

        cmd_buffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eColorAttachmentOutput,
            vk::PipelineStageFlagBits::eBottomOfPipe,
            {}, {}, {}, barrier);

        cmd_buffer.end();

        submit_and_present(window, cmd_buffer);

        ctx.current_frame = (ctx.current_frame + 1) % ctx.in_flight.size();

    } catch (const std::exception& e) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
            "Failed to render empty window '{}': {}",
            window->get_create_info().title, e.what());
    }
}

void BackendWindowHandler::submit_and_present(
    const std::shared_ptr<Window>& window,
    const vk::CommandBuffer& command_buffer)
{
    auto* ctx = find_window_context(window);
    if (!ctx) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
            "Window not registered for submit_and_present");
        return;
    }

    auto graphics_queue = m_context.get_graphics_queue();

    size_t frame_index = ctx->current_frame;
    auto& in_flight = ctx->in_flight[frame_index];
    auto& image_available = ctx->image_available[frame_index];
    auto& render_finished = ctx->render_finished[frame_index];

    vk::SubmitInfo submit_info {};
    vk::PipelineStageFlags wait_stages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &image_available;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &render_finished;

    try {
        auto result = graphics_queue.submit(1, &submit_info, in_flight);
    } catch (const vk::SystemError& e) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
            "Failed to submit primary command buffer: {}", e.what());
        return;
    }

    bool present_success = ctx->swapchain->present(
        ctx->current_image_index, render_finished, graphics_queue);

    if (!present_success) {
        ctx->needs_recreation = true;
    }

    ctx->current_frame = (frame_index + 1) % ctx->in_flight.size();

    MF_RT_TRACE(Journal::Component::Core, Journal::Context::GraphicsCallback,
        "Window '{}': frame submitted and presented",
        window->get_create_info().title);
}

bool BackendWindowHandler::is_window_registered(const std::shared_ptr<Window>& window) const
{
    return find_window_context(window) != nullptr;
}

void BackendWindowHandler::unregister_window(const std::shared_ptr<Window>& window)
{
    auto it = m_window_contexts.begin();
    while (it != m_window_contexts.end()) {
        if (it->window == window) {
            it->cleanup(m_context);
            MF_INFO(Journal::Component::Core, Journal::Context::GraphicsSubsystem,
                "Unregistered window '{}'", it->window->get_create_info().title);
            it = m_window_contexts.erase(it);
            return;
        }
        ++it;
    }
}

void BackendWindowHandler::handle_window_resize()
{
    for (auto& context : m_window_contexts) {
        if (context.needs_recreation) {
            recreate_swapchain_for_context(context);
        }
    }
}

void BackendWindowHandler::cleanup()
{
    for (auto& config : m_window_contexts) {
        config.cleanup(m_context);
    }
    m_window_contexts.clear();
}

}
