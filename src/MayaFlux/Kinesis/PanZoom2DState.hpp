#pragma once

#include "ViewTransform.hpp"

namespace MayaFlux::Kinesis {

/**
 * @struct PanZoom2DConfig
 * @brief Construction parameters for a 2D pan/zoom orthographic controller.
 */
struct PanZoom2DConfig {
    glm::vec2 initial_pan { 0.0F, 0.0F }; ///< Initial pan offset in world units
    float initial_zoom { 1.0F }; ///< Initial half-height of the orthographic frustum
    float scroll_speed { 0.1F }; ///< Fractional zoom change per scroll tick
    float min_zoom { 0.001F }; ///< Minimum half-height (maximum zoom in)
    float max_zoom { 10000.0F }; ///< Maximum half-height (maximum zoom out)
    float near_plane { -1.0F };
    float far_plane { 1.0F };
};

/**
 * @struct PanZoom2DState
 * @brief Mutable 2D pan/zoom navigation state.
 *
 * Pan offset and zoom (orthographic half-height) are the only mutable fields.
 * The ViewTransform is purely orthographic: view is identity, projection is
 * derived each frame as ortho(-zoom*aspect + pan.x, zoom*aspect + pan.x,
 * -zoom + pan.y, zoom + pan.y).
 *
 * Construct via make_pan_zoom_state(). Update via apply_pan_zoom_pan() and
 * apply_pan_zoom_scroll(), then call compute_pan_zoom_view_transform() each frame.
 */
struct PanZoom2DState {
    glm::vec2 pan { 0.0F, 0.0F }; ///< World-space pan offset
    float zoom { 1.0F }; ///< Orthographic half-height

    float scroll_speed { 0.1F };
    float min_zoom { 0.001F };
    float max_zoom { 10000.0F };
    float near_plane { -1.0F };
    float far_plane { 1.0F };

    bool drag_held { false };
    bool first_mouse { true };
    double last_x { 0.0 };
    double last_y { 0.0 };
};

/**
 * @brief Construct a PanZoom2DState from a PanZoom2DConfig.
 * @param config Source configuration.
 * @return Initialised PanZoom2DState.
 */
[[nodiscard]] MAYAFLUX_API PanZoom2DState make_pan_zoom_state(const PanZoom2DConfig& config);

/**
 * @brief Build a ViewTransform from the current PanZoom2DState.
 *
 * View matrix is identity. Projection is a symmetric ortho frustum centred
 * on state.pan with half-height state.zoom.
 *
 * @param state  PanZoom2D state (read-only).
 * @param aspect Framebuffer width / height.
 * @return ViewTransform ready for push constant upload.
 */
[[nodiscard]] MAYAFLUX_API ViewTransform compute_pan_zoom_view_transform(
    const PanZoom2DState& state, float aspect);

/**
 * @brief Pan by a pixel delta, scaled to world units by the current zoom level.
 *
 * Scaling by zoom ensures pan speed stays proportionate to zoom level.
 *
 * @param state  PanZoom2D state (pan mutated).
 * @param dx              Horizontal pixel delta (positive = right).
 * @param dy              Vertical pixel delta (positive = up).
 * @param viewport_width  Framebuffer width in pixels.
 * @param viewport_height Framebuffer height in pixels.
 */
MAYAFLUX_API void apply_pan_zoom_pan(
    PanZoom2DState& state, float dx, float dy,
    float viewport_width, float viewport_height);

/**
 * @brief Zoom by multiplying the half-height, clamped to [min_zoom, max_zoom].
 *
 * @param state PanZoom2D state (zoom mutated).
 * @param ticks Signed scroll ticks (positive = zoom in).
 */
MAYAFLUX_API void apply_pan_zoom_scroll(PanZoom2DState& state, float ticks);

} // namespace MayaFlux::Kinesis
