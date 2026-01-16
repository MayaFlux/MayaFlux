#include "Engine.hpp"

#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Core/Windowing/WindowManager.hpp"
#include "MayaFlux/Nodes/Generators/Stochastic.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"
#include "MayaFlux/Vruta/EventManager.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#ifdef MAYAFLUX_PLATFORM_MACOS
#include <CoreFoundation/CoreFoundation.h>
#include <unistd.h>
#endif

namespace MayaFlux::Core {

//-------------------------------------------------------------------------
// Initialization and Lifecycle
//-------------------------------------------------------------------------

Engine::Engine()
    : m_rng(new Nodes::Generator::Stochastics::Random())
{
}

Engine::~Engine()
{
    End();
}

Engine::Engine(Engine&& other) noexcept
    : m_stream_info(other.m_stream_info)
    , m_graphics_config(other.m_graphics_config)
    , m_is_paused(other.m_is_paused)
    , m_is_initialized(other.m_is_initialized)
    , m_should_shutdown(other.m_should_shutdown.load())
    , m_scheduler(std::move(other.m_scheduler))
    , m_node_graph_manager(std::move(other.m_node_graph_manager))
    , m_buffer_manager(std::move(other.m_buffer_manager))
    , m_subsystem_manager(std::move(other.m_subsystem_manager))
    , m_window_manager(std::move(other.m_window_manager))
    , m_event_manager(std::move(other.m_event_manager))
    , m_rng(std::move(other.m_rng))
{
    other.m_is_initialized = false;
    other.m_is_paused = false;
}

Engine& Engine::operator=(Engine&& other) noexcept
{
    if (this != &other) {
        End();

        m_stream_info = other.m_stream_info;
        m_graphics_config = other.m_graphics_config;

        m_subsystem_manager = std::move(other.m_subsystem_manager);
        m_node_graph_manager = std::move(other.m_node_graph_manager);
        m_buffer_manager = std::move(other.m_buffer_manager);
        m_scheduler = std::move(other.m_scheduler);
        m_window_manager = std::move(other.m_window_manager);
        m_event_manager = std::move(other.m_event_manager);
        m_rng = std::move(other.m_rng);

        m_is_initialized = other.m_is_initialized;
        m_is_paused = other.m_is_paused;
        m_should_shutdown = other.m_should_shutdown.load();

        other.m_is_initialized = false;
        other.m_is_paused = false;
    }
    return *this;
}

void Engine::Init()
{
    Init(m_stream_info, m_graphics_config);
}

void Engine::Init(const GlobalStreamInfo& streamInfo)
{
    Init(streamInfo, m_graphics_config);
}

void Engine::Init(const GlobalStreamInfo& streamInfo, const GlobalGraphicsConfig& graphics_config)
{
    MF_PRINT(Journal::Component::Core, Journal::Context::Init, "Engine initializing");
    m_stream_info = streamInfo;
    m_graphics_config = graphics_config;

    m_scheduler = std::make_shared<Vruta::TaskScheduler>(m_stream_info.sample_rate);
    m_event_manager = std::make_shared<Vruta::EventManager>();

    m_buffer_manager = std::make_shared<Buffers::BufferManager>(
        m_stream_info.output.channels,
        m_stream_info.input.enabled ? m_stream_info.input.channels : 0,
        m_stream_info.buffer_size);

    m_node_graph_manager = std::make_shared<Nodes::NodeGraphManager>();

    if (m_graphics_config.windowing_backend != GlobalGraphicsConfig::WindowingBackend::NONE) {
        m_window_manager = std::make_shared<WindowManager>(m_graphics_config);
    } else {
        MF_WARN(Journal::Component::Core, Journal::Context::Init, "No windowing backend selected - running in audio-only mode");
    }

    m_subsystem_manager = std::make_shared<SubsystemManager>(
        m_node_graph_manager, m_buffer_manager, m_scheduler, m_window_manager);

    m_subsystem_manager->create_audio_subsystem(m_stream_info, Utils::AudioBackendType::RTAUDIO);

    m_subsystem_manager->create_graphics_subsystem(m_graphics_config);

    m_buffer_manager->initialize_buffer_service();

    m_is_initialized = true;

    MF_PRINT(Journal::Component::Core, Journal::Context::Init, "Audio backend: RtAudio, Sample rate: {}", m_stream_info.sample_rate);
}

void Engine::Start()
{
    if (!m_is_initialized) {
        Init();
    }
    m_subsystem_manager->start_all_subsystems();
}

void Engine::Pause()
{
    if (m_is_paused || !m_is_initialized) {
        return;
    }

    m_subsystem_manager->pause_all_subsystems();
    m_is_paused = true;
}

void Engine::Resume()
{
    if (!m_is_paused || !m_is_initialized) {
        return;
    }

    m_subsystem_manager->resume_all_subsystems();
    m_is_paused = false;
}

bool Engine::is_running() const
{
    if (!m_is_initialized || m_is_paused) {
        return false;
    }

    if (m_is_initialized) {
        auto status = m_subsystem_manager->query_subsystem_status();
        for (const auto& [type, readiness] : status) {
            const auto& [is_ready, is_running] = readiness;
            if (is_ready && is_running) {
                return true;
            }
        }
    }
    return false;
}

void Engine::await_shutdown()
{
#ifdef MAYAFLUX_PLATFORM_MACOS
    run_macos_event_loop();
#else
    // Simple blocking wait on other platforms
    std::cin.get();
#endif

    m_should_shutdown.store(true, std::memory_order_release);

    MF_PRINT(Journal::Component::Core, Journal::Context::Runtime,
        "Shutdown requested, awaiting all subsystem termination ......");
}

void Engine::request_shutdown()
{
    m_should_shutdown.store(true, std::memory_order_release);

#ifdef MAYAFLUX_PLATFORM_MACOS
    CFRunLoopStop(CFRunLoopGetMain());
#endif
}

bool Engine::is_shutdown_requested() const
{
    return m_should_shutdown.load(std::memory_order_acquire);
}

#ifdef MAYAFLUX_PLATFORM_MACOS
void Engine::run_macos_event_loop()
{
    CFRunLoopRef runLoop = CFRunLoopGetMain();

    dispatch_source_t stdinSource = dispatch_source_create(
        DISPATCH_SOURCE_TYPE_READ,
        STDIN_FILENO,
        0,
        dispatch_get_main_queue());

    dispatch_source_set_event_handler(stdinSource, ^{
        char buf[1024];
        ssize_t bytes_read = read(STDIN_FILENO, buf, sizeof(buf));
        if (bytes_read > 0) {
            request_shutdown();
        }
    });

    dispatch_resume(stdinSource);

    double timeout_seconds = 1.0 / static_cast<double>(m_graphics_config.target_frame_rate);

    MF_INFO(Journal::Component::Core, Journal::Context::Runtime,
        "Main thread event loop running (polling at {}fps)",
        m_graphics_config.target_frame_rate);

    while (!is_shutdown_requested()) {
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, timeout_seconds, false);
    }

    dispatch_source_cancel(stdinSource);
    dispatch_release(stdinSource);

    MF_INFO(Journal::Component::Core, Journal::Context::Runtime,
        "Main thread event loop exiting");
}
#endif

void Engine::End()
{
    if (!m_is_initialized)
        return;

    if (m_subsystem_manager) {
        m_subsystem_manager->stop_audio_subsystem();
    }

    if (m_scheduler) {
        m_scheduler->terminate_all_tasks();
    }

    if (m_subsystem_manager) {
        m_subsystem_manager->stop_graphics_subsystem();
    }

    if (m_buffer_manager) {
        m_buffer_manager->terminate_active_buffers();
        m_buffer_manager.reset();
    }

    if (m_window_manager) {
        auto windows = m_window_manager->get_windows();
        for (auto& window : windows) {
            m_window_manager->destroy_window(window);
        }
        m_window_manager.reset();
    }

    if (m_node_graph_manager) {
        m_node_graph_manager->terminate_active_processing();
        m_node_graph_manager.reset();
    }

    if (m_subsystem_manager) {
        m_subsystem_manager->shutdown();
    }

    m_is_initialized = false;
    m_is_paused = false;
}

} // namespace MayaFlux::Core
