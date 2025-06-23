#pragma once

#include "config.h"

namespace MayaFlux::Core {

/**
 * @class IWindowBackend
 * @brief Forward declaration for window backend interface
 *
 * Represents the abstract interface for platform-specific windowing backends.
 */
class IWindowBackend;

/**
 * @class IWindowDevice
 * @brief Forward declaration for window device interface
 *
 * Represents the abstract interface for monitor and device enumeration.
 */
class IWindowDevice;

/**
 * @class IWindowContext
 * @brief Forward declaration for window context interface
 *
 * Represents the abstract interface for window/context lifecycle management.
 */
class IWindowContext;

/**
 * @struct GlobalWindowInfo
 * @brief Configuration structure for window/context creation and management
 *
 * Encapsulates all parameters required to create and configure a window or rendering context.
 * This includes output/display settings, input device options, window priority, and backend-specific options.
 *
 * Use cases:
 * - Passing configuration to window/context creation routines
 * - Storing persistent window settings
 * - Abstracting platform-specific window parameters
 */
struct GlobalWindowInfo {
    /**
     * @enum WindowFormat
     * @brief Supported framebuffer formats for window output
     *
     * Specifies the pixel format and precision for the window's framebuffer.
     */
    enum class WindowFormat {
        RGBA8, ///< 8-bit per channel RGBA
        RGBA16F, ///< 16-bit floating point per channel RGBA
        RGBA32F ///< 32-bit floating point per channel RGBA
    };

    /**
     * @enum WindowPriority
     * @brief Scheduling priority for the window/context
     *
     * Determines the scheduling and resource allocation priority for the window.
     */
    enum class WindowPriority {
        NORMAL, ///< Standard priority
        HIGH, ///< Elevated priority (e.g., for performance-sensitive windows)
        REALTIME ///< Real-time priority (may require special OS privileges)
    };

    /**
     * @struct WindowOutput
     * @brief Output/display configuration for the window
     *
     * Contains all parameters related to the window's appearance, size, monitor selection,
     * and framebuffer format.
     */
    struct WindowOutput {
        bool enabled = true; ///< Whether this output is enabled
        u_int32_t width = 1920; ///< Window width in pixels
        u_int32_t height = 1080; ///< Window height in pixels
        u_int32_t monitor_id = -1; ///< Target monitor ID (-1 for default)
        bool fullscreen = false; ///< Whether the window is fullscreen
        bool resizable = true; ///< Whether the window can be resized
        bool decorated = true; ///< Whether the window has OS decorations (title bar, borders)
        bool floating = false; ///< Whether the window is always on top
        WindowFormat format = WindowFormat::RGBA8; ///< Framebuffer format
    };

    /**
     * @struct WindowInput
     * @brief Input device configuration for the window
     *
     * Specifies which input devices and features are enabled for the window.
     */
    struct WindowInput {
        bool keyboard_enabled = true; ///< Enable keyboard input
        bool mouse_enabled = true; ///< Enable mouse input
        bool cursor_enabled = true; ///< Show and enable the mouse cursor
        bool raw_mouse_motion = false; ///< Enable raw mouse motion events
        bool sticky_keys = false; ///< Enable sticky keys (key state persists until queried)
        bool sticky_mouse_buttons = false; ///< Enable sticky mouse buttons
    };

    u_int32_t refresh_rate = 60; ///< Desired refresh rate (Hz)
    WindowPriority priority = WindowPriority::NORMAL; ///< Window scheduling priority
    std::string title = "MayaFlux Window"; ///< Window title/caption

    WindowOutput output; ///< Output/display configuration
    WindowInput input; ///< Input device configuration

    /**
     * @brief Backend-specific configuration options
     *
     * Allows passing arbitrary key-value pairs to the backend implementation.
     * Example: { "glfw.context_version_major" = 4 }
     */
    std::unordered_map<std::string, std::any> backend_options;
};

/**
 * @struct MonitorInfo
 * @brief Information about a physical display monitor
 *
 * Describes the properties and capabilities of a connected display monitor,
 * including physical dimensions, supported video modes, and refresh rates.
 */
struct MonitorInfo {
    std::string name; ///< Human-readable monitor name
    int32_t width_mm; ///< Physical width in millimeters
    int32_t height_mm; ///< Physical height in millimeters
    int32_t refresh_rate; ///< Current refresh rate (Hz)
    std::vector<u_int32_t> supported_refresh_rates; ///< List of supported refresh rates

    /**
     * @struct VideoMode
     * @brief Supported video mode for a monitor
     *
     * Describes a single video mode, including resolution, color depth, and refresh rate.
     */
    struct VideoMode {
        u_int32_t width, height; ///< Resolution in pixels
        u_int32_t red_bits, green_bits, blue_bits; ///< Color depth per channel
        u_int32_t refresh_rate; ///< Refresh rate (Hz)
    };
    std::vector<VideoMode> video_modes; ///< List of supported video modes
    bool is_primary = false; ///< Whether this is the primary monitor
};

/**
 * @struct WindowEvent
 * @brief Event structure for window and input events
 *
 * Encapsulates all information about a window or input event, including type,
 * timestamp, and event-specific data.
 *
 * Used for event-driven window/context management and input handling.
 */
struct WindowEvent {
    /**
     * @enum Type
     * @brief Enumerates all supported window and input event types
     */
    enum Type {
        WINDOW_RESIZE, ///< Window was resized
        WINDOW_CLOSE, ///< Window close requested
        WINDOW_FOCUS, ///< Window focus gained/lost
        WINDOW_ICONIFY, ///< Window minimized/restored
        KEY_PRESS, ///< Key pressed
        KEY_RELEASE, ///< Key released
        MOUSE_MOVE, ///< Mouse moved
        MOUSE_BUTTON, ///< Mouse button pressed/released
        MOUSE_SCROLL, ///< Mouse wheel scrolled
        CUSTOM ///< Custom user-defined event
    } type;

    double timestamp; ///< Event timestamp (seconds since epoch or app start)
    std::unordered_map<std::string, std::any> data; ///< Event-specific data (e.g., key code, mouse position)
};

/**
 * @typedef WindowEventCallback
 * @brief Callback type for handling window events
 *
 * Function signature for event handlers that process WindowEvent structures.
 */
using WindowEventCallback = std::function<void(const WindowEvent&)>;

} //
