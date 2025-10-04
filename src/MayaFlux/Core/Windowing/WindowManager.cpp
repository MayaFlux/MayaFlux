#include "WindowManager.hpp"

#include "MayaFlux/Core/Backends/Windowing/Glfw/GlfwSingleton.hpp"
#include "MayaFlux/Core/Backends/Windowing/Glfw/GlfwWindow.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Utils.hpp"

namespace MayaFlux::Core {

WindowManager::WindowManager(const GraphicsSurfaceInfo& config)
    : m_config(config)
{
    MF_PRINT(Journal::Component::Core, Journal::Context::WindowingSubsystem,
        "WindowManager initialized");
}

WindowManager::~WindowManager()
{
    stop_event_loop();

    m_windows.clear();
    m_window_lookup.clear();

    GLFWSingleton::terminate();

    MF_INFO(Journal::Component::Core, Journal::Context::WindowingSubsystem,
        "WindowManager destroyed");
}

std::shared_ptr<Window> WindowManager::create_window(const WindowCreateInfo& create_info)
{
    if (m_window_lookup.count(create_info.title) > 0) {
        MF_WARN(Journal::Component::Core, Journal::Context::WindowingSubsystem,
            "Window with title '{}' already exists", create_info.title);
        return nullptr;
    }

    auto window = create_window_internal(create_info);
    if (!window) {
        return nullptr;
    }

    m_windows.push_back(window);
    m_window_lookup[create_info.title] = window;

    MF_INFO(Journal::Component::Core, Journal::Context::WindowingSubsystem,
        "Created window '{}' - total: {}", create_info.title, m_windows.size());

    return window;
}

void WindowManager::destroy_window(const std::shared_ptr<Window>& window)
{
    if (!window)
        return;

    const std::string title = window->get_create_info().title;
    remove_from_lookup(window);

    auto it = std::ranges::find(m_windows, window);
    if (it != m_windows.end()) {
        m_windows.erase(it);
        MF_INFO(Journal::Component::Core, Journal::Context::WindowingSubsystem,
            "Destroyed window '{}' - remaining: {}", title, m_windows.size());
    }
}

bool WindowManager::destroy_window_by_title(const std::string& title)
{
    auto window = find_window(title);
    if (window) {
        destroy_window(window);
        return true;
    }
    return false;
}

std::vector<std::shared_ptr<Window>> WindowManager::get_windows() const
{
    std::vector<std::shared_ptr<Window>> ptrs;
    ptrs.reserve(m_windows.size());
    for (const auto& w : m_windows) {
        ptrs.push_back(w);
    }
    return ptrs;
}

std::shared_ptr<Window> WindowManager::find_window(const std::string& title) const
{
    auto it = m_window_lookup.find(title);
    return (it != m_window_lookup.end()) ? it->second.lock() : nullptr;
}

std::shared_ptr<Window> WindowManager::get_window(size_t index) const
{
    if (index >= m_windows.size()) {
        return nullptr;
    }
    return m_windows[index];
}

bool WindowManager::any_window_should_close() const
{
    return std::ranges::any_of(m_windows,
        [](const auto& w) { return w->should_close(); });
}

size_t WindowManager::destroy_closed_windows()
{
    size_t destroyed_count = 0;

    std::vector<std::shared_ptr<Window>> to_destroy;
    for (const auto& window : m_windows) {
        if (window->should_close()) {
            to_destroy.push_back(window);
        }
    }

    for (auto& window : to_destroy) {
        destroy_window(window);
        destroyed_count++;
    }

    if (destroyed_count > 0) {
        MF_INFO(Journal::Component::Core, Journal::Context::WindowingSubsystem,
            "Destroyed {} closed window(s)", destroyed_count);
    }

    return destroyed_count;
}

std::shared_ptr<Window> WindowManager::create_window_internal(
    const WindowCreateInfo& create_info)
{
    switch (m_config.windowing_backend) {
    case GraphicsSurfaceInfo::WindowingBackend::GLFW:
        return std::make_unique<GlfwWindow>(create_info, m_config);

    case GraphicsSurfaceInfo::WindowingBackend::SDL:
        MF_ERROR(Journal::Component::Core, Journal::Context::WindowingSubsystem,
            "SDL backend not implemented");
        return nullptr;

    case GraphicsSurfaceInfo::WindowingBackend::NATIVE:
        MF_ERROR(Journal::Component::Core, Journal::Context::WindowingSubsystem,
            "Native backend not implemented");
        return nullptr;

    default:
        MF_ERROR(Journal::Component::Core, Journal::Context::WindowingSubsystem,
            "Unknown windowing backend: {}",
            static_cast<int>(m_config.windowing_backend));
        return nullptr;
    }
}

void WindowManager::remove_from_lookup(const std::shared_ptr<Window>& window)
{
    const auto& title = window->get_create_info().title;
    m_window_lookup.erase(title);
}

void WindowManager::start_event_loop()
{
    if (m_event_loop_running) {
        MF_WARN(Journal::Component::Core, Journal::Context::WindowingSubsystem,
            "Event loop already running");
        return;
    }

    if (!can_use_background_thread()) {
        error<std::runtime_error>(Journal::Component::Core, Journal::Context::WindowingSubsystem,
            std::source_location::current(),
            "Background window thread not supported on this platform (likely macOS). \n Falling back to main thread polling.");
    }

    m_should_stop = false;
    m_event_loop_running = true;

    m_event_thread = std::make_unique<std::thread>(
        &WindowManager::event_loop_thread_func, this);

    MF_INFO(Journal::Component::Core, Journal::Context::WindowingSubsystem,
        "Started window event loop on dedicated thread");
}

void WindowManager::stop_event_loop()
{
    if (!m_event_loop_running) {
        return;
    }

    m_should_stop = true;

    if (m_event_thread && m_event_thread->joinable()) {
        m_event_thread->join();
    }

    m_event_loop_running = false;
    m_event_thread.reset();

    MF_INFO(Journal::Component::Core, Journal::Context::WindowingSubsystem,
        "Stopped window event loop thread");
}

void WindowManager::event_loop_thread_func()
{
    MF_INFO(Journal::Component::Core, Journal::Context::WindowingSubsystem,
        "Window event loop thread started (target: {} FPS)", m_config.target_frame_rate);

    const auto frame_time = Utils::frame_duration_ms(m_config.target_frame_rate);

    while (!m_should_stop && window_count() > 0) {
        auto frame_start = std::chrono::steady_clock::now();

        glfwPollEvents();

        {
            std::lock_guard<std::mutex> lock(m_hooks_mutex);
            for (const auto& [name, hook] : m_frame_hooks) {
                hook();
            }
        }

        destroy_closed_windows();

        auto frame_end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            frame_end - frame_start);

        if (elapsed < frame_time) {
            std::this_thread::sleep_for(frame_time - elapsed);
        }
    }

    MF_INFO(Journal::Component::Core, Journal::Context::WindowingSubsystem,
        "Window event loop thread exiting");
}

bool WindowManager::process()
{
    glfwPollEvents();

    {
        std::lock_guard<std::mutex> lock(m_hooks_mutex);
        for (const auto& [name, hook] : m_frame_hooks) {
            hook();
        }
    }

    destroy_closed_windows();

    return window_count() > 0;
}

void WindowManager::register_frame_hook(const std::string& name,
    std::function<void()> hook)
{
    std::lock_guard<std::mutex> lock(m_hooks_mutex);
    m_frame_hooks[name] = std::move(hook);
}

void WindowManager::unregister_frame_hook(const std::string& name)
{
    std::lock_guard<std::mutex> lock(m_hooks_mutex);
    m_frame_hooks.erase(name);
}

bool WindowManager::can_use_background_thread()
{
#ifdef MAYAFLUX_PLATFORM_MACOS
    return false;
#else
    return true;
#endif
}

} // namespace MayaFlux::Core
