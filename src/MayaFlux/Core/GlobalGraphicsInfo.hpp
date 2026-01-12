#pragma once

namespace MayaFlux::Core {

//==============================================================================
// GRAPHICS BACKEND CONFIGURATION (Vulkan/OpenGL/etc.)
//==============================================================================

/**
 * @struct GraphicsBackendInfo
 * @brief Configuration for graphics API backend (Vulkan/OpenGL/etc.)
 *
 * Separate from windowing - this is GPU/rendering configuration.
 * GraphicsSurfaceInfo handles windows, this handles the graphics API.
 */
struct MAYAFLUX_API GraphicsBackendInfo {
    /** @brief Enable validation layers (debug builds) */
    bool enable_validation = true;

    /** @brief Enable GPU debug markers (for profiling tools) */
    bool enable_debug_markers = false;

    /** @brief Required device features (Vulkan-specific) */
    struct {
        bool compute_shaders = true;
        bool geometry_shaders = false;
        bool tessellation_shaders = false;
        bool multi_viewport = false;
        bool sampler_anisotropy = true;
        bool fill_mode_non_solid = false;
    } required_features;

    /** @brief Memory allocation strategy */
    enum class MemoryStrategy : uint8_t {
        CONSERVATIVE, ///< Minimize allocations
        BALANCED, ///< Balance speed and memory
        AGGRESSIVE ///< Maximize performance
    } memory_strategy
        = MemoryStrategy::BALANCED;

    /** @brief Command buffer pooling strategy */
    enum class CommandPooling : uint8_t {
        PER_THREAD, ///< One pool per thread
        SHARED, ///< Shared pool
        PER_QUEUE ///< One pool per queue family
    } command_pooling
        = CommandPooling::PER_THREAD;

    /** @brief Maximum number of frames in flight (GPU pipelining) */
    uint32_t max_frames_in_flight = 2;

    /** @brief Enable compute queue (separate from graphics) */
    bool enable_compute_queue = true;

    /** @brief Enable transfer queue (separate from graphics) */
    bool enable_transfer_queue = false;

    /** @brief Shader compilation strategy */
    enum class ShaderCompilation : uint8_t {
        RUNTIME, ///< Compile at runtime
        PRECOMPILED, ///< Use pre-compiled SPIR-V
        CACHED ///< Cache compiled shaders
    } shader_compilation
        = ShaderCompilation::CACHED;

    /** @brief Shader cache directory (if caching enabled) */
    std::filesystem::path shader_cache_dir = "cache/shaders";

    /** @brief Backend-specific extensions to request */
    std::vector<std::string> required_extensions;
    std::vector<std::string> optional_extensions;
};

/**
 * @struct GraphicsResourceLimits
 * @brief Resource limits and budgets for graphics subsystem
 *
 * Prevents runaway resource usage, similar to audio buffer limits.
 */
struct MAYAFLUX_API GraphicsResourceLimits {
    /** @brief Maximum number of concurrent windows */
    uint32_t max_windows = 16;

    /** @brief Maximum staging buffer size (MB) */
    uint32_t max_staging_buffer_mb = 256;

    /** @brief Maximum compute buffer size (MB) */
    uint32_t max_compute_buffer_mb = 1024;

    /** @brief Maximum texture cache size (MB) */
    uint32_t max_texture_cache_mb = 2048;

    /** @brief Maximum number of descriptor sets */
    uint32_t max_descriptor_sets = 1024;

    /** @brief Maximum number of pipeline state objects */
    uint32_t max_pipelines = 256;
};

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
struct MAYAFLUX_API GraphicsSurfaceInfo {

    /**
     * @enum SurfaceFormat
     * @brief Default pixel format for window surfaces (Vulkan-compatible)
     */
    enum class SurfaceFormat : uint8_t {
        B8G8R8A8_SRGB, ///< Most common - 8-bit SRGB
        R8G8B8A8_SRGB, ///< Alternative 8-bit SRGB
        B8G8R8A8_UNORM, ///< 8-bit linear
        R8G8B8A8_UNORM, ///< 8-bit linear
        R16G16B16A16_SFLOAT, ///< 16-bit float HDR
        A2B10G10R10_UNORM, ///< 10-bit HDR
        R32G32B32A32_SFLOAT, ///< 32-bit float
    };

    /** @brief Default surface format for new windows */
    SurfaceFormat format = SurfaceFormat::B8G8R8A8_SRGB;

    /**
     * @enum ColorSpace
     * @brief Default color space for window surfaces
     */
    enum class ColorSpace : uint8_t {
        SRGB_NONLINEAR, ///< Standard sRGB
        EXTENDED_SRGB, ///< Extended sRGB for HDR
        HDR10_ST2084, ///< HDR10 PQ
        DISPLAY_P3, ///< DCI-P3
    };

    /** @brief Default color space for new windows */
    ColorSpace color_space = ColorSpace::SRGB_NONLINEAR;

    /**
     * @enum PresentMode
     * @brief Frame presentation strategy
     */
    enum class PresentMode : uint8_t {
        IMMEDIATE, ///< No vsync, tear possible
        MAILBOX, ///< Triple buffering, no tear
        FIFO, ///< Vsync, no tear
        FIFO_RELAXED, ///< Vsync, tear if late
    };

    /** @brief Default presentation mode for new windows */
    PresentMode present_mode = PresentMode::FIFO;

    /** @brief Default number of swapchain images (double/triple buffering) */
    uint32_t image_count = 3;

    /** @brief Enable region-based processing by default */
    bool enable_regions = true;

    /** @brief Maximum regions per window container */
    uint32_t max_regions_per_window = 256;

    /** @brief Enable HDR output if available */
    bool enable_hdr {};

    /** @brief Measure and report actual frame times */
    bool measure_frame_time {};

    /** @brief Backend-specific configuration parameters */
    std::unordered_map<std::string, std::any> backend_options;
};

/**
 * @struct GlfwPreInitConfig
 * @brief Configuration hints for GLFW initialization
 *
 * Set before initializing the GLFW library. These affect how GLFW sets up
 * its internal state and platform integration.
 */
struct GlfwPreInitConfig {
    /**
     * @enum Platform
     * @brief Force a specific windowing platform on Linux
     */
    enum class Platform {
        Default,
        Wayland,
        X11
    } platform
        = Platform::Default;

    /** this prevents crash on some wayland compositors */
    bool disable_libdecor {};

    bool cocoa_chdir_resources = true;
    bool cocoa_menubar = true;

    /** @brief Request OpenGL debug context (if using OpenGL backend) */
    bool headless {};
};

struct MAYAFLUX_API GlobalGraphicsConfig {
    /** @brief Pre-initialization configuration for GLFW */
    GlfwPreInitConfig glfw_preinit_config;

    /** @brief System-wide configuration for visual stream processing */
    GraphicsSurfaceInfo surface_info;

    /** @brief Graphics backend configuration */
    GraphicsBackendInfo backend_info;

    /** @brief Resource limits */
    GraphicsResourceLimits resource_limits;

    /**
     * @enum WindowingBackend
     * @brief Windowing library selection
     */
    enum class WindowingBackend : uint8_t {
        GLFW, ///< GLFW3 (default, cross-platform)
        SDL, ///< SDL2 (alternative, if implemented)
        NATIVE, ///< Platform-native (Win32/X11/Cocoa, if implemented)
        NONE ///< No windowing (offscreen rendering only)
    };

    /**
     * @enum VisualApi
     * @brief Supported graphics APIs (backend selection)
     */
    enum class GraphicsApi : uint8_t {
        VULKAN,
        OPENGL,
        METAL,
        DIRECTX12
    };

    /** @brief Target frame rate for visual processing (Hz) */
    uint32_t target_frame_rate = 60;

    /** @brief Selected windowing backend */
    WindowingBackend windowing_backend = WindowingBackend::GLFW;

    /** @brief Selected graphics API for rendering */
    GraphicsApi requested_api = GraphicsApi::VULKAN;
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
struct MAYAFLUX_API WindowCreateInfo {
    /** @brief Window title/identifier */
    std::string title = "MayaFlux Window";

    /** @brief Initial window dimensions */
    uint32_t width = 1920;
    uint32_t height = 1080;

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

    /** @brief Register this window for processing (if false, no grpahics API handles visuals) */
    bool register_for_processing = true;

    /** @brief Override global surface format (nullopt = use global default) */
    std::optional<GraphicsSurfaceInfo::SurfaceFormat> surface_format;

    /** @brief Override global present mode (nullopt = use global default) */
    std::optional<GraphicsSurfaceInfo::PresentMode> present_mode;

    /** @brief Container dimensions (channels) */
    struct {
        uint32_t color_channels = 4;
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
    uint32_t current_width = 0;
    uint32_t current_height = 0;

    bool is_visible = true;
    bool is_focused = false;
    bool is_minimized = false;
    bool is_maximized = false;
    bool is_hovered = false;

    uint64_t frame_count = 0;
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
enum class CursorMode : uint8_t {
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
enum class WindowEventType : uint8_t {
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
    MOUSE_MOTION,
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

    struct ResizeData {
        uint32_t width, height;
    };
    struct KeyData {
        int32_t key, scancode, mods;
    };
    struct MousePosData {
        double x, y;
    };
    struct MouseButtonData {
        int32_t button, mods;
    };
    struct ScrollData {
        double x_offset, y_offset;
    };

    using EventData = std::variant<
        std::monostate,
        ResizeData,
        KeyData,
        MousePosData,
        MouseButtonData,
        ScrollData,
        std::any>;

    EventData data;

    WindowEvent() = default;
    WindowEvent(const WindowEvent&) = default;
    WindowEvent(WindowEvent&&) noexcept = default;
    WindowEvent& operator=(const WindowEvent&) = default;
    WindowEvent& operator=(WindowEvent&&) noexcept = default;
    ~WindowEvent() = default;
};
;

using WindowEventCallback = std::function<void(const WindowEvent&)>;

//==============================================================================
// MONITOR INFORMATION (System query, not per-window config)
//==============================================================================

/**
 * @struct VideoMode
 * @brief Monitor video mode
 */
struct VideoMode {
    uint32_t width, height;
    uint32_t refresh_rate;
    uint8_t red_bits, green_bits, blue_bits;

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
