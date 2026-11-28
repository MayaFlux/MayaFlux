#pragma once

#include "Frustum.hpp"

namespace MayaFlux::Kinesis {

/**
 * @brief Rotate a viewpoint rigidly about a point.
 *
 * The centre keeps its image and its distance from the eye. Elevation stops
 * 1 degree short of looking along @p axis.
 *
 * @param view      Viewpoint to start from.
 * @param centre    World point held fixed.
 * @param azimuth   Radians about @p axis, by the right hand rule.
 * @param elevation Radians toward @p axis.
 * @param axis      Reference direction, any nonzero length.
 */
[[nodiscard]] MAYAFLUX_API Viewpoint rotate_viewpoint_about(
    const Viewpoint& view, const glm::vec3& centre,
    float azimuth, float elevation,
    const glm::vec3& axis = { 0.0F, 1.0F, 0.0F });

/** @brief rotate_viewpoint_about the eye itself, which stays where it is. */
[[nodiscard]] MAYAFLUX_API Viewpoint rotate_viewpoint(
    const Viewpoint& view, float azimuth, float elevation,
    const glm::vec3& axis = { 0.0F, 1.0F, 0.0F });

/**
 * @brief Translate parallel to the image plane until a world point has a given image.
 *
 * An aspect or frustum that cannot be imaged leaves the viewpoint unchanged, as
 * it does for scale_viewpoint_window.
 *
 * @param view   Viewpoint to start from.
 * @param point  World point. Behind a perspective eye the viewpoint is returned unchanged.
 * @param ndc    Image to give it, x and y in [-1, 1], +y up.
 * @param aspect Width over height of the destination.
 */
[[nodiscard]] MAYAFLUX_API Viewpoint translate_viewpoint_to_image(
    const Viewpoint& view, const glm::vec3& point, glm::vec2 ndc, float aspect);

/**
 * @brief Move the eye a fraction of the way to a point, keeping orientation.
 *
 * A negative fraction moves away. A fraction of 1 or more returns the
 * viewpoint unchanged.
 */
[[nodiscard]] MAYAFLUX_API Viewpoint translate_viewpoint_toward(
    const Viewpoint& view, const glm::vec3& point, float fraction);

/**
 * @brief Scale the frustum window, keeping what is imaged at @p ndc there.
 *
 * @param view   Viewpoint to start from.
 * @param factor Ratio of new size to old. Non-positive returns the viewpoint unchanged.
 * @param ndc    Fixed point of the scaling on screen.
 * @param aspect Width over height of the destination.
 */
[[nodiscard]] MAYAFLUX_API Viewpoint scale_viewpoint_window(
    const Viewpoint& view, float factor, glm::vec2 ndc, float aspect);

} // namespace MayaFlux::Kinesis
