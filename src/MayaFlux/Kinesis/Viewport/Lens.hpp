#pragma once

#include "ViewTransform.hpp"

namespace MayaFlux::Kinesis {

/**
 * @struct ScreenWindow
 * @brief The rectangle a lens looks through, in half widths of its film back.
 *
 * Measured on the plane one unit of tan(half the horizontal field of view) from the lens, so the
 * film back of a spherical lens runs from -1 to +1 across and from -h to +h up, where h is the
 * film's height over its width. Film offset, overscan, lens squeeze and the film back's own
 * transform are already folded in, as Alembic's screen window gives them.
 */
struct ScreenWindow {
    double left { -1.0 };
    double right { 1.0 };
    double bottom { -2.0 / 3.0 };
    double top { 2.0 / 3.0 };

    [[nodiscard]] double width() const noexcept { return right - left; }
    [[nodiscard]] double height() const noexcept { return top - bottom; }
};

/**
 * @struct Lens
 * @brief The optics of a viewpoint as a file or a user describes them: focal length, film back and clip planes.
 *
 * The defaults are Alembic's: a 35 mm lens on a 3.6 by 2.4 cm film back. Lengths of the film are in
 * centimetres and the focal length in millimetres, as DCC applications write them. f_stop and
 * focus_distance do not affect the projection and are kept for whoever draws depth of field.
 */
struct Lens {
    double focal_length { 35.0 }; ///< Millimetres.
    double horizontal_aperture { 3.6 }; ///< Film back width, centimetres.
    double vertical_aperture { 2.4 }; ///< Film back height, centimetres.
    ScreenWindow window; ///< What the film back covers, with offset, overscan and squeeze applied.
    double near { 0.1 };
    double far { 100000.0 };
    double f_stop { 5.6 };
    double focus_distance { 5.0 };
};

/**
 * @enum FilmFit
 * @brief How a film back is fitted into an output of another aspect ratio.
 */
enum class FilmFit : uint8_t {
    Fill, ///< The film covers the whole output and what overhangs is cropped.
    Horizontal, ///< The film's width spans the output and its height follows.
    Vertical, ///< The film's height spans the output and its width follows.
    Overscan ///< The whole film is seen inside the output, with bars where it does not reach.
};

/**
 * @brief Half the horizontal field of view's tangent: how far a ray at unit distance reaches to the edge of the film back.
 */
[[nodiscard]] inline double lens_half_tangent(const Lens& lens) noexcept
{
    return lens.horizontal_aperture * 10.0 / (2.0 * lens.focal_length);
}

/** @brief The horizontal field of view in radians. */
[[nodiscard]] inline double lens_horizontal_fov(const Lens& lens) noexcept
{
    return 2.0 * std::atan(lens_half_tangent(lens));
}

/** @brief The vertical field of view in radians the film back's height gives. */
[[nodiscard]] inline double lens_vertical_fov(const Lens& lens) noexcept
{
    return 2.0 * std::atan(lens.vertical_aperture * 10.0 / (2.0 * lens.focal_length));
}

/**
 * @brief A lens of a vertical field of view over a film of an aspect ratio, for a lens nobody wrote down.
 * @param vertical_fov Radians.
 * @param aspect Width over height of the film back.
 */
[[nodiscard]] inline Lens lens_from_fov(double vertical_fov, double aspect, double near, double far)
{
    Lens lens;
    lens.vertical_aperture = lens.horizontal_aperture / aspect;
    lens.focal_length = lens.vertical_aperture * 10.0 / (2.0 * std::tan(vertical_fov * 0.5));
    lens.window = { .left = -1.0, .right = 1.0, .bottom = -1.0 / aspect, .top = 1.0 / aspect };
    lens.near = near;
    lens.far = far;
    return lens;
}

/**
 * @brief The projection of a lens into an output of an aspect ratio.
 * @param lens The optics.
 * @param aspect Output width over height, above zero.
 * @param fit How the film back is fitted when its own aspect differs.
 *
 * An off-axis frustum: the film back's window, scaled to the tangents of the lens, fitted
 * into the aspect, then glm::frustum. A lens whose focal length or window is not usable gives
 * the identity.
 */
[[nodiscard]] inline glm::mat4 lens_projection(const Lens& lens, float aspect, FilmFit fit = FilmFit::Fill)
{
    const double reach = lens_half_tangent(lens);
    const double width = lens.window.width();
    const double height = lens.window.height();
    if (!(lens.focal_length > 0.0) || !(reach > 0.0) || !(width > 0.0) || !(height > 0.0)
        || !(aspect > 0.0F) || !(lens.near > 0.0) || !(lens.far > lens.near)) {
        return glm::mat4(1.0F);
    }

    double left = lens.window.left;
    double right = lens.window.right;
    double bottom = lens.window.bottom;
    double top = lens.window.top;

    const double film_aspect = width / height;
    const bool by_width = [&] {
        switch (fit) {
        case FilmFit::Horizontal:
            return true;
        case FilmFit::Vertical:
            return false;
        case FilmFit::Overscan:
            return aspect < film_aspect;
        case FilmFit::Fill:
        default:
            return aspect >= film_aspect;
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

    return glm::frustum(
        static_cast<float>(left * reach * lens.near), static_cast<float>(right * reach * lens.near),
        static_cast<float>(bottom * reach * lens.near), static_cast<float>(top * reach * lens.near),
        static_cast<float>(lens.near), static_cast<float>(lens.far));
}

/**
 * @brief The view and projection of a lens placed in the world.
 * @param placement Where the viewer is placed, as its own matrix. Scale and shear in it are
 *        dropped, since a view moves and turns and never stretches.
 * @param lens The optics.
 * @param aspect Output width over height.
 * @param fit How the film back is fitted into the output.
 *
 * The lens looks down its own negative z with y up, as Alembic and glm's look_at do.
 */
[[nodiscard]] inline ViewTransform view_from_lens(
    const glm::mat4& placement, const Lens& lens, float aspect, FilmFit fit = FilmFit::Fill)
{
    glm::mat4 rigid = placement;
    for (int column = 0; column < 3; ++column) {
        const glm::vec3 axis(rigid[column]);
        const float length = glm::length(axis);
        if (length > 1.0e-8F) {
            rigid[column] = glm::vec4(axis / length, 0.0F);
        }
    }
    return { .view = glm::inverse(rigid), .projection = lens_projection(lens, aspect, fit) };
}

} // namespace MayaFlux::Kinesis
