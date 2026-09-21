#pragma once

#include "Bounds.hpp"

namespace MayaFlux::Kinesis {

/**
 * @file Projection.hpp
 * @brief NDC-to-value mappings, inverse to the forward placement performed by
 *        geometry functions.
 *
 * Each function returns a callable suitable for direct use as the projection
 * argument of Portal::Forma::Geometry::wire_drag. They are pure: no window,
 * no context, no element. A projection paired with the geometry function that
 * shares its parameters is what makes a control draggable.
 *
 * Every function below also takes an optional @c live_offset callable,
 * defaulted to empty and a no-op when omitted. Every existing call site is
 * unaffected. When supplied, it shifts the cursor position into the frame
 * the projection's fixed geometry (bounds/center/path, captured at
 * construction) was computed in, rather than re-deriving that geometry per
 * call. This is the inverse of translating the drawn control by the same
 * offset. It has no dependency on Portal::Forma: the caller supplies the
 * accessor (e.g. a Scrollable's live offset), and this file only ever
 * calls it.
 */

/**
 * @brief Fraction along one axis of @p bounds, inverse to fader placement.
 *
 * horizontal_fader and vertical_fader place the handle's leading edge at
 * @c min + value * (extent - handle_extent), so the handle centre travels a
 * span shorter than the track by one handle. This subtracts half a handle
 * before dividing by the reduced span, so the handle centre tracks the cursor
 * across the whole range rather than lagging by up to half a handle at the
 * extremes.
 *
 * Pass @p handle_extent of zero for mappings with no handle inset, such as a
 * level_meter used as a scrub region.
 *
 * @param bounds        Track region in NDC.
 * @param handle_extent Handle width for horizontal, height for vertical.
 * @param horizontal    True to map the x axis, false for y.
 * @param live_offset   Optional. Current translation of the drawn control,
 *                       e.g. a Scrollable's live scroll offset.
 * @return Callable producing a value in [0, 1].
 */
[[nodiscard]] inline std::function<float(glm::vec2)>
axis_fraction(AABB2D bounds, float handle_extent = 0.F, bool horizontal = true,
    const std::function<glm::vec2()>& live_offset = {}) noexcept
{
    const float extent = horizontal ? bounds.width() : bounds.height();
    const float span = extent - handle_extent;
    const float origin = horizontal ? bounds.min.x : bounds.min.y;
    const float half = handle_extent * 0.5F;

    return [origin, half, span, horizontal, live_offset](glm::vec2 p) -> float {
        if (span <= 0.F)
            return 0.F;
        const float shift = live_offset ? (horizontal ? live_offset().x : live_offset().y) : 0.F;
        const float pos = (horizontal ? p.x : p.y) - shift;
        return std::clamp((pos - origin - half) / span, 0.F, 1.F);
    };
}

/**
 * @brief Unit-square coordinates of a point within @p bounds.
 *
 * Inverse to position_picker, which maps [0,1]² onto the region.
 *
 * @param bounds      Region in NDC.
 * @param live_offset Optional. Current translation of the drawn control.
 * @return Callable producing a value in [0, 1]².
 */
[[nodiscard]] inline std::function<glm::vec2(glm::vec2)>
unit_square(AABB2D bounds, const std::function<glm::vec2()>& live_offset = {}) noexcept
{
    const glm::vec2 origin = bounds.min;
    const glm::vec2 extent { bounds.width(), bounds.height() };

    return [origin, extent, live_offset](glm::vec2 p) -> glm::vec2 {
        if (extent.x <= 0.F || extent.y <= 0.F)
            return glm::vec2(0.F);
        const glm::vec2 shift = live_offset ? live_offset() : glm::vec2(0.F);
        return glm::clamp((p - shift - origin) / extent, glm::vec2(0.F), glm::vec2(1.F));
    };
}

/**
 * @brief Fraction along an angular sweep about @p center.
 *
 * Inverse to radial, which places the indicator at
 * @c angle_start + value * (angle_end - angle_start). Handles sweeps in
 * either direction and sweeps crossing the atan2 discontinuity. The cursor's
 * distance from the centre is ignored, so the control remains responsive
 * outside the drawn radius, which is what a knob gesture expects.
 *
 * Points at the exact centre return zero rather than an undefined angle.
 *
 * @param center      Sweep centre in NDC.
 * @param angle_start Angle in radians corresponding to value 0.
 * @param angle_end   Angle in radians corresponding to value 1.
 * @param live_offset Optional. Current translation of the drawn control.
 * @return Callable producing a value in [0, 1].
 */
[[nodiscard]] inline std::function<float(glm::vec2)>
angle_fraction(glm::vec2 center, float angle_start, float angle_end,
    const std::function<glm::vec2()>& live_offset = {}) noexcept
{
    const float delta = angle_end - angle_start;

    return [center, angle_start, delta, live_offset](glm::vec2 p) -> float {
        constexpr float k_two_pi = 6.283185307179586F;

        if (std::abs(delta) < 1e-6F)
            return 0.F;

        const glm::vec2 shift = live_offset ? live_offset() : glm::vec2(0.F);
        const glm::vec2 d = (p - shift) - center;
        if (glm::dot(d, d) < 1e-12F)
            return 0.F;

        float rel = std::fmod(std::atan2(d.y, d.x) - angle_start, k_two_pi);
        if (delta > 0.F && rel < 0.F) {
            rel += k_two_pi;
        } else if (delta < 0.F && rel > 0.F) {
            rel -= k_two_pi;
        }

        return std::clamp(rel / delta, 0.F, 1.F);
    };
}

/**
 * @brief angle_fraction centred on a region.
 *
 * Companion to the region-taking radial overload, which derives its centre
 * from the region in the same way.
 */
[[nodiscard]] inline std::function<float(glm::vec2)>
angle_fraction(AABB2D region, float angle_start, float angle_end,
    const std::function<glm::vec2()>& live_offset = {}) noexcept
{
    return angle_fraction(region.center(), angle_start, angle_end, live_offset);
}

/**
 * @brief Normalized arc-length position of the closest point on a polyline.
 *
 * Inverse to stroke_slider, which places the handle at a fraction of the
 * path's total arc length. Finds the nearest point across all segments and
 * returns its cumulative length divided by the total, so the handle follows
 * the cursor along the path regardless of how far off the path it strays.
 *
 * Cumulative lengths are computed once and captured; the returned callable
 * allocates nothing per invocation beyond an optional live_offset() call.
 *
 * @param points      Ordered polyline vertices in NDC. Copied into the closure.
 * @param live_offset Optional. Current translation of the drawn path -
 *                     shifts the cursor into the path's own frame rather
 *                     than re-translating every point per call.
 * @return Callable producing a value in [0, 1]. Returns 0 for paths with
 *         fewer than two points or zero total length.
 */
[[nodiscard]] inline std::function<float(glm::vec2)>
path_fraction(std::span<const glm::vec2> points, const std::function<glm::vec2()>& live_offset = {})
{
    std::vector<glm::vec2> pts(points.begin(), points.end());
    std::vector<float> cumulative(pts.size(), 0.F);

    for (size_t i = 1; i < pts.size(); ++i)
        cumulative[i] = cumulative[i - 1] + glm::length(pts[i] - pts[i - 1]);

    const float total = pts.empty() ? 0.F : cumulative.back();

    return [pts = std::move(pts), cumulative = std::move(cumulative), total, live_offset](
               glm::vec2 p) -> float {
        if (pts.size() < 2 || total <= 0.F)
            return 0.F;

        const glm::vec2 shift = live_offset ? live_offset() : glm::vec2(0.F);
        const glm::vec2 q = p - shift;

        float best_d2 = std::numeric_limits<float>::max();
        float best_s = 0.F;

        for (size_t i = 0; i + 1 < pts.size(); ++i) {
            const glm::vec2 a = pts[i];
            const glm::vec2 ab = pts[i + 1] - a;
            const float len2 = glm::dot(ab, ab);

            const float t = len2 > 1e-12F
                ? glm::clamp(glm::dot(q - a, ab) / len2, 0.F, 1.F)
                : 0.F;

            const glm::vec2 diff = q - (a + t * ab);
            const float d2 = glm::dot(diff, diff);

            if (d2 < best_d2) {
                best_d2 = d2;
                best_s = cumulative[i] + t * std::sqrt(len2);
            }
        }

        return std::clamp(best_s / total, 0.F, 1.F);
    };
}

/**
 * @brief Rescale a normalized projection onto an arbitrary range.
 *
 * Composes over any callable producing [0, 1], so a fader, knob, or stroke
 * slider can drive a frequency, gain, or index without the call site
 * repeating the remap.
 *
 * @param norm Projection producing a value in [0, 1].
 * @param lo   Value corresponding to 0.
 * @param hi   Value corresponding to 1.
 */
[[nodiscard]] inline std::function<float(glm::vec2)>
scaled(std::function<float(glm::vec2)> norm, float lo, float hi)
{
    return [norm = std::move(norm), lo, hi](glm::vec2 p) -> float {
        return lo + norm(p) * (hi - lo);
    };
}

} // namespace MayaFlux::Kinesis
