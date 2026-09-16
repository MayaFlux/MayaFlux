#pragma once

#include "ViewTransform.hpp"

#include "MayaFlux/Kinesis/Spatial/Bounds.hpp"

namespace MayaFlux::Kinesis {

/**
 * @brief Convert a pixel-space drag delta into a content-space delta.
 *
 * Scales the fraction of the viewport a pointer moved by the content
 * extent currently visible, so drag speed stays proportionate to scale:
 * more content in view moves the result further per pixel than less. No
 * sign convention is imposed; a caller applies whatever sign its own
 * semantics need (a grabbed canvas moves opposite the drag, a dragged
 * scrollbar thumb moves with it).
 *
 * Shared by PanZoom2DState::apply_pan_zoom_pan and any CPU-side drag
 * scroll: both are "drag the content by moving the viewport's frame over
 * it," differing only in what consumes the result (a GPU ViewTransform
 * versus a CPU layout offset).
 *
 * @param pixel_delta          Pixel-space delta.
 * @param visible_extent       Currently visible content extent (width, height), same units as the return value.
 * @param viewport_size_pixels Viewport size in pixels the delta was measured against.
 */
[[nodiscard]] MAYAFLUX_API glm::vec2 drag_to_offset_delta(
    glm::vec2 pixel_delta,
    glm::vec2 visible_extent,
    glm::vec2 viewport_size_pixels) noexcept;

/**
 * @struct ScrollState
 * @brief CPU-side scroll offset for a bounded content region.
 *
 * The interactable half of scrolling. offset is a content-space
 * translation, clamped so a viewport of viewport_bounds's size never
 * scrolls past content_bounds. offset increases as the view moves deeper
 * into content: zero shows the top-left of content_bounds, the clamped
 * maximum shows its bottom-right.
 *
 * content_bounds is not clamped to NDC. It is whatever extent the content
 * actually occupies (the accumulated height of every row a LayoutCursor
 * has advanced through, typically), routinely far taller than the
 * [-1, 1] a viewport can show at once - that excess is the whole point of
 * scrolling.
 *
 * A consumer applies offset however its own rendering model wants it.
 * Portal::Forma::LayoutCursor folds it directly into placed bounds (CPU
 * reflow, always hit-test-correct by construction, since the same value
 * that moved the geometry is available to move the hit region too). A
 * buffer wanting to shift many vertices at once without a per-element CPU
 * reflow can instead derive a ViewTransform via scroll_view_transform()
 * and skip that per-element work, at the cost of needing to remap
 * hit-test coordinates by the same offset itself: nothing does that
 * remapping automatically, so a GPU-only shift and CPU hit-testing will
 * disagree the moment offset is nonzero unless something closes that loop.
 */
struct ScrollState {
    glm::vec2 offset { 0.F, 0.F };
    AABB2D content_bounds { .min = glm::vec2(-1.F), .max = glm::vec2(1.F) };
    AABB2D viewport_bounds { .min = glm::vec2(-1.F), .max = glm::vec2(1.F) };
};

/**
 * @brief Clamp a content-space offset so a viewport of the given extent
 *        never scrolls past the edges of a content region, per axis.
 *
 * An axis where content is smaller than the viewport clamps to zero
 * rather than to a degenerate range: there is nothing to scroll on that
 * axis.
 */
[[nodiscard]] MAYAFLUX_API glm::vec2 clamp_offset_to_content(
    glm::vec2 offset,
    glm::vec2 viewport_extent,
    AABB2D content_bounds) noexcept;

/**
 * @brief Add a content-space delta to the offset and clamp to content_bounds.
 */
MAYAFLUX_API void apply_scroll(ScrollState& state, glm::vec2 delta) noexcept;

/**
 * @brief Apply a wheel-tick delta, scaled by speed, and clamp.
 *
 * Sign matches whatever convention Context::on_scroll's dx/dy already
 * deliver on this platform; not independently verified interactively, so
 * flip the sign here first if a wheel test scrolls backward.
 *
 * @param state Scroll state (offset mutated).
 * @param dx, dy Wheel delta, as delivered by Context::ScrollFn.
 * @param speed Content-space units per wheel tick.
 */
MAYAFLUX_API void apply_wheel_scroll(ScrollState& state, double dx, double dy, float speed = 0.08F) noexcept;

/**
 * @brief Build an orthographic ViewTransform that translates by +offset.
 *
 * For content already authored directly in NDC, as every Forma geometry
 * function is, this is view = translate(offset), projection = identity:
 * the vertex shader's projection * view * position reduces to
 * position + offset, the same shift LayoutCursor applies on the CPU side.
 *
 * Meant for a buffer that wants to shift many vertices as one GPU-side
 * move instead of a per-element CPU reflow. Hit-testing against elements
 * shifted this way still needs the same offset applied to the cursor
 * position before testing them - see ScrollState's own doc comment; this
 * function only produces the render-side half.
 */
[[nodiscard]] MAYAFLUX_API ViewTransform scroll_view_transform(const ScrollState& state) noexcept;

} // namespace MayaFlux::Kinesis
