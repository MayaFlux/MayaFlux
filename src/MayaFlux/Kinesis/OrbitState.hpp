#pragma once

#include "ViewTransform.hpp"

namespace MayaFlux::Kinesis {

/**
 * @struct OrbitConfig
 * @brief Construction parameters for an orbit navigation controller.
 */
struct OrbitConfig {
    glm::vec3 focal_point { 0.0F, 0.0F, 0.0F }; ///< World-space point the camera orbits around
    float initial_distance { 5.0F }; ///< Initial radius from focal point
    float initial_azimuth { 0.0F }; ///< Initial horizontal angle in radians
    float initial_elevation { glm::radians(20.0F) }; ///< Initial vertical angle in radians
    float fov_radians { glm::radians(60.0F) };
    float near_plane { 0.01F };
    float far_plane { 1000.0F };
    float rotate_sensitivity { 0.005F }; ///< Radians per pixel for MMB drag
    float pan_sensitivity { 0.001F }; ///< World units per pixel, scaled by distance
    float scroll_speed { 0.1F }; ///< Fractional distance change per scroll tick
    float min_distance { 0.1F }; ///< Minimum dolly distance
    float max_distance { 10000.0F }; ///< Maximum dolly distance
};

/**
 * @struct OrbitState
 * @brief Mutable orbit navigation state.
 *
 * Stores spherical coordinates relative to a focal point. Eye position is
 * derived each frame: focal + spherical_to_cartesian(azimuth, elevation, distance).
 *
 * Construct via make_orbit_state(). Update by calling the apply_* free
 * functions on mouse/scroll events, then call compute_orbit_view_transform()
 * each frame.
 */
struct OrbitState {
    glm::vec3 focal { 0.0F, 0.0F, 0.0F }; ///< World-space orbit target
    float azimuth { 0.0F }; ///< Horizontal angle in radians
    float elevation { glm::radians(20.0F) }; ///< Vertical angle in radians, clamped away from poles
    float distance { 5.0F }; ///< Radius from focal point

    float fov_radians { glm::radians(60.0F) };
    float near_plane { 0.01F };
    float far_plane { 1000.0F };
    float rotate_sensitivity { 0.005F };
    float pan_sensitivity { 0.001F };
    float scroll_speed { 0.1F };
    float min_distance { 0.1F };
    float max_distance { 10000.0F };

    bool mmb_held { false };
    bool pan_held { false };
    bool first_mouse { true };
    double last_x { 0.0 };
    double last_y { 0.0 };
};

/**
 * @brief Construct an OrbitState from an OrbitConfig.
 * @param config Source configuration.
 * @return Initialised OrbitState.
 */
[[nodiscard]] MAYAFLUX_API OrbitState make_orbit_state(const OrbitConfig& config);

/**
 * @brief Derive eye position from the current orbit state.
 *
 * Pure computation; does not mutate state.
 *
 * @param state Read-only orbit state.
 * @return World-space eye position.
 */
[[nodiscard]] MAYAFLUX_API glm::vec3 orbit_eye(const OrbitState& state);

/**
 * @brief Build a ViewTransform from the current OrbitState.
 *
 * Pure read; does not mutate state.
 *
 * @param state  Orbit state.
 * @param aspect Framebuffer width / height.
 * @return ViewTransform ready for push constant upload.
 */
[[nodiscard]] MAYAFLUX_API ViewTransform compute_orbit_view_transform(
    const OrbitState& state, float aspect);

/**
 * @brief Apply a mouse delta to azimuth and elevation.
 *
 * Elevation is clamped to [-89, +89] degrees to avoid gimbal lock at the poles.
 *
 * @param state Orbit state (azimuth, elevation mutated).
 * @param dx    Horizontal pixel delta (positive = right).
 * @param dy    Vertical pixel delta (positive = down).
 */
MAYAFLUX_API void apply_orbit_rotate(OrbitState& state, float dx, float dy);

/**
 * @brief Dolly the camera toward or away from the focal point.
 *
 * Distance is multiplied by (1 - ticks * scroll_speed) and clamped to
 * [min_distance, max_distance].
 *
 * @param state Orbit state (distance mutated).
 * @param ticks Signed scroll ticks (positive = closer).
 */
MAYAFLUX_API void apply_orbit_scroll(OrbitState& state, float ticks);

/**
 * @brief Pan the focal point in the camera's local right/up plane.
 *
 * Delta is scaled by distance so pan speed stays proportionate to zoom level.
 *
 * @param state Orbit state (focal mutated).
 * @param dx    Horizontal pixel delta.
 * @param dy    Vertical pixel delta.
 */
MAYAFLUX_API void apply_orbit_pan(OrbitState& state, float dx, float dy);

/**
 * @brief Snap to a named ortho view, preserving distance and focal point.
 *
 * @param state Orbit state (azimuth, elevation mutated).
 * @param view  0 = front (+Z), 1 = right (+X), 2 = top (+Y), 3 = flip opposite.
 */
MAYAFLUX_API void snap_orbit_ortho(OrbitState& state, int view);

} // namespace MayaFlux::Kinesis
