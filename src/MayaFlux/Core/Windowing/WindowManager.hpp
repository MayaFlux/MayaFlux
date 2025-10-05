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
    WindowManager(WindowManager&&) noexcept = delete;
    WindowManager& operator=(WindowManager&&) noexcept = delete;

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

    /**
     * @brief Start the window event loop on a dedicated thread
     *
     * Creates a background thread that continuously polls GLFW events.
     * This thread will run until stop() is called or all windows close.
     *
     * @note On macOS, this may not work - will fall back to main thread polling
     */
    void start_event_loop();

    /**
     * @brief Stop the window event loop thread
     *
     * Signals the event loop to stop and joins the thread.
     */
    void stop_event_loop();

    /**
     * @brief Check if event loop is running
     */
    bool is_event_loop_running() const { return m_event_loop_running; }

    /**
     * @brief Process windows for one frame
     *
     * This is the main per-frame operation that should be called
     * from the application's main loop. It:
     * 1. Polls GLFW events (triggers EventSource)
     * 2. Cleans up closed windows
     * 3. Optionally runs per-frame hooks
     *
     * @return True if processing should continue, false if all windows closed
     */
    bool process();

    /**
     * @brief Register a hook that runs every frame
     * @param name Hook identifier
     * @param hook Function to call each frame
     */
    void register_frame_hook(const std::string& name,
        std::function<void()> hook);

    /**
     * @brief Unregister a previously registered frame hook
     * @param name Hook identifier to remove
     */
    void unregister_frame_hook(const std::string& name);

    /**
     * @brief Get windows registered for processing
     */
    std::vector<std::shared_ptr<Window>> get_processing_windows() const { return m_processing_windows; }

private:
    GraphicsSurfaceInfo m_config;
    std::vector<std::shared_ptr<Window>> m_windows;
    std::unordered_map<std::string, std::weak_ptr<Window>> m_window_lookup;

    /**
     * @brief Factory for creating backend-specific windows
     */
    std::shared_ptr<Window> create_window_internal(const WindowCreateInfo& create_info);

    /** @brief Mutex for protecting m_windows and m_window_lookup */
    std::unique_ptr<std::thread> m_event_thread;

    /** @brief Atomic flag indicating if event loop thread is running */
    std::atomic<bool> m_event_loop_running { false };

    /** @brief Atomic flag to signal event loop thread to stop */
    std::atomic<bool> m_should_stop { false };

    /**
     * @brief Removes window from lookup table
     */
    void remove_from_lookup(const std::shared_ptr<Window>& window);

    /**
     * @brief Calls all registered frame hooks
     */
    std::unordered_map<std::string, std::function<void()>> m_frame_hooks;

    /** @brief Mutex for protecting m_windows and m_window_lookup */
    mutable std::mutex m_hooks_mutex;

    /** @brief Mutex for protecting m_windows and m_window_lookup */
    void event_loop_thread_func();

    /** @brief Thread for background event polling */
    static bool can_use_background_thread();

    std::vector<std::shared_ptr<Window>> m_processing_windows;
};

} // namespace MayaFlux::Core
