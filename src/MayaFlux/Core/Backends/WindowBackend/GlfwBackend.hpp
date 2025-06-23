#pragma once

#include "WindowBackend.hpp"

class GLFWmonitor;
class GLFWwindow;

namespace MayaFlux::Core {

/**
 * @class GLFWDevice
 * @brief Implementation of IWindowDevice for GLFW-based monitor enumeration.
 *
 * GLFWDevice provides access to the system's connected display monitors using the GLFW library.
 * It enumerates available monitors, identifies the primary monitor, and exposes monitor information
 * in a platform-agnostic format.
 *
 * Responsibilities:
 * - Enumerate all connected monitors at construction.
 * - Provide monitor information and primary monitor ID.
 * - Convert GLFW monitor data to MayaFlux's MonitorInfo structure.
 */
class GLFWDevice : public IWindowDevice {
public:
    /**
     * @brief Constructs a GLFWDevice and enumerates all monitors.
     */
    GLFWDevice();

    /**
     * @brief Destructor for GLFWDevice.
     */
    ~GLFWDevice() override;

    /**
     * @brief Retrieves information about all connected monitors.
     * @return Vector of MonitorInfo structures for each detected monitor.
     */
    inline std::vector<MonitorInfo> get_monitors() const override { return m_monitors; }

    /**
     * @brief Gets the system's primary monitor identifier.
     * @return Monitor ID for the primary display.
     */
    inline u_int32_t get_primary_monitor() const override { return m_primary_monitor; }

    /**
     * @brief Retrieves detailed information for a specific monitor.
     * @param monitor_id System identifier for the monitor.
     * @return MonitorInfo structure describing the monitor's properties.
     */
    MonitorInfo get_monitor_info(u_int32_t monitor_id) const override;

private:
    /**
     * @brief Enumerates all connected monitors and populates m_monitors.
     */
    void enumerate_monitors();

    /**
     * @brief Converts a GLFWmonitor pointer to a MonitorInfo structure.
     * @param monitor Pointer to the GLFWmonitor.
     * @param is_primary True if this is the primary monitor.
     * @return MonitorInfo structure with monitor details.
     */
    static MonitorInfo convert_monitor_info(GLFWmonitor* monitor, bool is_primary);

    std::vector<MonitorInfo> m_monitors; ///< List of detected monitors.
    u_int32_t m_primary_monitor; ///< ID of the primary monitor.
};

/**
 * @class GLFWContext
 * @brief Implementation of IWindowContext for GLFW-based window/context management.
 *
 * GLFWContext manages the lifecycle and properties of a single GLFW window, including creation,
 * visibility, event handling, and property manipulation. It provides a platform-agnostic interface
 * for window management and user input via the MayaFlux IWindowContext contract.
 *
 * Responsibilities:
 * - Create and destroy GLFW windows.
 * - Show, hide, and manage window visibility.
 * - Handle window events and dispatch them via callbacks.
 * - Provide access to native window and display handles.
 * - Support window property manipulation (size, position, title).
 */
class GLFWContext : public IWindowContext {
public:
    /**
     * @brief Constructs a GLFWContext for a given monitor and window configuration.
     * @param monitor Pointer to the target GLFWmonitor.
     * @param window_info Configuration parameters for the window/context.
     */
    GLFWContext(GLFWmonitor* monitor, const GlobalWindowInfo& window_info);

    /**
     * @brief Destructor for GLFWContext.
     */
    ~GLFWContext() override;

    /**
     * @brief Creates the window/context with the configured parameters.
     */
    void create() override;

    /**
     * @brief Makes the window visible on the screen.
     */
    void show() override;

    /**
     * @brief Hides the window from the screen.
     */
    void hide() override;

    /**
     * @brief Destroys the window/context and releases all resources.
     */
    void destroy() override;

    /**
     * @brief Checks if the window/context has been created.
     * @return True if the window is created, false otherwise.
     */
    bool is_created() const override;

    /**
     * @brief Checks if the window is currently visible.
     * @return True if the window is visible, false otherwise.
     */
    bool is_visible() const override;

    /**
     * @brief Checks if the window should close (e.g., user requested close).
     * @return True if the window should close, false otherwise.
     */
    bool should_close() const override;

    /**
     * @brief Polls for windowing system events (non-blocking).
     */
    void poll_events() override;

    /**
     * @brief Waits for windowing system events (blocking).
     */
    void wait_events() override;

    /**
     * @brief Registers a callback for window events.
     * @param callback Function to handle window events (input, resize, etc.).
     */
    void set_event_callback(WindowEventCallback callback) override;

    /**
     * @brief Retrieves the native window handle.
     * @return Opaque pointer to the platform-specific window object.
     */
    void* get_native_handle() const override;

    /**
     * @brief Retrieves the native display/context handle.
     * @return Opaque pointer to the platform-specific display/context object.
     */
    void* get_native_display() const override;

    /**
     * @brief Sets the window size in pixels.
     * @param width Desired window width.
     * @param height Desired window height.
     */
    void set_size(u_int32_t width, u_int32_t height) override;

    /**
     * @brief Sets the window position on the screen.
     * @param x X-coordinate of the window's top-left corner.
     * @param y Y-coordinate of the window's top-left corner.
     */
    void set_position(u_int32_t x, u_int32_t y) override;

    /**
     * @brief Sets the window title (caption).
     * @param title New window title string.
     */
    void set_title(const std::string& title) override;

    /**
     * @brief Retrieves the current window size.
     * @return Pair of (width, height) in pixels.
     */
    std::pair<u_int32_t, u_int32_t> get_size() const override;

    /**
     * @brief Retrieves the current window position.
     * @return Pair of (x, y) coordinates.
     */
    std::pair<u_int32_t, u_int32_t> get_position() const override;

private:
    /**
     * @brief GLFW callback for window size changes.
     */
    static void window_size_callback(GLFWwindow* window, int width, int height);

    /**
     * @brief GLFW callback for window close events.
     */
    static void window_close_callback(GLFWwindow* window);

    /**
     * @brief GLFW callback for window focus events.
     */
    static void window_focus_callback(GLFWwindow* window, int focused);

    /**
     * @brief GLFW callback for keyboard events.
     */
    static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);

    /**
     * @brief GLFW callback for cursor position events.
     */
    static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos);

    /**
     * @brief GLFW callback for mouse button events.
     */
    static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);

    /**
     * @brief GLFW callback for scroll events.
     */
    static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

    /**
     * @brief Configures GLFW window hints based on window_info.
     */
    void configure_window_hints();

    GLFWwindow* m_window = nullptr; ///< Pointer to the GLFW window.
    GLFWmonitor* m_monitor = nullptr; ///< Pointer to the target GLFW monitor.
    GlobalWindowInfo m_window_info; ///< Window/context configuration.
    WindowEventCallback m_event_callback; ///< Registered event callback.
    bool m_is_created = false; ///< Whether the window has been created.
};

/**
 * @class GLFWBackend
 * @brief Implementation of IWindowBackend for the GLFW windowing system.
 *
 * GLFWBackend provides a concrete backend for window/context and monitor management using GLFW.
 * It enables runtime selection of GLFW as the windowing backend and provides device/context creation.
 *
 * Responsibilities:
 * - Initialize and manage the GLFW library as needed.
 * - Create device managers for monitor enumeration.
 * - Create window/context instances for rendering and user interaction.
 */
class GLFWBackend : public IWindowBackend {
public:
    /**
     * @brief Constructs a GLFWBackend and initializes the backend state.
     */
    GLFWBackend();

    /**
     * @brief Destructor for GLFWBackend.
     */
    ~GLFWBackend() override;

    /**
     * @brief Creates a device manager for monitor enumeration.
     * @return Unique pointer to a GLFWDevice instance.
     */
    std::unique_ptr<IWindowDevice> create_device_manager() override;

    /**
     * @brief Creates a window/context for a specific monitor.
     * @param monitor_id System identifier for the target monitor.
     * @param window_info Configuration parameters for the window/context.
     * @return Unique pointer to a GLFWContext instance.
     */
    std::unique_ptr<IWindowContext> create_window_context(
        u_int32_t monitor_id,
        const GlobalWindowInfo& window_info) override;

private:
    bool m_initialized = false; ///< Tracks whether the backend has been initialized.
};

}
