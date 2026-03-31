#pragma once

#include "MayaFlux/Kinesis/NavigationState.hpp"

namespace MayaFlux::Buffers {
class RenderProcessor;
}

namespace MayaFlux::Core {
class Window;
}

namespace MayaFlux {

/// @brief Alias for backwards compatibility; prefer Kinesis::NavigationConfig in new code.
using ViewportPresetConfig = Kinesis::NavigationConfig;

/**
 * @enum ViewportPresetMode
 * @brief Selects which navigation controller bind_viewport_preset installs.
 *
 * Each mode registers a distinct set of event handlers and may interpret
 * ViewportPresetConfig fields differently. Modes not yet implemented emit a
 * runtime error and return without binding.
 */
enum class ViewportPresetMode : uint8_t {
    Fly, ///< First-person fly: WASD/QE translate, RMB drag yaw/pitch, scroll dolly, KP ortho snaps
    // Orbit, ///< Tumble around a focal point (not yet implemented)
    // Trackball, ///< Virtual trackball (not yet implemented)
    // PanZoom2D, ///< Orthographic 2D pan and zoom (not yet implemented)
};

/**
 * @brief Bind a navigation preset to a window and render processor.
 *
 * Registers event handlers under the prefix "vp_<name>_".
 * The specific handlers installed depend on @p mode; see ViewportPresetMode.
 *
 * Currently only Fly mode is implemented, which does the following:
 * Registers event handlers under the prefix "vp_<name>_":
 *   W/S/A/D   forward, backward, strafe left, strafe right
 *   Q/E       move down, move up along world Y
 *   RMB drag  yaw and pitch (inverted)
 *   Scroll    dolly along view direction
 *   KP1       front view
 *   KP3       right view
 *   KP7       top view
 *   KP9       flip to opposite view
 *
 * @param window     Window supplying input events
 * @param processor  RenderProcessor that receives the ViewTransform source
 * @param mode    Navigation controller to install, defaults to Fly preset
 * @param config     Tuning parameters
 * @param name       Unique prefix for registered events; must be unique per window
 */
MAYAFLUX_API void bind_viewport_preset(
    const std::shared_ptr<Core::Window>& window,
    const std::shared_ptr<Buffers::RenderProcessor>& processor,
    ViewportPresetMode mode = ViewportPresetMode::Fly,
    const ViewportPresetConfig& config = {},
    const std::string& name = "default");

/**
 * @brief Bind a navigation preset to all RenderProcessors currently registered
 *        against a window.
 *
 * Calls bind_viewport_preset(window, rp, mode, config, name) for every buffer
 * registered with the window at call time that returns a non-null RenderProcessor.
 * Buffers registered after this call are not covered.
 *
 * @param window  Window supplying input events
 * @param mode    Navigation controller to install, defaults to Fly preset
 * @param config  Tuning parameters
 * @param name    Preset name forwarded to bind_viewport_preset()
 */
MAYAFLUX_API void bind_viewport_preset(
    const std::shared_ptr<Core::Window>& window,
    ViewportPresetMode mode = ViewportPresetMode::Fly,
    const ViewportPresetConfig& config = {},
    const std::string& name = "default");

/**
 * @brief Cancel all event handlers registered by bind_viewport_preset() and
 *        restore the window input config to its state before bind was called.
 *
 * @param window  Window passed to bind_viewport_preset()
 * @param name    Name passed to bind_viewport_preset()
 */
MAYAFLUX_API void unbind_viewport_preset(
    const std::shared_ptr<Core::Window>& window,
    const std::string& name = "default");

} // namespace MayaFlux
