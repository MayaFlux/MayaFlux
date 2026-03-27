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
 * @brief Bind a first-person fly-navigation preset to a window and render processor.
 *
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
 * @param config     Tuning parameters
 * @param name       Unique prefix for registered events; must be unique per window
 */
MAYAFLUX_API void bind_viewport_preset(
    const std::shared_ptr<Core::Window>& window,
    const std::shared_ptr<Buffers::RenderProcessor>& processor,
    const ViewportPresetConfig& config = {},
    const std::string& name = "default");

/**
 * @brief Bind a viewport preset to all RenderProcessors currently registered
 *        against a window.
 *
 * Calls bind_viewport_preset(window, rp, config, name) for every buffer
 * registered with the window at call time that returns a non-null RenderProcessor.
 * Buffers registered after this call are not covered.
 *
 * @param window  Window supplying input events
 * @param config  Tuning parameters
 * @param name    Preset name forwarded to bind_viewport_preset()
 */
MAYAFLUX_API void bind_viewport_preset(
    const std::shared_ptr<Core::Window>& window,
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
