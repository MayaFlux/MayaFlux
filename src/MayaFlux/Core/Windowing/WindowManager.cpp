#include "WindowManager.hpp"

#include "MayaFlux/Core/Backends/Windowing/Glfw/GlfwSingleton.hpp"
#include "MayaFlux/Core/Backends/Windowing/Glfw/GlfwWindow.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

#include "MayaFlux/Transitive/Parallel/Dispatch.hpp"

namespace MayaFlux::Core {

WindowManager::WindowManager(const GlobalGraphicsConfig& config)
    : m_config(config)
{
    MF_PRINT(Journal::Component::Core, Journal::Context::WindowingSubsystem,
        "WindowManager initialized");
}

WindowManager::~WindowManager()
{
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

    MF_INFO(Journal::Component::Core, Journal::Context::WindowingSubsystem,
        "Creating window '{}' ({}x{}), for platform {}", create_info.title, create_info.width, create_info.height, GLFWSingleton::get_platform());

    auto window = create_window_internal(create_info);
    if (!window) {
        return nullptr;
    }

    m_windows.push_back(window);
    m_window_lookup[create_info.title] = window;

    if (window->get_create_info().register_for_processing) {
        m_processing_windows.push_back(window);
    }

    MF_INFO(Journal::Component::Core, Journal::Context::WindowingSubsystem,
        "Created window '{}' - total: {}", create_info.title, m_windows.size());

    return window;
}

void WindowManager::destroy_window(const std::shared_ptr<Window>& window, bool cleanup_backend)
{
    if (!window)
        return;

    auto pr_it = std::ranges::find(m_processing_windows, window);
    if (pr_it != m_processing_windows.end()) {
        m_processing_windows.erase(pr_it);
    }

    const std::string title = window->get_create_info().title;
    remove_from_lookup(window);

    auto it = std::ranges::find(m_windows, window);
    if (it != m_windows.end()) {
        m_windows.erase(it);
        MF_INFO(Journal::Component::Core, Journal::Context::WindowingSubsystem,
            "Destroyed window '{}' - remaining: {}", title, m_windows.size());
    }

    if (!cleanup_backend) {
        return;
    }

    if (m_terminate.load()) {
        window->destroy();
    } else {
        Parallel::dispatch_main_sync([&]() {
            window->destroy();
        });
    }
    MF_DEBUG(Journal::Component::Core, Journal::Context::WindowingSubsystem,
        "Backend resources for window '{}' cleaned up", title);
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
    case GlobalGraphicsConfig::WindowingBackend::GLFW:
        return std::make_unique<GlfwWindow>(create_info, m_config.surface_info,
            m_config.requested_api, m_config.glfw_preinit_config);

    case GlobalGraphicsConfig::WindowingBackend::SDL:
        MF_ERROR(Journal::Component::Core, Journal::Context::WindowingSubsystem,
            "SDL backend not implemented");
        return nullptr;

    case GlobalGraphicsConfig::WindowingBackend::NATIVE:
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

bool WindowManager::process()
{
    Parallel::dispatch_main_sync([]() {
        glfwPollEvents();
    });

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

} // namespace MayaFlux::Core
