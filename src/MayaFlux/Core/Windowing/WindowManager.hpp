#pragma once

#include "MayaFlux/Core/Backends/Windowing/Window.hpp"
#include "MayaFlux/Core/GlobalGraphicsInfo.hpp"

namespace MayaFlux::Core {

/**
 * @class WindowManager
 * @brief Manages window lifecycle and GLFW event polling
 *
 * Responsibilities:
 * - Create/destroy windows
 * - Poll GLFW events (calls glfwPollEvents)
 * - Query windows by title/index
 *
 * NOT responsible for:
 * - Frame timing (that's FrameClock)
 * - Rendering (that's VulkanBackend)
 * - Coroutine scheduling (that's TaskScheduler)
 * - Node/buffer processing (that's their respective managers)
 */
class WindowManager {
public:
    /**
     * @brief Constructs WindowManager with global graphics config
     * @param config Global graphics configuration for window defaults
     */
    explicit WindowManager(const GraphicsSurfaceInfo& config);

    ~WindowManager();

    WindowManager(const WindowManager&) = delete;
    WindowManager& operator=(const WindowManager&) = delete;
    WindowManager(WindowManager&&) noexcept = default;
    WindowManager& operator=(WindowManager&&) noexcept = default;

    /**
     * @brief Creates a new window
     * @param create_info Window creation parameters
     * @return Pointer to created window (owned by manager)
     */
    std::shared_ptr<Window> create_window(const WindowCreateInfo& create_info);

    /**
     * @brief Destroys a window by pointer
     * @param window Pointer to window to destroy
     */
    void destroy_window(const std::shared_ptr<Window>& window);

    /**
     * @brief Destroys a window by title
     * @param title Title of window to destroy
     * @return True if window was found and destroyed
     */
    bool destroy_window_by_title(const std::string& title);

    /**
     * @brief Polls GLFW for events
     *
     * This is the core responsibility of WindowManager. Calling this:
     * 1. Triggers GLFW callbacks
     * 2. Updates window state
     * 3. Signals EventSources
     * 4. Resumes waiting event coroutines
     */
    void poll_events();

    /**
     * @brief Gets all active windows
     */
    std::vector<std::shared_ptr<Window>> get_windows() const;

    /**
     * @brief Finds window by title
     * @return Pointer to window, or nullptr if not found
     */
    std::shared_ptr<Window> find_window(const std::string& title) const;

    /**
     * @brief Gets window by index
     * @return Pointer to window, or nullptr if index out of bounds
     */
    std::shared_ptr<Window> get_window(size_t index) const;

    /**
     * @brief Gets number of active windows
     */
    size_t window_count() const { return m_windows.size(); }

    /**
     * @brief Checks if any window should close
     * @return True if at least one window has close requested
     */
    bool any_window_should_close() const;

    /**
     * @brief Destroys all windows that should close
     * @return Number of windows destroyed
     */
    size_t destroy_closed_windows();

    /**
     * @brief Gets the global graphics configuration
     */
    const GraphicsSurfaceInfo& get_config() const { return m_config; }

private:
    GraphicsSurfaceInfo m_config;
    std::vector<std::shared_ptr<Window>> m_windows;
    std::unordered_map<std::string, std::weak_ptr<Window>> m_window_lookup;

    /**
     * @brief Factory for creating backend-specific windows
     */
    std::shared_ptr<Window> create_window_internal(const WindowCreateInfo& create_info);

    /**
     * @brief Removes window from lookup table
     */
    void remove_from_lookup(const std::shared_ptr<Window>& window);
};

} // namespace MayaFlux::Core
