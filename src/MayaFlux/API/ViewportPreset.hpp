#pragma once

#include "MayaFlux/Kinesis/Spatial/Bounds.hpp"
#include "MayaFlux/Kinesis/Viewport/Frustum.hpp"
#include "MayaFlux/Transitive/Memory/OwnedCell.hpp"

#include "MayaFlux/Kinesis/Viewport/NavigationMappings.hpp"
#include "MayaFlux/Kinesis/Viewport/NavigationState.hpp"

namespace MayaFlux::Buffers {
class RenderProcessor;
}

namespace MayaFlux::Core {
class Window;
}



namespace MayaFlux {

/**
 * @brief What binding a preset gives back: the viewpoint it drives and the controller's id on it.
 *
 * Hand the viewpoint to another driver to take the view from the controller, and back to the id
 * for the controller to carry on from wherever it was left. Every processor of one binding draws
 * from the one viewpoint.
 */
using ViewportBinding = std::pair<std::shared_ptr<Memory::OwnedCell<Kinesis::Viewpoint>>, uint32_t>;

/// @brief Alias for backwards compatibility; prefer Kinesis::NavigationConfig in new code.
using ViewportPresetConfig = Kinesis::NavigationConfig;

/**
 * @enum ViewportPresetMode
 * @brief Selects which navigation controller bind_viewport_preset installs.
 *
 * Each mode registers a distinct set of event handlers and may interpret
 * ViewportPresetConfig fields differently.
 *
 * Bindings on one window do not act together. One binding takes a press, a
 * scroll or a key: of those whose region holds the pointer, the last bound or
 * raised. A Forma element under the pointer comes before any binding, and so do
 * keys its focused element listens for.
 */
enum class ViewportPresetMode : uint8_t {
    Fly, ///< First-person fly: WASD/QE translate, RMB drag yaw/pitch, scroll dolly, KP ortho snaps
    Orbit, ///< Rotation about a pivot, pan and distance
    PanZoom2D, ///< Orthographic 2D pan, and zoom about the cursor
    Screenspace, ///< Perspective pan in camera's local right/up plane, scroll dolly, no rotation
    // Trackball, ///< Virtual trackball (not yet implemented)
};

/**
 * @brief Bind a navigation preset to a window and render processor using default settings.
 *
 * Convenience wrapper that constructs a default config for the selected mode
 * and forwards to the corresponding bind_*_preset function. To tune the
 * controller, call bind_fly_preset(), bind_orbit_preset(), or
 * bind_pan_zoom_preset() directly.
 *
 * Registers event handlers named for this window and @p name, so one name may be bound on several windows.
 *
 * Mode defaults:
 *   Fly       - WASD/QE translate, RMB drag yaw/pitch, scroll dolly, KP ortho snaps
 *   Orbit     - MMB rotate around focal point, Shift+MMB pan, scroll dolly, KP ortho snaps
 *   PanZoom2D - MMB drag pan, scroll zoom, orthographic projection
 *
 * @param window     Window supplying input events
 * @param processor  RenderProcessor that draws from the viewpoint
 * @param mode       Navigation controller to install, defaults to Fly
 * @param name       Unique prefix for registered events; must be unique per window
 */
MAYAFLUX_API ViewportBinding bind_viewport_preset(
    const std::shared_ptr<Core::Window>& window,
    const std::shared_ptr<Buffers::RenderProcessor>& processor,
    ViewportPresetMode mode = ViewportPresetMode::Fly,
    const std::string& name = "default");

/**
 * @brief Bind a navigation preset to all RenderProcessors currently registered
 *        against a window using default settings.
 *
 * One viewpoint and one controller for every buffer registered with the window at call time
 * that has a RenderProcessor whose view is not pinned, which leaves Forma's
 * buffers alone.
 * Buffers registered after this call are not covered.
 *
 * @param window  Window supplying input events
 * @param mode    Navigation controller to install, defaults to Fly
 * @param name    Preset name forwarded to bind_viewport_preset()
 */
MAYAFLUX_API ViewportBinding bind_viewport_preset(
    const std::shared_ptr<Core::Window>& window,
    ViewportPresetMode mode = ViewportPresetMode::Fly,
    const std::string& name = "default");

/**
 * @brief Bind the fly navigation controller to a window and render processor.
 *
 * Registers event handlers named for this window and @p name, so one name may be bound on several windows. Key assignments
 * default to WASD/QE translation with KP ortho snaps; pass a FlyKeyMap to
 * override any or all bindings. Ortho snap slots set to std::nullopt are
 * silently skipped.
 *
 * @param window     Window supplying input events
 * @param processor  RenderProcessor that draws from the viewpoint
 * @param config     Camera/motion tuning parameters
 * @param key_map    Key assignments; defaults to WASD/QE + KP snaps
 * @param name       Unique prefix for registered events; must be unique per window
 */
MAYAFLUX_API ViewportBinding bind_fly_preset(
    const std::shared_ptr<Core::Window>& window,
    const std::shared_ptr<Buffers::RenderProcessor>& processor,
    const ViewportPresetConfig& config = {},
    const Kinesis::FlyKeyMap& key_map = {},
    const std::string& name = "default");

/**
 * @brief Bind the fly navigation controller to all RenderProcessors currently
 *        registered against a window.
 *
 * One viewpoint and one controller for every buffer registered with the window at call time
 * that has a RenderProcessor whose view is not pinned, which leaves Forma's
 * buffers alone.
 * Buffers registered after this call are not covered.
 *
 * @param window   Window supplying input events
 * @param config   Camera/motion tuning parameters
 * @param key_map  Key assignments; defaults to WASD/QE + KP snaps
 * @param name     Preset name forwarded to bind_fly_preset()
 */
MAYAFLUX_API ViewportBinding bind_fly_preset(
    const std::shared_ptr<Core::Window>& window,
    const ViewportPresetConfig& config = {},
    const Kinesis::FlyKeyMap& key_map = {},
    const std::string& name = "default");

/**
 * @brief Bind the orbit navigation controller to a window and render processor.
 *
 * Registers event handlers named for this window and @p name, so one name may be bound on several windows. MMB drag rotates
 * around the focal point; holding key_map.pan_modifier during MMB drag pans
 * the focal point instead. Scroll dollies the distance. Ortho snap slots set
 * to std::nullopt are silently skipped.
 *
 * @param window     Window supplying input events
 * @param processor  RenderProcessor that draws from the viewpoint
 * @param config     Orbit tuning parameters
 * @param key_map    Key assignments; defaults to Shift pan modifier + KP snaps
 * @param name       Unique prefix for registered events; must be unique per window
 */
MAYAFLUX_API ViewportBinding bind_orbit_preset(
    const std::shared_ptr<Core::Window>& window,
    const std::shared_ptr<Buffers::RenderProcessor>& processor,
    const Kinesis::OrbitConfig& config = {},
    const Kinesis::OrbitKeyMap& key_map = {},
    const std::string& name = "default");

/**
 * @brief Bind the orbit navigation controller to all RenderProcessors currently
 *        registered against a window.
 *
 * @param window   Window supplying input events
 * @param config   Orbit tuning parameters
 * @param key_map  Key assignments
 * @param name     Preset name forwarded to the per-processor overload
 */
MAYAFLUX_API ViewportBinding bind_orbit_preset(
    const std::shared_ptr<Core::Window>& window,
    const Kinesis::OrbitConfig& config = {},
    const Kinesis::OrbitKeyMap& key_map = {},
    const std::string& name = "default");

/**
 * @brief Bind the 2D pan/zoom controller to a window and render processor.
 *
 * Drag pans the view; scroll zooms by scaling the orthographic half-height.
 * View matrix is always identity; only the orthographic projection changes.
 *
 * @param window     Window supplying input events
 * @param processor  RenderProcessor that draws from the viewpoint
 * @param config     Pan/zoom tuning parameters
 * @param key_map    Input assignments; defaults to MMB drag
 * @param name       Unique prefix for registered events; must be unique per window
 */
MAYAFLUX_API ViewportBinding bind_pan_zoom_preset(
    const std::shared_ptr<Core::Window>& window,
    const std::shared_ptr<Buffers::RenderProcessor>& processor,
    const Kinesis::PanZoom2DConfig& config = {},
    const Kinesis::PanZoom2DKeyMap& key_map = {},
    const std::string& name = "default");

/**
 * @brief Bind the 2D pan/zoom controller to all RenderProcessors currently
 *        registered against a window.
 *
 * @param window   Window supplying input events
 * @param config   Pan/zoom tuning parameters
 * @param key_map  Input assignments
 * @param name     Preset name forwarded to the per-processor overload
 */
MAYAFLUX_API ViewportBinding bind_pan_zoom_preset(
    const std::shared_ptr<Core::Window>& window,
    const Kinesis::PanZoom2DConfig& config = {},
    const Kinesis::PanZoom2DKeyMap& key_map = {},
    const std::string& name = "default");

/**
 * @brief Bind the screenspace navigation controller to a window and render processor.
 *
 * Drag translates the eye along the camera's local right and up axes derived
 * from the current yaw and pitch. Rotation is never modified. Scroll dollies
 * along the forward vector. Perspective projection is preserved throughout.
 *
 * To set the initial viewing direction, supply a NavigationConfig with
 * initial_eye and initial_target; yaw and pitch are derived from those on
 * construction exactly as in bind_fly_preset.
 *
 * @param window     Window supplying input events
 * @param processor  RenderProcessor that draws from the viewpoint
 * @param config     Camera tuning parameters; move_speed is unused
 * @param key_map    Input assignments; defaults to RMB drag
 * @param name       Unique prefix for registered events; must be unique per window
 */
MAYAFLUX_API ViewportBinding bind_screenspace_preset(
    const std::shared_ptr<Core::Window>& window,
    const std::shared_ptr<Buffers::RenderProcessor>& processor,
    const Kinesis::NavigationConfig& config = {},
    const Kinesis::ScreenspaceKeyMap& key_map = {},
    const std::string& name = "default");

/**
 * @brief Bind the screenspace navigation controller to all RenderProcessors
 *        currently registered against a window.
 *
 * @param window   Window supplying input events
 * @param config   Camera tuning parameters
 * @param key_map  Input assignments
 * @param name     Preset name forwarded to the per-processor overload
 */
MAYAFLUX_API ViewportBinding bind_screenspace_preset(
    const std::shared_ptr<Core::Window>& window,
    const Kinesis::NavigationConfig& config = {},
    const Kinesis::ScreenspaceKeyMap& key_map = {},
    const std::string& name = "default");

/**
 * @brief Drive a viewpoint that already exists, such as a Locus camera or
 *        another preset's.
 *
 * The controller takes over from where the viewpoint stands, with the mode's
 * default settings. No processor is changed; whatever draws from the
 * viewpoint follows. PanZoom2D keeps its own lens when the viewpoint has a
 * perspective one.
 *
 * @param window  Window supplying input events
 * @param view    Viewpoint to drive
 * @param mode    Navigation controller to install
 * @param name    Unique prefix for registered events; must be unique per window
 */
MAYAFLUX_API ViewportBinding join_viewport_preset(
    const std::shared_ptr<Core::Window>& window,
    const std::shared_ptr<Memory::OwnedCell<Kinesis::Viewpoint>>& view,
    ViewportPresetMode mode = ViewportPresetMode::Fly,
    const std::string& name = "default");

/**
 * @brief Confine a binding to part of the window.
 *
 * It takes input only when the pointer is there, keys included, and pan and
 * zoom measure against that part, as for a view shown in an inset or drawn
 * into a render lane of that shape. std::nullopt gives it the whole window.
 *
 * @param window  Window passed to the bind
 * @param region  Rectangle in window NDC, +y up
 * @param name    Name passed to the bind
 */
MAYAFLUX_API void set_viewport_preset_region(
    const std::shared_ptr<Core::Window>& window,
    const std::optional<Kinesis::AABB2D>& region,
    const std::string& name = "default");

/**
 * @brief Confine a binding to a shape, in addition to any region.
 *
 * It takes input only where @p within holds for the pointer in window NDC, +y
 * up, such as inside a circle or over what a placed element covers. Pan and zoom
 * still measure against the region. An empty function removes it.
 *
 * @param window  Window passed to the bind
 * @param within  Test of a window NDC point
 * @param name    Name passed to the bind
 */
MAYAFLUX_API void set_viewport_preset_shape(
    const std::shared_ptr<Core::Window>& window,
    std::function<bool(glm::vec2)> within,
    const std::string& name = "default");

/** @brief Put a binding ahead of the other bindings of its window. */
MAYAFLUX_API void raise_viewport_preset(
    const std::shared_ptr<Core::Window>& window,
    const std::string& name = "default");

/**
 * @brief Cancel all event handlers registered under @p name, restore the
 *        window input config and stop driving the viewpoint.
 *
 * What draws from the viewpoint keeps the view as it was left.
 *
 * @param window  Window passed to bind_viewport_preset()
 * @param name    Name passed to bind_viewport_preset()
 */
MAYAFLUX_API void unbind_viewport_preset(
    const std::shared_ptr<Core::Window>& window,
    const std::string& name = "default");

} // namespace MayaFlux
