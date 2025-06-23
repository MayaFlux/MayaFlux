#pragma once

#include "WindowContextInfo.hpp"

namespace MayaFlux::Utils {
/**
 * @enum WindowingBackendType
 * @brief Identifiers for supported windowing system backends
 *
 * Enumerates the available windowing system implementations (e.g., GLFW, SDL, native APIs)
 * that can be selected at runtime for window/context management.
 */
enum class WindowingBackendType;
}

namespace MayaFlux::Core {

/**
 * @class WindowingBackendFactory
 * @brief Factory for instantiating windowing backend implementations
 *
 * Provides a centralized mechanism for creating concrete windowing backend instances
 * based on the requested backend type. Enables runtime selection and abstraction
 * of platform-specific window/context management.
 */
class WindowingBackendFactory {
public:
    /**
     * @brief Creates a specific windowing backend implementation
     * @param type Identifier for the requested windowing backend type
     * @return Unique pointer to an IWindowBackend implementation
     *
     * Instantiates and configures the appropriate backend implementation
     * based on the specified type, abstracting the details of backend
     * selection and initialization.
     */
    static std::unique_ptr<IWindowBackend> create_backend(Utils::WindowingBackendType type);
};

/**
 * @class IWindowDevice
 * @brief Interface for windowing device enumeration and monitor management
 *
 * Provides access to the system's available display monitors and related
 * information, abstracting platform-specific enumeration and querying.
 */
class IWindowDevice {
public:
    /** @brief Virtual destructor for proper cleanup of derived classes */
    virtual ~IWindowDevice() = default;

    /**
     * @brief Retrieves information about all connected monitors
     * @return Vector of MonitorInfo structures for each detected monitor
     */
    virtual std::vector<MonitorInfo> get_monitors() const = 0;

    /**
     * @brief Gets the system's primary monitor identifier
     * @return Monitor ID for the primary display
     */
    virtual u_int32_t get_primary_monitor() const = 0;

    /**
     * @brief Retrieves detailed information for a specific monitor
     * @param monitor_id System identifier for the monitor
     * @return MonitorInfo structure describing the monitor's properties
     */
    virtual MonitorInfo get_monitor_info(u_int32_t monitor_id) const = 0;
};

/**
 * @class IWindowContext
 * @brief Interface for window/context lifecycle and event management
 *
 * Defines the contract for platform-agnostic window creation, visibility,
 * event handling, and property manipulation. Enables real-time interaction
 * with the windowing system and user input.
 */
class IWindowContext {
public:
    /** @brief Virtual destructor for proper cleanup of derived classes */
    virtual ~IWindowContext() = default;

    /**
     * @brief Creates the window/context with the configured parameters
     *
     * Allocates system resources and initializes the window for display and event handling.
     */
    virtual void create() = 0;

    /**
     * @brief Makes the window visible on the screen
     */
    virtual void show() = 0;

    /**
     * @brief Hides the window from the screen
     */
    virtual void hide() = 0;

    /**
     * @brief Destroys the window/context and releases all resources
     */
    virtual void destroy() = 0;

    /**
     * @brief Checks if the window/context has been created
     * @return True if the window is created, false otherwise
     */
    virtual bool is_created() const = 0;

    /**
     * @brief Checks if the window is currently visible
     * @return True if the window is visible, false otherwise
     */
    virtual bool is_visible() const = 0;

    /**
     * @brief Checks if the window should close (e.g., user requested close)
     * @return True if the window should close, false otherwise
     */
    virtual bool should_close() const = 0;

    /**
     * @brief Polls for windowing system events (non-blocking)
     *
     * Processes pending events such as input, resize, or close requests.
     */
    virtual void poll_events() = 0;

    /**
     * @brief Waits for windowing system events (blocking)
     *
     * Suspends execution until an event occurs, then processes it.
     */
    virtual void wait_events() = 0;

    /**
     * @brief Registers a callback for window events
     * @param callback Function to handle window events (input, resize, etc.)
     */
    virtual void set_event_callback(WindowEventCallback callback) = 0;

    /**
     * @brief Retrieves the native window handle
     * @return Opaque pointer to the platform-specific window object
     */
    virtual void* get_native_handle() const = 0;

    /**
     * @brief Retrieves the native display/context handle
     * @return Opaque pointer to the platform-specific display/context object
     */
    virtual void* get_native_display() const = 0;

    /**
     * @brief Sets the window size in pixels
     * @param width Desired window width
     * @param height Desired window height
     */
    virtual void set_size(u_int32_t width, u_int32_t height) = 0;

    /**
     * @brief Sets the window position on the screen
     * @param x X-coordinate of the window's top-left corner
     * @param y Y-coordinate of the window's top-left corner
     */
    virtual void set_position(u_int32_t x, u_int32_t y) = 0;

    /**
     * @brief Sets the window title (caption)
     * @param title New window title string
     */
    virtual void set_title(const std::string& title) = 0;

    /**
     * @brief Retrieves the current window size
     * @return Pair of (width, height) in pixels
     */
    virtual std::pair<u_int32_t, u_int32_t> get_size() const = 0;

    /**
     * @brief Retrieves the current window position
     * @return Pair of (x, y) coordinates
     */
    virtual std::pair<u_int32_t, u_int32_t> get_position() const = 0;
};

/**
 * @class IWindowBackend
 * @brief Interface for windowing backend abstraction layer
 *
 * Defines the contract for platform-specific windowing subsystem implementations,
 * providing hardware-agnostic access to window/context creation and monitor management.
 */
class IWindowBackend {
public:
    /** @brief Virtual destructor for proper cleanup of derived classes */
    virtual ~IWindowBackend() = default;

    /**
     * @brief Creates a device manager for monitor enumeration
     * @return Unique pointer to an IWindowDevice implementation
     *
     * Instantiates a platform-specific device manager that can enumerate
     * and provide information about available display monitors.
     */
    virtual std::unique_ptr<IWindowDevice> create_device_manager() = 0;

    /**
     * @brief Creates a window/context for a specific monitor
     * @param monitor_id System identifier for the target monitor
     * @param window_info Configuration parameters for the window/context
     * @return Unique pointer to an IWindowContext implementation
     *
     * Establishes a window/context on the specified monitor with the requested configuration.
     */
    virtual std::unique_ptr<IWindowContext> create_window_context(
        u_int32_t monitor_id,
        const GlobalWindowInfo& window_info)
        = 0;
};
}
