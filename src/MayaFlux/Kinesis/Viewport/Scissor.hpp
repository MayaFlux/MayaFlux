#pragma once

#include "ViewTransform.hpp"

#include "MayaFlux/Kinesis/Spatial/Bounds.hpp"

namespace MayaFlux::Kinesis {

/**
 * @struct Scissor
 * @brief NDC clip region for a draw, resolved to a pixel rect against a
 *        live framebuffer size wherever it is applied.
 *
 * Not a rendering-backend type and not Forma-specific: it lives in Kinesis
 * because anything with a spatial footprint can produce one. A 2D bounds
 * region converts directly; anything exposing bounds() (Portal::Forma's
 * Mapped<T>, Collapsible, ValueRow, ValueGroup, and so on) converts through
 * the same HasBounds mechanism place() already uses; a 3D world-space
 * AABB3D converts by projecting through a ViewTransform. Portal::Graphics::
 * RenderConfig holds one as std::optional<Scissor>, and
 * Buffers::RenderProcessor resolves it to a backend rect (Vulkan's
 * vk::Rect2D) fresh every frame.
 *
 * bounds are NDC (matches AABB2D convention: x/y in [-1,1], center origin,
 * +Y up), so one Scissor stays correct across a resize; nothing here is
 * resolved to pixels until the frame that draws it.
 */
struct Scissor {
    AABB2D bounds { .min = glm::vec2(-1.F), .max = glm::vec2(1.F) };

    [[nodiscard]] bool operator==(const Scissor& other) const noexcept
    {
        return bounds.min == other.bounds.min && bounds.max == other.bounds.max;
    }

    /// @brief Construct directly from an NDC AABB2D.
    [[nodiscard]] static Scissor from(AABB2D ndc_bounds) noexcept
    {
        return { .bounds = ndc_bounds };
    }

    /// @brief Construct from anything exposing bounds(): Mapped<T>, Collapsible, ValueRow, ValueGroup, and so on.
    template <HasBounds T>
    [[nodiscard]] static Scissor from(const T& anchor) noexcept
    {
        return { .bounds = bounds_of(anchor) };
    }

    /**
     * @brief Project a world-space AABB3D through a ViewTransform and take
     *        the NDC bounding box of its projected corners.
     *
     * Projects all 8 corners through projection * view, divides by
     * clip-space w, and takes the min/max of the resulting NDC x/y,
     * clamped to [-1,1]. A corner behind the eye (w <= 0) is skipped rather
     * than let it fold the bounding box onto itself, so an object crossing
     * the near plane still resolves from whichever corners are in front of
     * the camera. A box with no corner in front of the camera at all
     * returns the full-screen Scissor: there is nothing visible to clip to,
     * and the caller almost certainly should not be drawing it this frame
     * regardless of what the scissor rect says.
     */
    [[nodiscard]] static Scissor from(const AABB3D& world_bounds, const ViewTransform& view) noexcept
    {
        const glm::mat4 view_projection = view.projection * view.view;

        glm::vec2 lo { 1.F, 1.F };
        glm::vec2 hi { -1.F, -1.F };
        bool any_in_front = false;

        for (int i = 0; i < 8; ++i) {
            const glm::vec3 corner {
                (i & 1) ? world_bounds.max.x : world_bounds.min.x,
                (i & 2) ? world_bounds.max.y : world_bounds.min.y,
                (i & 4) ? world_bounds.max.z : world_bounds.min.z,
            };

            const glm::vec4 clip = view_projection * glm::vec4(corner, 1.F);
            if (clip.w <= 0.F)
                continue;

            const glm::vec2 ndc { clip.x / clip.w, clip.y / clip.w };
            lo = glm::min(lo, ndc);
            hi = glm::max(hi, ndc);
            any_in_front = true;
        }

        if (!any_in_front)
            return {};

        return { .bounds = {
                     .min = glm::clamp(lo, glm::vec2(-1.F), glm::vec2(1.F)),
                     .max = glm::clamp(hi, glm::vec2(-1.F), glm::vec2(1.F)),
                 } };
    }
};

} // namespace MayaFlux::Kinesis
