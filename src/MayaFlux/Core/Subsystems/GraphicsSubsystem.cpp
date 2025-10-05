#include "GraphicsSubsystem.hpp"
#include "MayaFlux/Core/Backends/Graphics/VKContext.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

#include "MayaFlux/Vruta/Clock.hpp"
#include "MayaFlux/Vruta/Routine.hpp"

namespace MayaFlux::Core {

GraphicsSubsystem::GraphicsSubsystem()
    : m_vulkan_context(std::make_unique<VKContext>())
    , m_frame_clock(std::make_shared<Vruta::FrameClock>(60))
    , m_subsystem_tokens {
        .Buffer = Buffers::ProcessingToken::GRAPHICS_BACKEND,
        .Node = Nodes::ProcessingToken::VISUAL_RATE,
        .Task = Vruta::ProcessingToken::FRAME_ACCURATE
    }
{
}

GraphicsSubsystem::~GraphicsSubsystem()
{
    stop();
}

void GraphicsSubsystem::initialize(SubsystemProcessingHandle& handle, const GraphicsSurfaceInfo& surface_info)
{
    MF_INFO(Journal::Component::Core, Journal::Context::GraphicsSubsystem,
        "Initializing Graphics Subsystem...");

    m_handle = &handle;
    m_graphics_info = surface_info;

    if (!m_vulkan_context->initialize(m_graphics_info, true)) {
        error<std::runtime_error>(
            Journal::Component::Core,
            Journal::Context::GraphicsSubsystem,
            std::source_location::current(),
            "Failed to initialize Vulkan context!");
    }

    if (surface_info.target_frame_rate > 0) {
        m_frame_clock->set_target_fps(surface_info.target_frame_rate);
    }

    register_callbacks();

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
    return register_frame_processor();

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

void GraphicsSubsystem::graphics_thread_loop()
{
    while (m_running.load(std::memory_order_acquire)) {
        if (m_paused.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }

        m_frame_clock->tick();

        process();

        // uint64_t current_frame = m_frame_clock->current_frame();

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

}
