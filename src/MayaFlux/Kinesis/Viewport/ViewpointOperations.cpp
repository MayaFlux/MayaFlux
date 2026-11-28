#include "ViewpointOperations.hpp"

#include <glm/gtc/quaternion.hpp>

namespace MayaFlux::Kinesis {

namespace {

    /** False for a destination or a frustum that frustum_projection cannot image. */
    bool images(const glm::mat4& projection, float aspect)
    {
        return std::isfinite(aspect) && aspect > 0.0F && projection != glm::mat4(1.0F);
    }

    /** Point of the plane z = depth in eye coordinates that projects to ndc. */
    glm::vec2 preimage(const glm::mat4& projection, glm::vec2 ndc, float depth)
    {
        const float w = projection[2][3] * depth + projection[3][3];
        return {
            (ndc.x * w - projection[2][0] * depth - projection[3][0]) / projection[0][0],
            (ndc.y * w - projection[2][1] * depth - projection[3][1]) / projection[1][1],
        };
    }

}

Viewpoint rotate_viewpoint_about(
    const Viewpoint& view, const glm::vec3& centre,
    float azimuth, float elevation, const glm::vec3& axis)
{
    const glm::mat4& placement = view.first;
    const glm::vec3 up = glm::normalize(axis);
    const glm::vec3 forward = -glm::normalize(glm::vec3(placement[2]));

    glm::vec3 side = glm::cross(forward, up);
    side = glm::length(side) > 1.0e-6F ? glm::normalize(side) : glm::normalize(glm::vec3(placement[0]));

    constexpr float k_limit = glm::radians(89.0F);
    const float present = std::asin(glm::clamp(glm::dot(forward, up), -1.0F, 1.0F));
    const float wanted = glm::clamp(present + elevation,
        std::min(-k_limit, present), std::max(k_limit, present));

    const glm::mat4 turn = glm::rotate(glm::mat4(1.0F), azimuth, up)
        * glm::rotate(glm::mat4(1.0F), wanted - present, side);

    return {
        glm::translate(glm::mat4(1.0F), centre) * turn
            * glm::translate(glm::mat4(1.0F), -centre) * placement,
        view.second
    };
}

Viewpoint rotate_viewpoint(
    const Viewpoint& view, float azimuth, float elevation, const glm::vec3& axis)
{
    return rotate_viewpoint_about(view, glm::vec3(view.first[3]), azimuth, elevation, axis);
}

Viewpoint translate_viewpoint_to_image(
    const Viewpoint& view, const glm::vec3& point, glm::vec2 ndc, float aspect)
{
    const ViewTransform vt = view_from_frustum(view.first, view.second, aspect);
    const glm::vec3 seen = glm::vec3(vt.view * glm::vec4(point, 1.0F));

    if (!images(vt.projection, aspect)
        || vt.projection[2][3] * seen.z + vt.projection[3][3] <= 0.0F) {
        return view;
    }

    const glm::vec2 shift = glm::vec2(seen) - preimage(vt.projection, ndc, seen.z);

    Viewpoint moved = view;
    moved.first[3] += glm::vec4(
        glm::normalize(glm::vec3(view.first[0])) * shift.x
            + glm::normalize(glm::vec3(view.first[1])) * shift.y,
        0.0F);
    return moved;
}

Viewpoint translate_viewpoint_toward(
    const Viewpoint& view, const glm::vec3& point, float fraction)
{
    if (!(fraction < 1.0F)) {
        return view;
    }

    Viewpoint moved = view;
    moved.first[3] = glm::vec4(glm::mix(glm::vec3(view.first[3]), point, fraction), 1.0F);
    return moved;
}

Viewpoint scale_viewpoint_window(
    const Viewpoint& view, float factor, glm::vec2 ndc, float aspect)
{
    const glm::mat4 projection = frustum_projection(view.second, aspect);
    if (!(factor > 0.0F) || !images(projection, aspect)) {
        return view;
    }

    const glm::vec2 fixed = preimage(projection, ndc, -1.0F);

    Viewpoint scaled = view;
    scaled.second.window.min = fixed + (view.second.window.min - fixed) * factor;
    scaled.second.window.max = fixed + (view.second.window.max - fixed) * factor;
    return scaled;
}

Viewpoint translate_viewpoint(const Viewpoint& view, const glm::vec3& displacement)
{
    Viewpoint moved = view;
    for (int column = 0; column < 3; ++column) {
        moved.first[3] += glm::vec4(
            glm::normalize(glm::vec3(view.first[column])) * displacement[column], 0.0F);
    }
    return moved;
}

Viewpoint align_viewpoint(
    const Viewpoint& view, const glm::vec3& centre, const glm::vec3& direction,
    const glm::vec3& axis)
{
    if (!(glm::length(direction) > 0.0F)) {
        return view;
    }

    const glm::vec3 forward = glm::normalize(direction);
    glm::vec3 right = glm::cross(forward, glm::normalize(axis));

    if (glm::length(right) <= 1.0e-6F) {
        const glm::vec3 present = glm::normalize(glm::vec3(view.first[0]));
        right = present - forward * glm::dot(present, forward);
        if (glm::length(right) <= 1.0e-6F) {
            right = glm::cross(forward, glm::normalize(glm::vec3(view.first[1])));
        }
    }
    right = glm::normalize(right);

    const float distance = glm::distance(glm::vec3(view.first[3]), centre);
    return {
        glm::mat4(
            glm::vec4(right, 0.0F),
            glm::vec4(glm::cross(right, forward), 0.0F),
            glm::vec4(-forward, 0.0F),
            glm::vec4(centre - forward * distance, 1.0F)),
        view.second
    };
}

Viewpoint mix_viewpoints(const Viewpoint& from, const Viewpoint& to, float t)
{
    const float at = std::clamp(t, 0.0F, 1.0F);
    const glm::quat turn = glm::slerp(
        glm::normalize(glm::quat_cast(glm::mat3(from.first))),
        glm::normalize(glm::quat_cast(glm::mat3(to.first))), at);

    glm::mat4 placement = glm::mat4_cast(turn);
    placement[3] = glm::vec4(glm::mix(glm::vec3(from.first[3]), glm::vec3(to.first[3]), at), 1.0F);

    const Frustum& a = from.second;
    const Frustum& b = to.second;
    Frustum frustum = at < 0.5F ? a : b;
    frustum.window.min = glm::mix(a.window.min, b.window.min, at);
    frustum.window.max = glm::mix(a.window.max, b.window.max, at);
    frustum.near = a.near + (b.near - a.near) * static_cast<double>(at);
    frustum.far = a.far + (b.far - a.far) * static_cast<double>(at);
    return { placement, frustum };
}

} // namespace MayaFlux::Kinesis
