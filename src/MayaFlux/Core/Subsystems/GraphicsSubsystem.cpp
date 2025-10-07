#include "GraphicsSubsystem.hpp"
#include "MayaFlux/Core/Backends/Graphics/VKCommandManager.hpp"
#include "MayaFlux/Core/Backends/Graphics/VKRenderPass.hpp"
#include "MayaFlux/Core/Backends/Graphics/VKSwapchain.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#include "MayaFlux/Core/Backends/Windowing/Window.hpp"

#include "MayaFlux/Vruta/Clock.hpp"
#include "MayaFlux/Vruta/Routine.hpp"

namespace MayaFlux::Core {

void WindowSwapchainConfig::cleanup(VKContext& context)
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

    if (render_pass) {
        render_pass->cleanup(device);
        render_pass.reset();
    }

    window->set_graphics_registered(false);
}

GraphicsSubsystem::GraphicsSubsystem(const GlobalGraphicsConfig& graphics_config)
    : m_vulkan_context(std::make_unique<VKContext>())
    , m_frame_clock(std::make_shared<Vruta::FrameClock>(60))
    , m_subsystem_tokens {
        .Buffer = Buffers::ProcessingToken::GRAPHICS_BACKEND,
        .Node = Nodes::ProcessingToken::VISUAL_RATE,
        .Task = Vruta::ProcessingToken::FRAME_ACCURATE
    }
    , m_graphics_config(graphics_config)
{
    m_command_manager = std::make_unique<VKCommandManager>();
}

GraphicsSubsystem::~GraphicsSubsystem()
{
    stop();
}

void GraphicsSubsystem::initialize(SubsystemProcessingHandle& handle)
{
    MF_INFO(Journal::Component::Core, Journal::Context::GraphicsSubsystem,
        "Initializing Graphics Subsystem...");

    m_handle = &handle;

    if (!m_vulkan_context->initialize(m_graphics_config, true)) {
        error<std::runtime_error>(
            Journal::Component::Core,
            Journal::Context::GraphicsSubsystem,
            std::source_location::current(),
            "Failed to initialize Vulkan context!");
    }

    if (m_graphics_config.target_frame_rate > 0) {
        m_frame_clock->set_target_fps(m_graphics_config.target_frame_rate);
    }

    if (!m_command_manager->initialize(
            m_vulkan_context->get_device(),
            m_vulkan_context->get_queue_families().graphics_family.value())) {
        error<std::runtime_error>(
            Journal::Component::Core,
            Journal::Context::GraphicsSubsystem,
            std::source_location::current(),
            "Failed to initialize command manager!");
    }

    m_is_ready = true;

    MF_INFO(Journal::Component::Core, Journal::Context::GraphicsSubsystem,
        "Graphics Subsystem initialized (Target FPS: {})",
        m_frame_clock->frame_rate());
}

void GraphicsSubsystem::register_frame_processor()
{
    if (!m_handle) {
        error<std::runtime_error>(
            Journal::Component::Core,
            Journal::Context::GraphicsSubsystem,
            std::source_location::current(),
            "Cannot register frame processor: no processing handle");
    }

    auto scheduler = m_handle->tasks;
    if (!scheduler.is_valid()) {
        error<std::runtime_error>(
            Journal::Component::Core,
            Journal::Context::GraphicsSubsystem,
            std::source_location::current(),
            "Cannot register frame processor: no scheduler available");
    }

    scheduler.register_token_processor(
        [this](const std::vector<std::shared_ptr<Vruta::Routine>>& tasks, uint64_t processing_units) {
            this->process_frame_coroutines_impl(tasks, processing_units);
        });

    MF_INFO(Journal::Component::Core, Journal::Context::GraphicsSubsystem,
        "Registered custom FRAME_ACCURATE processor");
}

void GraphicsSubsystem::register_callbacks()
{
    if (!m_is_ready) {
        error<std::runtime_error>(
            Journal::Component::Core,
            Journal::Context::GraphicsSubsystem,
            std::source_location::current(),
            "Subsystem is not initialized. Please initialize before registering callbacks.");
    }

    register_frame_processor();

    MF_INFO(Journal::Component::Core, Journal::Context::GraphicsSubsystem,
        "Graphics callbacks registered (self-driven mode)");
}

void GraphicsSubsystem::process_frame_coroutines_impl(
    const std::vector<std::shared_ptr<Vruta::Routine>>& tasks,
    uint64_t processing_units)
{
    if (tasks.empty()) {
        return;
    }

    uint64_t current_frame = m_frame_clock->current_position();

    if (processing_units == 0) {
        processing_units = 1;
    }

    for (uint64_t i = 0; i < processing_units; ++i) {
        uint64_t frame_to_process = current_frame + i;

        for (auto& routine : tasks) {
            if (!routine || !routine->is_active()) {
                continue;
            }

            if (routine->requires_clock_sync()) {
                if (frame_to_process >= routine->next_execution()) {
                    routine->try_resume(frame_to_process);
                }
            } else {
                routine->try_resume(frame_to_process);
            }
        }
    }
}

void GraphicsSubsystem::start()
{
    if (m_running.load(std::memory_order_acquire)) {
        MF_WARN(Journal::Component::Core, Journal::Context::GraphicsSubsystem,
            "Graphics thread already running!");
        return;
    }

    m_running.store(true);
    m_paused.store(false, std::memory_order_release);

    m_frame_clock->reset();

    m_graphics_thread = std::thread([this]() {
        m_graphics_thread_id = std::this_thread::get_id();

        MF_INFO(Journal::Component::Core, Journal::Context::GraphicsSubsystem,
            "Graphics thread started (ID: {}, Target FPS: {})",
            std::hash<std::thread::id> {}(m_graphics_thread_id),
            m_frame_clock->frame_rate());

        try {
            graphics_thread_loop();
        } catch (const std::exception& e) {
            error<std::runtime_error>(
                Journal::Component::Core,
                Journal::Context::GraphicsSubsystem,
                std::source_location::current(),
                "Graphics thread error: {}", e.what());
        }

        MF_INFO(Journal::Component::Core, Journal::Context::GraphicsSubsystem,
            "Graphics thread stopped.");
    });
}

void GraphicsSubsystem::stop()
{
    if (!m_running.load(std::memory_order_acquire)) {
        return;
    }

    MF_INFO(Journal::Component::Core, Journal::Context::GraphicsSubsystem,
        "Stopping Graphics Subsystem...");

    m_running.store(false, std::memory_order_release);

    if (m_graphics_thread.joinable()) {
        m_graphics_thread.join();
    }

    for (auto& config : m_window_configs) {
        config.cleanup(*m_vulkan_context);
    }

    m_vulkan_context->wait_idle();

    if (m_command_manager) {
        m_command_manager->cleanup();
    }

    m_vulkan_context->cleanup();

    MF_INFO(Journal::Component::Core, Journal::Context::GraphicsSubsystem,
        "Graphics Subsystem stopped.");
}

void GraphicsSubsystem::pause()
{
    if (!m_running.load(std::memory_order_acquire)) {
        MF_WARN(Journal::Component::Core, Journal::Context::GraphicsSubsystem,
            "Cannot pause - graphics thread not running");
        return;
    }

    m_paused.store(true, std::memory_order_release);

    MF_INFO(Journal::Component::Core, Journal::Context::GraphicsSubsystem,
        "Graphics processing paused");
}

void GraphicsSubsystem::resume()
{
    if (!m_running.load(std::memory_order_acquire)) {
        MF_WARN(Journal::Component::Core, Journal::Context::GraphicsSubsystem,
            "Cannot resume - graphics thread not running");
        return;
    }

    m_paused.store(false, std::memory_order_release);

    MF_INFO(Journal::Component::Core, Journal::Context::GraphicsSubsystem,
        "Graphics processing resumed");
}

uint32_t GraphicsSubsystem::get_target_fps() const
{
    return m_frame_clock->frame_rate();
}

double GraphicsSubsystem::get_measured_fps() const
{
    return m_frame_clock->get_measured_fps();
}

void GraphicsSubsystem::set_target_fps(uint32_t fps)
{
    m_frame_clock->set_target_fps(fps);

    MF_INFO(Journal::Component::Core, Journal::Context::GraphicsSubsystem,
        "Target FPS updated to {}", fps);
}

void GraphicsSubsystem::process()
{
    if (!m_handle) {
        return;
    }

    for (auto& [name, hook] : m_handle->pre_process_hooks) {
        hook(1);
    }

    m_handle->tasks.process(1);

    m_handle->nodes.process(1);

    m_handle->buffers.process(1);

    register_windows_for_processing();
    handle_window_resizes();

    m_handle->windows.process();

    render_all_windows();

    cleanup_closed_windows();
    for (auto& [name, hook] : m_handle->post_process_hooks) {
        hook(1);
    }
}

void GraphicsSubsystem::register_windows_for_processing()
{
    for (auto& window : m_handle->windows.get_processing_windows()) {

        if (window->is_graphics_registered() || find_config(window)) {
            continue;
        }

        vk::SurfaceKHR surface = m_vulkan_context->create_surface(window);
        if (!surface) {
            MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsSubsystem,
                "Failed to create Vulkan surface for window '{}'",
                window->get_create_info().title);
            continue;
        }

        if (!m_vulkan_context->update_present_family(surface)) {
            MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsSubsystem,
                "No presentation support for window '{}'", window->get_create_info().title);
            m_vulkan_context->destroy_surface(surface);
            continue;
        }

        WindowSwapchainConfig config;
        config.window = window;
        config.surface = surface;
        config.swapchain = std::make_unique<VKSwapchain>();

        if (!config.swapchain->create(*m_vulkan_context, surface, window->get_create_info())) {
            MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsSubsystem,
                "Failed to create swapchain for window '{}'", window->get_create_info().title);
            continue;
        }

        config.render_pass = std::make_unique<VKRenderPass>();
        if (!config.render_pass->create(
                m_vulkan_context->get_device(),
                config.swapchain->get_image_format())) {

            MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsSubsystem,
                "Failed to create render pass for window '{}'", window->get_create_info().title);

            config.swapchain->cleanup();
            m_vulkan_context->destroy_surface(surface);
            continue;
        }

        if (!create_sync_objects(config)) {
            MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsSubsystem,
                "Failed to create sync objects for window '{}'",
                window->get_create_info().title);
            config.swapchain->cleanup();
            m_vulkan_context->destroy_surface(surface);
            continue;
        }

        m_window_configs.push_back(std::move(config));
        window->set_graphics_registered(true);

        window->set_event_callback([this, window_ptr = window](const WindowEvent& event) {
            if (event.type == WindowEventType::WINDOW_RESIZED) {
                auto* config = find_config(window_ptr);
                if (config) {
                    config->needs_recreation = true;
                }
            }
        });

        MF_INFO(Journal::Component::Core, Journal::Context::GraphicsSubsystem,
            "Registered window '{}' for graphics processing", window->get_create_info().title);
    }
}

bool GraphicsSubsystem::create_sync_objects(WindowSwapchainConfig& config)
{
    auto device = m_vulkan_context->get_device();
    uint32_t image_count = config.swapchain->get_image_count();

    try {
        config.image_available.resize(image_count);
        config.render_finished.resize(image_count);
        config.in_flight.resize(image_count);

        vk::SemaphoreCreateInfo semaphore_info {};
        vk::FenceCreateInfo fence_info {};
        fence_info.flags = vk::FenceCreateFlagBits::eSignaled;

        for (uint32_t i = 0; i < image_count; ++i) {
            config.image_available[i] = device.createSemaphore(semaphore_info);
            config.render_finished[i] = device.createSemaphore(semaphore_info);
        }

        for (auto& i : config.in_flight) {
            i = device.createFence(fence_info);
        }

        config.current_frame = 0;

        return true;
    } catch (const vk::SystemError& e) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to create sync objects: {}", e.what());
        return false;
    }
}

void GraphicsSubsystem::handle_window_resizes()
{
    for (auto& config : m_window_configs) {
        if (config.needs_recreation) {
            const auto& state = config.window->get_state();

            m_vulkan_context->wait_idle();

            if (!config.swapchain->recreate(state.current_width, state.current_height)) {
                MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsSubsystem,
                    "Failed to recreate swapchain for window '{}'",
                    config.window->get_create_info().title);
            } else {
                config.needs_recreation = false;
            }
        }
    }
}

void GraphicsSubsystem::cleanup_closed_windows()
{
    auto it = m_window_configs.begin();
    while (it != m_window_configs.end()) {
        if (it->window->should_close()) {
            it->cleanup(*m_vulkan_context);

            MF_INFO(Journal::Component::Core, Journal::Context::GraphicsSubsystem,
                "Unregistered window '{}'", it->window->get_create_info().title);

            it = m_window_configs.erase(it);
        } else {
            ++it;
        }
    }
}

void GraphicsSubsystem::render_all_windows()
{
    for (auto& config : m_window_configs) {
        render_window(config);
    }
}

void GraphicsSubsystem::record_clear_commands(
    vk::CommandBuffer cmd,
    vk::Image image,
    vk::Extent2D /*extent*/)
{
    vk::ImageMemoryBarrier barrier {};
    barrier.oldLayout = vk::ImageLayout::eUndefined;
    barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = vk::AccessFlags {};
    barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

    cmd.pipelineBarrier(
        vk::PipelineStageFlagBits::eTopOfPipe,
        vk::PipelineStageFlagBits::eTransfer,
        vk::DependencyFlags {},
        0, nullptr,
        0, nullptr,
        1, &barrier);

    vk::ClearColorValue clear_color {};
    clear_color.float32[0] = 0.0F;
    clear_color.float32[1] = 0.1F;
    clear_color.float32[2] = 0.2F;
    clear_color.float32[3] = 1.0F;

    vk::ImageSubresourceRange range {};
    range.aspectMask = vk::ImageAspectFlagBits::eColor;
    range.baseMipLevel = 0;
    range.levelCount = 1;
    range.baseArrayLayer = 0;
    range.layerCount = 1;

    cmd.clearColorImage(
        image,
        vk::ImageLayout::eTransferDstOptimal,
        &clear_color,
        1, &range);

    barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    barrier.newLayout = vk::ImageLayout::ePresentSrcKHR;
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlags {};

    cmd.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eBottomOfPipe,
        vk::DependencyFlags {},
        0, nullptr,
        0, nullptr,
        1, &barrier);
}

void GraphicsSubsystem::render_window(WindowSwapchainConfig& config)
{
    auto device = m_vulkan_context->get_device();
    auto graphics_queue = m_vulkan_context->get_graphics_queue();

    size_t frame_index = config.current_frame;
    auto& in_flight = config.in_flight[frame_index];

    device.waitForFences(1, &in_flight, VK_TRUE, UINT64_MAX);
    device.resetFences(1, &in_flight);

    auto image_index_opt = config.swapchain->acquire_next_image(config.image_available[frame_index]);
    if (!image_index_opt.has_value()) {
        config.needs_recreation = true;
        return;
    }
    uint32_t image_index = image_index_opt.value();

    auto& image_available = config.image_available[frame_index];
    auto& render_finished = config.render_finished[frame_index];

    vk::CommandBuffer cmd = m_command_manager->allocate_command_buffer();
    vk::CommandBufferBeginInfo begin_info {};
    begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    cmd.begin(begin_info);

    // --- INTEGRATION POINT ---
    // vk::RenderPassBeginInfo render_pass_info {};
    // render_pass_info.renderPass = config.render_pass->get();
    // render_pass_info.framebuffer = config.framebuffers[image_index];
    // render_pass_info.renderArea.offset = {0, 0};
    // render_pass_info.renderArea.extent = config.swapchain->get_extent();
    // vk::ClearValue clear_value;
    // clear_value.color = vk::ClearColorValue{...};
    // render_pass_info.clearValueCount = 1;
    // render_pass_info.pClearValues = &clear_value;
    // cmd.beginRenderPass(render_pass_info, vk::SubpassContents::eInline);
    // // ...draw calls here...
    // cmd.endRenderPass();

    record_clear_commands(cmd, config.swapchain->get_images()[image_index], config.swapchain->get_extent());
    cmd.end();

    vk::SubmitInfo submit_info {};
    vk::PipelineStageFlags wait_stages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &image_available;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &render_finished;

    try {
        graphics_queue.submit(1, &submit_info, in_flight);
    } catch (const vk::SystemError& e) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to submit draw command buffer: {}", e.what());
        m_command_manager->free_command_buffer(cmd);
        return;
    }

    bool present_success = config.swapchain->present(image_index, render_finished, graphics_queue);
    if (!present_success) {
        config.needs_recreation = true;
    }

    config.current_frame = (frame_index + 1) % config.in_flight.size();
}

void GraphicsSubsystem::graphics_thread_loop()
{
    while (m_running.load(std::memory_order_acquire)) {
        if (m_paused.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }

        m_frame_clock->tick();

        process();

        m_frame_clock->wait_for_next_frame();

        if (m_frame_clock->is_frame_late()) {
            uint64_t lag = m_frame_clock->get_frame_lag();
            if (lag > 2) {
                MF_RT_WARN(Journal::Component::Core, Journal::Context::GraphicsSubsystem,
                    "Frame lag detected: {} frames behind (Measured FPS: {:.1f})",
                    lag, m_frame_clock->get_measured_fps());
            }
        }
    }
}

void GraphicsSubsystem::shutdown()
{
    stop();
    m_is_ready = false;
}

}
