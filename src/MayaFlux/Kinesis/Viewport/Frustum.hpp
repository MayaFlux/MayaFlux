#pragma once

#include "ViewTransform.hpp"

#include "MayaFlux/Kinesis/Spatial/Bounds.hpp"

namespace MayaFlux::Kinesis {

/**
 * @enum AspectFit
 * @brief How a window is fitted into an output of another aspect ratio.
 */
enum class AspectFit : uint8_t {
    Cover, ///< The window covers the whole output and what overhangs is cropped.
    Width, ///< The window's width spans the output and its height follows.
    Height, ///< The window's height spans the output and its width follows.
    Contain ///< The whole window is inside the output, with margins where it does not reach.
};

/**
 * @struct Frustum
 * @brief The volume a projection maps: a window and two clipping distances, looking down negative z.
 *
 * For a perspective frustum the window lies on the plane at distance one, so its edges are the
 * tangents of the half angles. For an orthographic one it is in scene units.
 */
struct Frustum {
    AABB2D window { .min = { -18.0F / 35.0F, -12.0F / 35.0F }, .max = { 18.0F / 35.0F, 12.0F / 35.0F } };
    double near { 0.1 };
    double far { 100000.0 };
    bool orthographic {};
    AspectFit fit { AspectFit::Cover };
};

/** @brief A viewpoint: a rigid placement looking down its own negative z, and the frustum from there. */
using Viewpoint = std::pair<glm::mat4, Frustum>;

/** @brief Whether two viewpoints are exactly the same placement and frustum. */
[[nodiscard]] inline bool same_viewpoint(const Viewpoint& a, const Viewpoint& b) noexcept
{
    const Frustum& l = a.second;
    const Frustum& r = b.second;
    return a.first == b.first && l.window.min == r.window.min && l.window.max == r.window.max
        && l.near == r.near && l.far == r.far && l.orthographic == r.orthographic && l.fit == r.fit;
}

/** @brief The horizontal angle of a perspective frustum in radians. */
[[nodiscard]] inline double frustum_horizontal_fov(const Frustum& frustum) noexcept
{
    return std::atan(static_cast<double>(frustum.window.max.x)) - std::atan(static_cast<double>(frustum.window.min.x));
}

/** @brief The vertical angle of a perspective frustum in radians. */
[[nodiscard]] inline double frustum_vertical_fov(const Frustum& frustum) noexcept
{
    return std::atan(static_cast<double>(frustum.window.max.y)) - std::atan(static_cast<double>(frustum.window.min.y));
}

/**
 * @brief A symmetric perspective frustum of a vertical angle and an aspect ratio, fitted by height.
 * @param vertical_fov Radians.
 * @param aspect Window width over height.
 */
[[nodiscard]] inline Frustum frustum_from_fov(double vertical_fov, double aspect, double near, double far)
{
    const auto up = static_cast<float>(std::tan(vertical_fov * 0.5));
    const auto across = static_cast<float>(std::tan(vertical_fov * 0.5) * aspect);
    return {
        .window = { .min = { -across, -up }, .max = { across, up } },
        .near = near,
        .far = far,
        .orthographic = false,
        .fit = AspectFit::Height
    };
}

/**
 * @brief The projection of a frustum into an output of an aspect ratio, by the frustum's fit.
 * @param frustum The volume.
 * @param aspect Output width over height, above zero.
 * @return The identity for a frustum whose window or distances are not usable.
 */
[[nodiscard]] inline glm::mat4 frustum_projection(const Frustum& frustum, float aspect)
{
    double left = frustum.window.min.x;
    double right = frustum.window.max.x;
    double bottom = frustum.window.min.y;
    double top = frustum.window.max.y;

    const double width = right - left;
    const double height = top - bottom;
    if (!(width > 0.0) || !(height > 0.0) || !(aspect > 0.0F)
        || (!frustum.orthographic && !(frustum.near > 0.0)) || !(frustum.far > frustum.near)) {
        return glm::mat4(1.0F);
    }

    const double window_aspect = width / height;
    const bool by_width = [&] {
        switch (frustum.fit) {
        case AspectFit::Width:
            return true;
        case AspectFit::Height:
            return false;
        case AspectFit::Contain:
            return aspect < window_aspect;
        case AspectFit::Cover:
        default:
            return aspect >= window_aspect;
        }
    }();

    if (by_width) {
        const double centre = 0.5 * (bottom + top);
        const double half = 0.5 * width / aspect;
        bottom = centre - half;
        top = centre + half;
    } else {
        const double centre = 0.5 * (left + right);
        const double half = 0.5 * height * aspect;
        left = centre - half;
        right = centre + half;
    }

    if (frustum.orthographic) {
        return glm::orthoRH_ZO(
            static_cast<float>(left), static_cast<float>(right),
            static_cast<float>(bottom), static_cast<float>(top),
            static_cast<float>(frustum.near), static_cast<float>(frustum.far));
    }

    return glm::frustumRH_ZO(
        static_cast<float>(left * frustum.near), static_cast<float>(right * frustum.near),
        static_cast<float>(bottom * frustum.near), static_cast<float>(top * frustum.near),
        static_cast<float>(frustum.near), static_cast<float>(frustum.far));
}

/**
 * @brief The view and projection of a frustum at a placement.
 * @param placement Where the viewer is, as its own matrix. Scale and shear in it are dropped,
 *        since a view moves and turns and never stretches.
 * @param frustum The volume seen from there.
 * @param aspect Output width over height.
 */
[[nodiscard]] inline ViewTransform view_from_frustum(
    const glm::mat4& placement, const Frustum& frustum, float aspect)
{
    glm::mat4 rigid = placement;
    for (int column = 0; column < 3; ++column) {
        const glm::vec3 axis(rigid[column]);
        const float length = glm::length(axis);
        if (length > 1.0e-8F) {
            rigid[column] = glm::vec4(axis / length, 0.0F);
        }
    }
    return { .view = glm::inverse(rigid), .projection = frustum_projection(frustum, aspect) };
}

} // namespace MayaFlux::Kinesis
