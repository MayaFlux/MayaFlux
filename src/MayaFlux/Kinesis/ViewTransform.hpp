#pragma once

#include <glm/gtc/matrix_transform.hpp>

namespace MayaFlux::Kinesis {

/**
 * @struct ViewTransform
 * @brief View and projection matrices as a named push constant slot
 *
 * Two glm::mat4 fields (128 bytes total) that map directly to a vertex
 * shader push constant block. No Camera class, no scene graph, no
 * transform hierarchy. The third dimension is not special: it is a
 * push constant slot that the user may optionally supply, which
 * enables depth testing and provides view/projection matrices to the
 * vertex shader.
 *
 * Construction uses free functions (look_at, perspective, ortho) or
 * direct assignment. Both matrices can be driven by signal graph node
 * outputs for audio-reactive or algorithmically controlled viewpoints.
 */
struct ViewTransform {
    glm::mat4 view { 1.0F };
    glm::mat4 projection { 1.0F };
};

static_assert(sizeof(ViewTransform) == 128,
    "ViewTransform must be exactly 128 bytes (Vulkan minimum push constant size)");

/**
 * @brief Construct view matrix from eye position, target, and up vector
 * @param eye Observer position in source space
 * @param target Point the observer is looking at
 * @param up Up direction (default: positive Y)
 * @return ViewTransform with view matrix set, projection identity
 */
[[nodiscard]] inline ViewTransform look_at(
    const glm::vec3& eye,
    const glm::vec3& target,
    const glm::vec3& up = glm::vec3(0.0F, 1.0F, 0.0F))
{
    return { .view = glm::lookAt(eye, target, up) };
}

/**
 * @brief Construct perspective projection matrix
 * @param fov_radians Vertical field of view in radians
 * @param aspect Width / height aspect ratio
 * @param near Near clip plane distance
 * @param far Far clip plane distance
 * @return ViewTransform with projection matrix set, view identity
 */
[[nodiscard]] inline ViewTransform perspective(
    float fov_radians,
    float aspect,
    float near,
    float far)
{
    return { .projection = glm::perspective(fov_radians, aspect, near, far) };
}

/**
 * @brief Construct orthographic projection matrix
 * @param left Left clipping plane
 * @param right Right clipping plane
 * @param bottom Bottom clipping plane
 * @param top Top clipping plane
 * @param near Near clipping plane (default: -1.0)
 * @param far Far clipping plane (default: 1.0)
 * @return ViewTransform with projection matrix set, view identity
 */
[[nodiscard]] inline ViewTransform ortho(
    float left, float right,
    float bottom, float top,
    float near = -1.0F, float far = 1.0F)
{
    return { .projection = glm::ortho(left, right, bottom, top, near, far) };
}

/**
 * @brief Construct complete ViewTransform from look-at and perspective parameters
 * @param eye Observer position in source space
 * @param target Point the observer is looking at
 * @param fov_radians Vertical field of view in radians
 * @param aspect Width / height aspect ratio
 * @param near Near clip plane distance
 * @param far Far clip plane distance
 * @param up Up direction (default: positive Y)
 * @return ViewTransform with both view and projection matrices set
 */
[[nodiscard]] inline ViewTransform look_at_perspective(
    const glm::vec3& eye,
    const glm::vec3& target,
    float fov_radians,
    float aspect,
    float near,
    float far,
    const glm::vec3& up = glm::vec3(0.0F, 1.0F, 0.0F))
{
    return {
        .view = glm::lookAt(eye, target, up),
        .projection = glm::perspective(fov_radians, aspect, near, far)
    };
}

} // namespace MayaFlux::Kinesis
