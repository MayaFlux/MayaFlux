#include "GraphicsSubsystem.hpp"
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
    // register_callbacks();

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
        [this](const std::vector<std::shared_ptr<Vruta::Routine>>& tasks, u_int64_t processing_units) {
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
    u_int64_t processing_units)
{
    if (tasks.empty()) {
        return;
    }

    u_int64_t current_frame = m_frame_clock->current_position();

    if (processing_units == 0) {
        processing_units = 1;
    }

    for (u_int64_t i = 0; i < processing_units; ++i) {
        u_int64_t frame_to_process = current_frame + i;

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

    cleanup_closed_windows();
    register_windows_for_processing();
    handle_window_resizes();

    m_handle->windows.process();

    // TODO: Need to consider if specific synchronization is needed here

    // TODO Phase 1.4+: Record Vulkan commands
    // auto cmd = m_cmd_manager.get_current_command_buffer();
    // vkBeginCommandBuffer(cmd, ...);
    // record_render_commands(cmd);
    // vkEndCommandBuffer(cmd);
    // vkQueueSubmit(...);

    // TODO Phase 1.3+: Present to swapchain
    // m_swapchain.present(image_index, ...);

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

        WindowSwapchainConfig config;
        config.window = window;
        config.surface = surface;
        config.swapchain = std::make_unique<VKSwapchain>();

        if (!config.swapchain->create(*m_vulkan_context, surface, window->get_create_info())) {
            MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsSubsystem,
                "Failed to create swapchain for window '{}'", window->get_create_info().title);
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
    u_int32_t frames_in_flight = config.swapchain->get_image_count();

    try {
        config.image_available.resize(frames_in_flight);
        config.render_finished.resize(frames_in_flight);
        config.in_flight.resize(frames_in_flight);

        vk::SemaphoreCreateInfo semaphore_info {};
        vk::FenceCreateInfo fence_info {};
        fence_info.flags = vk::FenceCreateFlagBits::eSignaled;

        for (uint32_t i = 0; i < frames_in_flight; ++i) {
            config.image_available[i] = device.createSemaphore(semaphore_info);
            config.render_finished[i] = device.createSemaphore(semaphore_info);
            config.in_flight[i] = device.createFence(fence_info);
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

void GraphicsSubsystem::graphics_thread_loop()
{
    while (m_running.load(std::memory_order_acquire)) {
        if (m_paused.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }

        m_frame_clock->tick();

        register_windows_for_processing();

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
