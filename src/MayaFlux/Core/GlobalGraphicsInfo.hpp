#pragma once

namespace MayaFlux::Core {

class FrameClock;
class VulkanBackend;

//==============================================================================
// GLOBAL VISUAL STREAM INFO (Parallel to GlobalStreamInfo)
//==============================================================================

/**
 * @struct GraphicsSurfaceInfo
 * @brief System-wide configuration for visual stream processing
 *
 * Defines technical parameters for ALL windows/visual streams in the system.
 * This is set once at subsystem initialization, similar to audio sample rate.
 * Individual windows inherit these defaults but can override specific params.
 */
struct GraphicsSurfaceInfo {
    /** @brief Target frame rate for visual processing (Hz) */
    u_int32_t target_frame_rate = 60;

    /**
     * @enum WindowingBackend
     * @brief Windowing library selection
     */
    enum class WindowingBackend : u_int8_t {
        GLFW, ///< GLFW3 (default, cross-platform)
        SDL, ///< SDL2 (alternative, if implemented)
        NATIVE, ///< Platform-native (Win32/X11/Cocoa, if implemented)
    };

    /** @brief Selected windowing backend */
    WindowingBackend windowing_backend = WindowingBackend::GLFW;

    /**
     * @enum VisualApi
     * @brief Supported graphics APIs (backend selection)
     */
    enum class VisualApi : u_int8_t {
        VULKAN,
        OPENGL,
        METAL,
        DIRECTX12
    };

    /** @brief Selected graphics API for rendering */
    VisualApi requested_api = VisualApi::VULKAN;

    /**
     * @enum SurfaceFormat
     * @brief Default pixel format for window surfaces (Vulkan-compatible)
     */
    enum class SurfaceFormat : u_int8_t {
        B8G8R8A8_SRGB, ///< Most common - 8-bit SRGB
        R8G8B8A8_SRGB, ///< Alternative 8-bit SRGB
        B8G8R8A8_UNORM, ///< 8-bit linear
        R8G8B8A8_UNORM, ///< 8-bit linear
        R16G16B16A16_SFLOAT, ///< 16-bit float HDR
        A2B10G10R10_UNORM, ///< 10-bit HDR
        R32G32B32A32_SFLOAT, ///< 32-bit float
    };

    /** @brief Default surface format for new windows */
    SurfaceFormat default_format = SurfaceFormat::B8G8R8A8_SRGB;

    /**
     * @enum ColorSpace
     * @brief Default color space for window surfaces
     */
    enum class ColorSpace : u_int8_t {
        SRGB_NONLINEAR, ///< Standard sRGB
        EXTENDED_SRGB, ///< Extended sRGB for HDR
        HDR10_ST2084, ///< HDR10 PQ
        DISPLAY_P3, ///< DCI-P3
    };

    /** @brief Default color space for new windows */
    ColorSpace default_color_space = ColorSpace::SRGB_NONLINEAR;

    /**
     * @enum PresentMode
     * @brief Frame presentation strategy
     */
    enum class PresentMode : u_int8_t {
        IMMEDIATE, ///< No vsync, tear possible
        MAILBOX, ///< Triple buffering, no tear
        FIFO, ///< Vsync, no tear
        FIFO_RELAXED, ///< Vsync, tear if late
    };

    /** @brief Default presentation mode for new windows */
    PresentMode default_present_mode = PresentMode::FIFO;

    /** @brief Default number of swapchain images (double/triple buffering) */
    u_int32_t preferred_image_count = 3;

    /** @brief Enable region-based processing by default */
    bool enable_regions = true;

    /** @brief Maximum regions per window container */
    u_int32_t max_regions_per_window = 256;

    /** @brief Enable HDR output if available */
    bool enable_hdr {};

    /** @brief Measure and report actual frame times */
    bool measure_frame_time {};

    /** @brief Output detailed diagnostic information */
    bool verbose_logging {};

    /** @brief On Linux, force use of Wayland even if X11 is available */
    bool linux_force_wayland = true;

    /** @brief Backend-specific configuration parameters */
    std::unordered_map<std::string, std::any> backend_options;
};

//==============================================================================
// PER-WINDOW CREATION INFO (Parallel to audio ChannelConfig)
//==============================================================================

/**
 * @struct WindowCreateInfo
 * @brief Configuration for creating a single window instance
 *
 * Lightweight per-window parameters. Most settings inherited from
 * GraphicsSurfaceInfo. This is like creating a new audio channel - you specify
 * only what differs from global defaults.
 */
struct WindowCreateInfo {
    /** @brief Window title/identifier */
    std::string title = "MayaFlux Window";

    /** @brief Initial window dimensions */
    u_int32_t width = 1920;
    u_int32_t height = 1080;

    /** @brief Target monitor ID (-1 = primary monitor) */
    int32_t monitor_id = -1;

    /** @brief Start in fullscreen mode */
    bool fullscreen = false;

    /** @brief Window can be resized by user */
    bool resizable = true;

    /** @brief Show OS window decorations (title bar, borders) */
    bool decorated = true;

    /** @brief Transparent framebuffer (compositing) */
    bool transparent = false;

    /** @brief Window always on top */
    bool floating = false;

    /** @brief Override global surface format (nullopt = use global default) */
    std::optional<GraphicsSurfaceInfo::SurfaceFormat> surface_format;

    /** @brief Override global present mode (nullopt = use global default) */
    std::optional<GraphicsSurfaceInfo::PresentMode> present_mode;

    /** @brief Container dimensions (channels) */
    struct {
        u_int32_t color_channels = 4;
        bool has_depth = false;
        bool has_stencil = false;
    } container_format;
};

//==============================================================================
// WINDOW RUNTIME STATE (Read-only, updated by subsystem)
//==============================================================================

/**
 * @struct WindowState
 * @brief Runtime state of a window (mutable by system, read by user)
 *
 * You don't set these - the windowing subsystem updates them as events occur.
 */
struct WindowState {
    u_int32_t current_width = 0;
    u_int32_t current_height = 0;

    bool is_visible = true;
    bool is_focused = false;
    bool is_minimized = false;
    bool is_maximized = false;
    bool is_hovered = false;

    u_int64_t frame_count = 0;
    double last_present_time = 0.0;
    double average_frame_time = 0.0;
};

//==============================================================================
// INPUT CONFIGURATION (Runtime mutable)
//==============================================================================

/**
 * @enum CursorMode
 * @brief Cursor visibility and behavior
 */
enum class CursorMode : u_int8_t {
    NORMAL, ///< Visible and movable
    HIDDEN, ///< Invisible but movable
    DISABLED, ///< Invisible and locked (FPS camera)
    CAPTURED, ///< Invisible, locked, raw motion
};

/**
 * @struct InputConfig
 * @brief Input configuration for a window
 *
 * Can be changed at runtime via window->set_input_config()
 */
struct InputConfig {
    bool keyboard_enabled = true;
    bool mouse_enabled = true;
    CursorMode cursor_mode = CursorMode::NORMAL;

    bool sticky_keys = false;
    bool sticky_mouse_buttons = false;
    bool raw_mouse_motion = false;
};

//==============================================================================
// WINDOW EVENTS
//==============================================================================

/**
 * @enum WindowEventType
 * @brief Types of window and input events
 */
enum class WindowEventType : u_int8_t {
    WINDOW_CREATED,
    WINDOW_DESTROYED,
    WINDOW_CLOSED,

    WINDOW_RESIZED,
    WINDOW_MOVED,
    WINDOW_FOCUS_GAINED,
    WINDOW_FOCUS_LOST,
    WINDOW_MINIMIZED,
    WINDOW_MAXIMIZED,
    WINDOW_RESTORED,

    KEY_PRESSED,
    KEY_RELEASED,
    KEY_REPEAT,
    MOUSE_MOVED,
    MOUSE_BUTTON_PRESSED,
    MOUSE_BUTTON_RELEASED,
    MOUSE_SCROLLED,
    MOUSE_ENTERED,
    MOUSE_EXITED,

    FRAMEBUFFER_RESIZED,

    CUSTOM,
};

/**
 * @struct WindowEvent
 * @brief Event data for window and input events
 */
struct WindowEvent {
    WindowEventType type;
    double timestamp;

    union EventData {
        struct {
            u_int32_t width, height;
        } resize;
        struct {
            int32_t key, scancode, mods;
        } key;
        struct {
            double x, y;
        } mouse_pos;
        struct {
            int32_t button, mods;
        } mouse_button;
        struct {
            double x_offset, y_offset;
        } scroll;
        std::any custom;

        EventData()
            : custom()
        {
        }
        ~EventData() { }

        EventData(const EventData& other)
        {
            new (&custom) std::any(other.custom);
        }

        EventData(EventData&& other) noexcept
        {
            new (&custom) std::any(std::move(other.custom));
        }

        EventData& operator=(const EventData& other)
        {
            if (this != &other) {
                custom = other.custom;
            }
            return *this;
        }

        EventData& operator=(EventData&& other) noexcept
        {
            if (this != &other) {
                custom = std::move(other.custom);
            }
            return *this;
        }
    } data;

    WindowEvent(const WindowEvent& other) = default;

    WindowEvent(WindowEvent&& other) noexcept
        : type(other.type)
        , timestamp(other.timestamp)
        , data(std::move(other.data))
    {
    }

    WindowEvent& operator=(const WindowEvent& other)
    {
        if (this != &other) {
            type = other.type;
            timestamp = other.timestamp;
            data = other.data;
        }
        return *this;
    }

    WindowEvent& operator=(WindowEvent&& other) noexcept
    {
        if (this != &other) {
            type = other.type;
            timestamp = other.timestamp;
            data = std::move(other.data);
        }
        return *this;
    }

    WindowEvent() = default;
    ~WindowEvent() = default;
};

using WindowEventCallback = std::function<void(const WindowEvent&)>;

//==============================================================================
// MONITOR INFORMATION (System query, not per-window config)
//==============================================================================

/**
 * @struct VideoMode
 * @brief Monitor video mode
 */
struct VideoMode {
    u_int32_t width, height;
    u_int32_t refresh_rate;
    u_int8_t red_bits, green_bits, blue_bits;

    bool operator==(const VideoMode& other) const
    {
        return width == other.width && height == other.height && refresh_rate == other.refresh_rate;
    }
};

/**
 * @struct MonitorInfo
 * @brief Information about a physical display
 */
struct MonitorInfo {
    int32_t id;
    std::string name;
    int32_t width_mm, height_mm;
    VideoMode current_mode;
    bool is_primary = false;
};

} // namespace MayaFlux::Core
