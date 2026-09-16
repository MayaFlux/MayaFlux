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
 * @struct Tracked
 * @brief One element registered with a Scrollable for reflow on scroll.
 */
struct Tracked {
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
 * it: track() registers an element id plus the caller's own reposition
 * callback, invoked with that element's scrolled bounds on every scroll.
 * This is the same shape as Collapsible::attach(): Scrollable moves bounds
 * and asks the caller's callback to keep visuals in sync, it does not
 * reach into arbitrary geometry functions to do that itself. indicator()
 * follows the same principle for a scroll readout: Scrollable computes the
 * two numbers (position, extent) and drives sync() on the caller's own
 * element, but draws nothing itself.
 *
 * Wheel input over the viewport is wired by place() as the default
 * trigger, but scroll_by() is a plain method: any other input source (a
 * dragged indicator, a key, MIDI, anything) can drive scrolling by calling
 * it directly, with no dependency on Context::on_scroll at all.
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
     *                 track()ed element with a buffer gets.
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
     * @brief Register an element for scroll reflow.
     *
     * Expands scroll->content_bounds to include base_bounds, so the clamp
     * in scroll_by() always reflects everything tracked so far - no manual
     * content-height bookkeeping needed. On every scroll, this element's
     * bounds_hint is updated to base_bounds.translated(offset) and
     * reposition is called with that same shifted region: reposition owns
     * whatever resubmission its own content needs (a raw buffer resubmit,
     * rebuilding a Mapped<T>'s geometry, anything). Scrollable does not
     * inspect or constrain what kind of content id is.
     *
     * base_bounds is normalized against the current offset before it is
     * stored, so it is always safe to pass cursor_out.advance()'s return
     * directly - that return is already shifted by whatever offset is
     * live right now (that is the whole point of LayoutCursor::bind_scroll:
     * an element placed after scrolling already happened lands in the right
     * spot immediately). Without this normalization an element tracked
     * while scrolled would have that offset baked into its anchor and
     * drift further out of place on every scroll after.
     *
     * @param id          Element id, already registered on the same Layer
     *                    place() used.
     * @param base_bounds This element's current on-screen NDC bounds - the
     *                    direct return of cursor_out.advance(), scrolled or not.
     * @param reposition  Called with the scrolled bounds on every scroll.
     * @param clip_buf    Optional. When given, Kinesis::Scissor::from(viewport_bounds)
     *                    is set on its render processor immediately, so the
     *                    element is clipped to the viewport from the start.
     * @return *this, so place()/track()/indicator() can chain into one
     *         expression for the common case of a single tracked item -
     *         see Portal::Forma::TextField::scrollable() for exactly that.
     */
    MAYAFLUX_API Scrollable& track(
        uint32_t id,
        Kinesis::AABB2D base_bounds,
        std::function<void(Kinesis::AABB2D)> reposition,
        const std::shared_ptr<Buffers::FormaBuffer>& clip_buf = nullptr);

    /**
     * @brief Apply a scroll delta and reflow every tracked element.
     *
     * The input-agnostic core: place() wires wheel input to this by
     * default, but any other trigger can call it directly with its own
     * delta - nothing here assumes the source.
     *
     * @param layer Layer the viewport and every tracked element were
     *              registered on.
     * @param delta Content-space scroll delta, added to the current
     *              offset and clamped to scroll->content_bounds.
     */
    MAYAFLUX_API void scroll_by(Layer& layer, glm::vec2 delta);

    /**
     * @brief A self-contained wheel-scroll handler bound to this
     *        Scrollable's own state.
     *
     * place() wires exactly this behind the viewport's own on_scroll, but
     * Context::handle_scroll dispatches to only the topmost element under
     * the cursor (Layer::hit_test), so content drawn on top of the
     * viewport - anything track()ed, since it registers after and so hit-
     * tests first - never reaches the viewport's own on_scroll while the
     * cursor sits over it. A caller in that position (TextField::
     * scrollable() is exactly this case) registers the returned function
     * on its own element's on_scroll instead of duplicating the wheel-
     * delta-to-content-delta conversion by hand.
     *
     * Captures only this Scrollable's own shared state (scroll, offset,
     * the tracked list, the indicator), the same shared_ptrs place()'s own
     * wiring captures - safe to keep registered after this Scrollable
     * value itself is destroyed, same as every other Context callback this
     * struct wires.
     *
     * @param surface Surface owning the same Layer/Context place() used.
     * @return A Context::ScrollFn-shaped callable, ready for on_scroll.
     */
    [[nodiscard]] MAYAFLUX_API std::function<void(uint32_t, glm::vec2, double, double)>
    wheel_handler(Surface& surface) const;

    /**
     * @brief Attach a caller-built element as a live, draggable scroll
     *        indicator.
     *
     * Scrollable draws nothing itself. Build whatever Mapped<glm::vec2>
     * your own geometry function produces - a bar, a squiggle, a sampled
     * texture, anything computable from (position_frac, extent_frac) - via
     * Portal::Forma::create<glm::vec2>, register it, and hand it here.
     * position_frac is 0 at the top of content_bounds and 1 at the fully
     * scrolled bottom; extent_frac is the viewport/content height ratio
     * (how much of the content is visible at once), so a geometry function
     * can size its own drawing without asking Scrollable anything further.
     *
     * On every scroll - wheel (place()'s default wiring), scroll_by(), or
     * dragging this same element - the live fraction pair is written to
     * el.state and el.sync() is called, the same "call sync() straight
     * from the handler that changed the value, no polling" idiom
     * Collapsible's own on_press already uses. track() also refreshes
     * extent_frac (without a Layer, so el's hit region only, not
     * bounds_hint, can lag very slightly until the next scroll -
     * self-corrects on the first interaction after).
     *
     * Also wires on_drag on el.element.id: dragging maps the cursor's NDC
     * y absolutely onto content offset within @p track, the same
     * inverse-mapping idiom Geometry::vertical_fader uses for its own
     * handle, routed through scroll_by() so drag and wheel scrolling stay
     * in sync through one path. No handle-size compensation is applied,
     * unlike vertical_fader - an indicator has no fixed rendered size for
     * Scrollable to know about.
     *
     * @param surface Surface owning the same Layer/Context place() used.
     * @param el      Caller's Mapped<glm::vec2>, already registered on
     *                surface's Layer.
     * @param track   NDC region the drag is measured against; its full
     *                height maps to the full scrollable range.
     * @return *this, for chaining - see track()'s @return.
     */
    MAYAFLUX_API Scrollable& indicator(Surface& surface, Mapped<glm::vec2> el, Kinesis::AABB2D track);

private:
    std::shared_ptr<std::vector<Tracked>> m_tracked;
    std::shared_ptr<Mapped<glm::vec2>> m_indicator;
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
