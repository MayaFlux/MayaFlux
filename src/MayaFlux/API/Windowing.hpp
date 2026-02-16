#pragma once

namespace MayaFlux {

namespace Core {
    struct GraphicsSurfaceInfo;
    class WindowManager;
    class Window;
    struct WindowCreateInfo;
}

/**
 * @brief Gets a handle to default window manager
 * @return Reference to the WindowManager
 */
MAYAFLUX_API Core::WindowManager& get_window_manager();

/**
 * @brief Create a new window with specified parameters
 * @param create_info Struct containing window creation parameters (title, dimensions, monitor_id, surface format overrides, etc.)
 * @return Shared pointer to the created Window instance
 *
 * This function abstracts away platform-specific window creation details.
 * The returned Window object can be used for rendering and event handling.
 */
MAYAFLUX_API std::shared_ptr<Core::Window> create_window(const Core::WindowCreateInfo& create_info);

/**
 * @brief Convert window pixel coordinates to normalized device coordinates (NDC)
 * @param window_x X coordinate in window space [0, width]
 * @param window_y Y coordinate in window space [0, height] (top-left origin)
 * @param window_width Window width in pixels
 * @param window_height Window height in pixels
 * @return NDC coordinates as vec3 (x, y, 0.0) in range [-1, +1] (center origin, +Y up)
 *
 * Window coordinates use top-left origin with Y increasing downward.
 * NDC coordinates use center origin with Y increasing upward.
 * Z is always set to 0.0 for 2D screen-space positions.
 *
 * Example:
 *   normalize_coords(0, 0, 800, 600) → (-1.0, -1.0, 0.0)  // top-left
 *   normalize_coords(400, 300, 800, 600) → (0.0, 0.0, 0.0)  // center
 *   normalize_coords(800, 600, 800, 600) → (+1.0, +1.0, 0.0)  // bottom-right
 */
MAYAFLUX_API glm::vec3 normalize_coords(double window_x, double window_y,
    uint32_t window_width, uint32_t window_height);

/**
 * @brief Convert window pixel coordinates to NDC using window state
 * @param window_x X coordinate in window space
 * @param window_y Y coordinate in window space
 * @param window Window to extract dimensions from
 * @return NDC coordinates as vec3 (x, y, 0.0)
 */
MAYAFLUX_API glm::vec3 normalize_coords(double window_x, double window_y,
    const std::shared_ptr<Core::Window>& window);

/**
 * @brief Convert NDC coordinates to window pixel coordinates
 * @param ndc_x X coordinate in NDC space [-1, +1]
 * @param ndc_y Y coordinate in NDC space [-1, +1] (center origin, +Y up)
 * @param ndc_z Z coordinate (ignored, accepted for convenience when passing vec3)
 * @param window_width Window width in pixels
 * @param window_height Window height in pixels
 * @return Window coordinates in pixels (top-left origin, +Y down)
 *
 * Inverse of normalize_coords(). Z coordinate is ignored.
 */
MAYAFLUX_API glm::vec2 window_coords(double ndc_x, double ndc_y, double ndc_z,
    uint32_t window_width, uint32_t window_height);

/**
 * @brief Convert NDC coordinates to window pixel coordinates using window state
 * @param ndc_x X coordinate in NDC space
 * @param ndc_y Y coordinate in NDC space
 * @param ndc_z Z coordinate (ignored)
 * @param window Window to extract dimensions from
 * @return Window coordinates in pixels
 */
MAYAFLUX_API glm::vec2 window_coords(double ndc_x, double ndc_y, double ndc_z,
    const std::shared_ptr<Core::Window>& window);

/**
 * @brief Convert NDC vec3 position to window pixel coordinates
 * @param ndc_pos NDC position (z component ignored)
 * @param window_width Window width in pixels
 * @param window_height Window height in pixels
 * @return Window coordinates in pixels
 */
MAYAFLUX_API glm::vec2 window_coords(const glm::vec3& ndc_pos,
    uint32_t window_width, uint32_t window_height);

/**
 * @brief Convert NDC vec3 position to window pixel coordinates using window state
 * @param ndc_pos NDC position (z component ignored)
 * @param window Window to extract dimensions from
 * @return Window coordinates in pixels
 */
MAYAFLUX_API glm::vec2 window_coords(const glm::vec3& ndc_pos,
    const std::shared_ptr<Core::Window>& window);

/**
 * @brief Get window aspect ratio (width/height)
 * @param window_width Window width in pixels
 * @param window_height Window height in pixels
 * @return Aspect ratio as float
 */
MAYAFLUX_API float aspect_ratio(uint32_t window_width, uint32_t window_height);

/**
 * @brief Get window aspect ratio from window state
 * @param window Window to extract dimensions from
 * @return Aspect ratio as float
 */
MAYAFLUX_API float aspect_ratio(const std::shared_ptr<Core::Window>& window);

/**
 * @brief Normalize coordinates preserving aspect ratio (useful for circular/square shapes)
 * @param window_x X coordinate in window space
 * @param window_y Y coordinate in window space
 * @param window_width Window width in pixels
 * @param window_height Window height in pixels
 * @return Aspect-corrected NDC coordinates where circles remain circular
 *
 * Unlike normalize_coords(), this ensures geometric shapes maintain their proportions.
 * The shorter dimension maps to [-1, +1], the longer dimension extends beyond.
 */
MAYAFLUX_API glm::vec3 normalize_coords_aspect(double window_x, double window_y,
    uint32_t window_width, uint32_t window_height);

/**
 * @brief Normalize coordinates preserving aspect ratio using window state
 * @param window_x X coordinate in window space
 * @param window_y Y coordinate in window space
 * @param window Window to extract dimensions from
 * @return Aspect-corrected NDC coordinates
 */
MAYAFLUX_API glm::vec3 normalize_coords_aspect(double window_x, double window_y,
    const std::shared_ptr<Core::Window>& window);

/**
 * @brief Check if a point in window coordinates is inside the window bounds
 * @param window_x X coordinate in window space
 * @param window_y Y coordinate in window space
 * @param window_width Window width in pixels
 * @param window_height Window height in pixels
 * @return true if point is within [0, width) × [0, height)
 */
MAYAFLUX_API bool is_in_bounds(double window_x, double window_y,
    uint32_t window_width, uint32_t window_height);

/**
 * @brief Check if a point in window coordinates is inside the window bounds
 * @param window_x X coordinate in window space
 * @param window_y Y coordinate in window space
 * @param window Window to extract dimensions from
 * @return true if point is within window bounds
 */
MAYAFLUX_API bool is_in_bounds(double window_x, double window_y,
    const std::shared_ptr<Core::Window>& window);

}
