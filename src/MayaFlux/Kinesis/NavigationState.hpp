#pragma once

#include "ViewTransform.hpp"

namespace MayaFlux::Kinesis {

/**
 * @struct NavigationConfig
 * @brief Tuning parameters for a first-person fly-navigation controller.
 */
struct NavigationConfig {
    glm::vec3 initial_eye { 0.0F, 0.0F, 5.0F };
    glm::vec3 initial_target { 0.0F, 0.0F, 0.0F };
    float fov_radians { glm::radians(60.0F) };
    float near_plane { 0.01F };
    float far_plane { 1000.0F };
    float move_speed { 3.0F }; ///< World units per second
    float mouse_sensitivity { 0.002F }; ///< Radians per pixel
    float scroll_speed { 0.5F }; ///< World units per scroll tick
};

/**
 * @struct NavigationState
 * @brief Mutable first-person navigation state.
 *
 * Holds eye position, orientation angles, per-axis movement flags, and
 * mouse tracking state. All fields are plain data; no engine or event
 * system dependency.
 *
 * Construct via make_navigation_state() to derive initial yaw/pitch from
 * NavigationConfig::initial_target. Update by mutating fields directly,
 * then call compute_view_transform() each frame.
 */
struct NavigationState {
    glm::vec3 eye { 0.0F, 0.0F, 5.0F };
    float yaw { 0.0F }; ///< Radians, horizontal rotation
    float pitch { 0.0F }; ///< Radians, vertical rotation, clamped to [-89, +89] degrees

    bool forward_held { false };
    bool back_held { false };
    bool left_held { false };
    bool right_held { false };
    bool down_held { false };
    bool up_held { false };

    bool rmb_held { false };
    bool first_mouse { true };
    double last_x { 0.0 };
    double last_y { 0.0 };

    float move_speed { 3.0F };
    float mouse_sensitivity { 0.002F };
    float scroll_speed { 0.5F };
    float fov_radians { glm::radians(60.0F) };
    float near_plane { 0.01F };
    float far_plane { 1000.0F };

    std::chrono::steady_clock::time_point last_tick { std::chrono::steady_clock::now() };
};

/**
 * @brief Construct a NavigationState from a NavigationConfig.
 *
 * Derives initial yaw and pitch from the look direction
 * (config.initial_target - config.initial_eye) so the first frame does
 * not produce a view jump.
 *
 * @param config Source configuration
 * @return Initialised NavigationState
 */
[[nodiscard]] NavigationState make_navigation_state(const NavigationConfig& config);

/**
 * @brief Compute a ViewTransform from the current NavigationState.
 *
 * Computes dt from state.last_tick, advances eye position by held movement
 * flags scaled by move_speed * dt, updates last_tick, then constructs view
 * and projection matrices.
 *
 * @param state  Navigation state (eye and last_tick mutated by this call)
 * @param aspect Framebuffer width / height
 * @return ViewTransform ready for push constant upload
 */
[[nodiscard]] ViewTransform compute_view_transform(NavigationState& state, float aspect);

/**
 * @brief Apply a mouse delta to yaw and pitch.
 *
 * Pitch is clamped to [-89, +89] degrees to avoid gimbal lock at the poles.
 *
 * @param state Navigation state (yaw, pitch mutated)
 * @param dx    Horizontal pixel delta (positive = right)
 * @param dy    Vertical pixel delta (positive = down)
 */
void apply_mouse_delta(NavigationState& state, float dx, float dy);

/**
 * @brief Dolly eye along the current forward vector.
 *
 * @param state Navigation state (eye mutated)
 * @param ticks Signed scroll ticks (positive = forward)
 */
void apply_scroll(NavigationState& state, float ticks);

/**
 * @brief Snap to a named ortho view.
 *
 * Preserves the current distance from the origin. Yaw/pitch are set to
 * exact axis-aligned values. The top view uses pitch = -89 degrees to
 * avoid the degenerate lookAt case at exactly -90 degrees.
 *
 * @param state Navigation state (eye, yaw, pitch mutated)
 * @param view  0 = front (+Z), 1 = right (+X), 2 = top (+Y), 3 = flip opposite
 */
void snap_ortho(NavigationState& state, int view);

} // namespace MayaFlux::Kinesis
