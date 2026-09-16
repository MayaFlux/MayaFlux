#pragma once

#include "LayoutCursor.hpp"

#include "MayaFlux/Kinesis/Viewport/ScrollState.hpp"

namespace MayaFlux::Buffers {
class FormaBuffer;
}

namespace MayaFlux::Portal::Forma {

class Layer;
class Surface;

/**
 * @struct ScrollableChild
 * @brief One element tracked by a Scrollable for reflow on scroll.
 */
struct ScrollableChild {
    uint32_t id {};
    Kinesis::AABB2D base_bounds {};
    std::function<void(Kinesis::AABB2D)> reposition;
};

/**
 * @struct Scrollable
 * @brief A clipped, scroll-offset viewport over content taller than it.
 *
 * Before place() is called, the struct carries only configuration. After
 * place(), viewport_id, scroll, offset, buf, and cursor_out are populated.
 * This mirrors the Element/Collapsible pattern: configure with setters,
 * register with a single explicit call.
 *
 * Scrollable owns a background/hit-test element sized to the viewport and
 * a LayoutCursor (cursor_out) scoped to that viewport, already bound to the
 * live scroll offset via LayoutCursor::bind_scroll, so content placed
 * through it lands at the correct on-screen position immediately, scrolled
 * or not. It does not own or know what kind of content is placed inside
 * it: track() registers a child id plus the caller's own reposition
 * callback, invoked with the child's scrolled bounds on every scroll. This
 * is the same shape as Collapsible::attach(): Scrollable moves bounds and
 * asks the caller's callback to keep visuals in sync, it does not reach
 * into arbitrary geometry functions to do that itself.
 *
 * Wheel input over the viewport is wired by place() as the default
 * trigger, but scroll_by() is a plain method: any other input source (a
 * dragged scrollbar thumb, a key, MIDI, anything) can drive scrolling by
 * calling it directly, with no dependency on Context::on_scroll at all.
 *
 * @code
 * auto panel = Scrollable {}
 *     .background_color({ 0.08F, 0.08F, 0.1F })
 *     .place(buf, surface, viewport);
 *
 * const Kinesis::AABB2D bounds = panel.cursor_out.advance(row_h);
 * auto row_buf = Portal::Forma::create_buffer(
 *     window, Kinesis::filled_rect(bounds, color),
 *     Portal::Graphics::PrimitiveTopology::TRIANGLE_STRIP);
 * const uint32_t id = surface.layer().add(
 *     Element {}.with_bounds(bounds).with_buffer(row_buf).non_interactive());
 *
 * panel.track(id, bounds,
 *     [row_buf, color](Kinesis::AABB2D shifted) {
 *         row_buf->submit(Kinesis::filled_rect(shifted, color));
 *     },
 *     row_buf);
 * @endcode
 */
struct Scrollable {
    // =========================================================================
    // Results - populated by place()
    // =========================================================================

    /// @brief Element id of the viewport background. Valid after place().
    uint32_t viewport_id {};

    /// @brief Clamped scroll offset and content/viewport extents. Valid after place().
    std::shared_ptr<Kinesis::ScrollState> scroll;

    /// @brief Reactive mirror of scroll->offset, bound into cursor_out. Valid after place().
    std::shared_ptr<MappedState<glm::vec2>> offset;

    /// @brief FormaBuffer backing the viewport background. Valid after place().
    std::shared_ptr<Buffers::FormaBuffer> buf;

    /// @brief Cursor scoped to the viewport, bound to offset. Valid after place().
    LayoutCursor cursor_out;

    /// @brief NDC region occupied by the viewport. Valid after place().
    Kinesis::AABB2D viewport_bounds {};

    /// @brief Viewport region, satisfying Kinesis::HasBounds.
    [[nodiscard]] Kinesis::AABB2D bounds() const noexcept { return viewport_bounds; }

    // =========================================================================
    // Configuration - set before place()
    // =========================================================================

    /// @brief Background fill color. Default: glm::vec3(0.08F, 0.08F, 0.1F).
    glm::vec3 m_background { 0.08F, 0.08F, 0.1F };

    /// @brief Content-space units scrolled per wheel tick. Default: 0.05F.
    float m_wheel_speed { 0.05F };

    // =========================================================================
    // Configuration setters
    // =========================================================================

    Scrollable& background_color(glm::vec3 c)
    {
        m_background = c;
        return *this;
    }

    Scrollable& wheel_speed(float s)
    {
        m_wheel_speed = s;
        return *this;
    }

    // =========================================================================
    // Placement
    // =========================================================================

    /**
     * @brief Register the viewport element, wire wheel scrolling, and
     *        populate scroll, offset, buf, and cursor_out.
     *
     * @param in_buf   Pre-created buffer for the viewport background.
     * @param surface  Surface to register the viewport on.
     * @param viewport NDC region: hit-test bounds, wheel-scroll trigger
     *                 region, background quad, and the Scissor clip every
     *                 track()ed child with a buffer gets.
     * @return *this, with results populated.
     */
    MAYAFLUX_API Scrollable& place(
        std::shared_ptr<Buffers::FormaBuffer> in_buf,
        Surface& surface,
        Kinesis::AABB2D viewport);

    // =========================================================================
    // Post-placement
    // =========================================================================

    /**
     * @brief Register a child element for scroll reflow.
     *
     * Expands scroll->content_bounds to include base_bounds, so the clamp
     * in scroll_by() always reflects everything tracked so far - no manual
     * content-height bookkeeping needed. On every scroll, this child's
     * bounds_hint is updated to base_bounds.translated(offset) and
     * reposition is called with that same shifted region: reposition owns
     * whatever resubmission its own content needs (a raw buffer resubmit,
     * rebuilding a Mapped<T>'s geometry, anything). Scrollable does not
     * inspect or constrain what kind of content child_id is.
     *
     * base_bounds is normalized against the current offset before it is
     * stored, so it is always safe to pass cursor_out.advance()'s return
     * directly - that return is already shifted by whatever offset is
     * live right now (that is the whole point of LayoutCursor::bind_scroll:
     * a child placed after scrolling already happened lands in the right
     * spot immediately). Without this normalization a child tracked while
     * scrolled would have that offset baked into its anchor and drift
     * further out of place on every scroll after.
     *
     * @param child_id    Element id, already registered on the same Layer
     *                    place() used.
     * @param base_bounds This child's current on-screen NDC bounds - the
     *                    direct return of cursor_out.advance(), scrolled or not.
     * @param reposition  Called with the scrolled bounds on every scroll.
     * @param clip_buf    Optional. When given, Kinesis::Scissor::from(viewport_bounds)
     *                    is set on its render processor immediately, so the
     *                    child is clipped to the viewport from the start.
     */
    MAYAFLUX_API void track(
        uint32_t child_id,
        Kinesis::AABB2D base_bounds,
        std::function<void(Kinesis::AABB2D)> reposition,
        const std::shared_ptr<Buffers::FormaBuffer>& clip_buf = nullptr);

    /**
     * @brief Apply a scroll delta and reflow every tracked child.
     *
     * The input-agnostic core: place() wires wheel input to this by
     * default, but any other trigger can call it directly with its own
     * delta - nothing here assumes the source.
     *
     * @param layer Layer the viewport and every tracked child were
     *              registered on.
     * @param delta Content-space scroll delta, added to the current
     *              offset and clamped to scroll->content_bounds.
     */
    MAYAFLUX_API void scroll_by(Layer& layer, glm::vec2 delta);

private:
    std::shared_ptr<std::vector<ScrollableChild>> m_children;
};

// =============================================================================
// Free function escape hatch
// =============================================================================

/**
 * @brief Construct a scrollable viewport.
 *
 * Thin wrapper delegating to Scrollable::place(). Preserved as an escape
 * hatch, matching make_collapsible.
 */
[[nodiscard]] MAYAFLUX_API Scrollable make_scrollable(
    std::shared_ptr<Buffers::FormaBuffer> buf,
    Surface& surface,
    Kinesis::AABB2D viewport,
    glm::vec3 background_color = glm::vec3(0.08F, 0.08F, 0.1F),
    float wheel_speed = 0.05F);

} // namespace MayaFlux::Portal::Forma
