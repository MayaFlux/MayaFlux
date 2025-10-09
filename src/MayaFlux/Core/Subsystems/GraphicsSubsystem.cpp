#include "GraphicsSubsystem.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VulkanBackend.hpp"
#include "MayaFlux/Core/Backends/Windowing/Window.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

#include "MayaFlux/Vruta/Clock.hpp"
#include "MayaFlux/Vruta/Routine.hpp"

namespace MayaFlux::Core {

std::unique_ptr<IGraphicsBackend> create_graphics_backend(GlobalGraphicsConfig::GraphicsApi api)
{
    switch (api) {
    case GlobalGraphicsConfig::GraphicsApi::VULKAN:
        return std::make_unique<VulkanBackend>();
    case GlobalGraphicsConfig::GraphicsApi::OPENGL:
    case GlobalGraphicsConfig::GraphicsApi::METAL:
    case GlobalGraphicsConfig::GraphicsApi::DIRECTX12:
    default:
        return nullptr;
    }
}

GraphicsSubsystem::GraphicsSubsystem(const GlobalGraphicsConfig& graphics_config)
    : m_backend(create_graphics_backend(graphics_config.requested_api))
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

    if (!m_backend->initialize(m_graphics_config)) {
        error<std::runtime_error>(
            Journal::Component::Core,
            Journal::Context::GraphicsSubsystem,
            std::source_location::current(),
            "No graphics backend available");
    }

    if (m_graphics_config.target_frame_rate > 0) {
        m_frame_clock->set_target_fps(m_graphics_config.target_frame_rate);
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
            error_rethrow(
                Journal::Component::Core,
                Journal::Context::GraphicsSubsystem,
                std::source_location::current(),
                "Graphics thread error");
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

    m_backend->cleanup();

    for (auto& window : m_registered_windows) {
        window->set_graphics_registered(false);
    }
    m_registered_windows.clear();

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
    m_backend->handle_window_resize();

    render_all_windows();
    m_handle->windows.process();

    cleanup_closed_windows();
    for (auto& [name, hook] : m_handle->post_process_hooks) {
        hook(1);
    }
}

void GraphicsSubsystem::register_windows_for_processing()
{
    for (auto& window : m_handle->windows.get_processing_windows()) {

        if (window->is_graphics_registered()) {
            continue;
        }

        if (m_backend->register_window(window)) {
            m_registered_windows.push_back(window);
        } else {
            MF_WARN(Journal::Component::Core, Journal::Context::GraphicsSubsystem,
                "Failed to register window '{}' for graphics processing",
                window->get_create_info().title);
            continue;
        }
    }
}

void GraphicsSubsystem::cleanup_closed_windows()
{
    for (auto& window : m_registered_windows) {
        if (window->should_close() && window->is_graphics_registered()) {
            m_backend->unregister_window(window);
            window->set_graphics_registered(false);
        }
    }

    m_registered_windows.erase(
        std::remove_if(
            m_registered_windows.begin(), m_registered_windows.end(),
            [](const std::shared_ptr<Window>& win) { return win->should_close(); }),
        m_registered_windows.end());
}

void GraphicsSubsystem::render_all_windows()
{
    for (auto& window : m_registered_windows) {
        m_backend->render_window(window);
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
