#pragma once

#include "Scrollable.hpp"

#include "MayaFlux/Portal/Text/InkPress.hpp"
#include "MayaFlux/Portal/Text/TextEdit.hpp"

namespace MayaFlux::Buffers {
class FormaBuffer;
}

namespace MayaFlux::Portal::Forma {

class Context;
class Surface;

/**
 * @struct TextField
 * @brief An editable text region: keyboard/text-input wiring over an
 *        Element the caller already has a buffer for.
 *
 * Before place() is called, the struct carries no state. After place(),
 * element_id, state, params, and field_bounds are populated. This mirrors
 * the Scrollable/Collapsible pattern: place() takes a pre-built buffer
 * (Portal::Forma::create_buffer at the call site) and does not itself
 * depend on Forma.hpp - no file under Portal/Forma/ may, except Forma.cpp
 * itself. Portal::Forma::create_text_field (in Forma.hpp) is the one-call
 * convenience that builds the buffer and delegates here, the same split
 * Portal::Forma::create<T> already has between itself and a Form<T>.
 *
 * Editing state lives in a Portal::Text::EditableText, mutated through
 * Portal::Text::move()/erase()/insert_codepoint()/insert_literal() - this
 * struct only wires Context callbacks to those operations and re-renders.
 * It does not manage focus visuals; wire on_focus_gained/on_focus_lost
 * separately, same as any other Forma element.
 *
 * Cursor movement beyond Left/Right/character-count needed no new
 * Portal::Text API: Up/Down resolve via x_at() (current pen position) and
 * index_at() (nearest byte at that x, one line_height() away) - the exact
 * idiom test_text_input() (test_8.hpp) already used to verify x_at()/
 * index_at() round-trip. A mouse click does the same, with the click's NDC
 * position mapped into pixel space first (the inverse of the mapping
 * caret_ndc() itself uses).
 *
 * A caret rides in the same buf as the text quad rather than owning a
 * buffer, Element, or Layer entry of its own: it is a second
 * Kakshya::MeshVertex quad (Kinesis::textured_mesh_rect at weight 0, a
 * flat fill - forma_multi.frag already branches per-vertex on weight, so
 * weight<=0 draws that vertex's own color with no texture read at all)
 * appended after the text quad's 6 vertices in the same buf->submit()
 * call. Every keystroke and every reposition() therefore resubmits both
 * quads together (12 MeshVertex, 720 bytes) rather than only the one that
 * actually moved - FormaBuffer::submit() has no partial-range write, so
 * this is the whole buffer either way, and at this size it costs nothing
 * to send it whole. Caret pixel position comes from Portal::Text::x_at()
 * plus GlyphAtlas::ascender()/line_height(), matching the same pen_x=0,
 * pen_y=ascender(), wrap_w=render_bounds.x origin press()/repress()
 * already rasterize text at internally - Portal::Text needed no new API
 * for this.
 *
 * params is a shared, mutable Portal::Text::PressParams: every keystroke's
 * re-render reads *params fresh, so a caller can change it once (e.g. a
 * focus-driven background color) and have every subsequent keystroke see
 * it, without re-wiring anything.
 *
 * buf is exposed (same convention Scrollable/Collapsible use) so a field
 * can be clipped or scrolled with no TextField-specific plumbing: pass it
 * directly as Scrollable::track()'s clip_buf to get scissor-to-viewport
 * clipping for free, and wire reposition() as the track() reposition
 * callback to move the field on scroll:
 *
 * @code
 * auto buf = Portal::Forma::create_buffer(
 *     window, Graphics::PrimitiveTopology::TRIANGLE_LIST,
 *     std::vector{ std::pair{ std::string("text"), std::shared_ptr<Core::VKImage>{} } });
 *
 * auto params = std::make_shared<Portal::Text::PressParams>(
 *     Portal::Text::PressParams { .color = { 0.9F, 0.9F, 0.9F, 1.F }, .render_bounds = { 800, 64 } });
 *
 * auto field = TextField {}.place(buf, surface, bounds, params, "hello");
 *
 * // Living inside a Scrollable, composed by hand:
 * auto panel = Scrollable {}.place(viewport_buf, surface, viewport_bounds);
 * panel.track(surface.layer(), field.element_id, field.bounds(),
 *     [field](Kinesis::AABB2D shifted) mutable { field.reposition(shifted); },
 *     field.buf);
 * @endcode
 *
 * scrollable() is the fluent equivalent of that same composition, for the
 * common single-field case - but the Scrollable itself is entirely the
 * caller's: build and configure it however (place(), indicator(),
 * background_color(), wheel_speed() - all fluent) before calling
 * scrollable(), which only needs a buffer for itself, that already-placed
 * Scrollable to track into, and how tall to grow relative to its viewport.
 * Everything Scrollable already owns (the viewport buffer, its bounds, any
 * indicator) stays there - this struct has no reason to know about it a
 * second time. Portal::Forma::create_text_field(..., scrollable=true) in
 * Forma.hpp is nothing more than a call to this.
 */
struct TextField {
    // =========================================================================
    // Results - populated by place()
    // =========================================================================

    /// @brief Element id of the text field. Valid after place().
    uint32_t element_id {};

    /// @brief Live editable text and cursor. Valid after place().
    std::shared_ptr<MappedState<Portal::Text::EditableText>> state;

    /// @brief Shared, mutable render params - every re-render reads this
    ///        fresh, so a caller can change it once and have every
    ///        subsequent keystroke see it. Valid after place().
    std::shared_ptr<Portal::Text::PressParams> params;

    /// @brief FormaBuffer backing the text quad. Valid after place(). Pass
    ///        directly as Scrollable::track()'s clip_buf for scissor
    ///        clipping - no separate scissor call needed.
    std::shared_ptr<Buffers::FormaBuffer> buf;

    /// @brief NDC region occupied by the field. Valid after place().
    Kinesis::AABB2D field_bounds {};

    /// @brief Field region, satisfying Kinesis::HasBounds.
    [[nodiscard]] Kinesis::AABB2D bounds() const noexcept { return field_bounds; }

    // =========================================================================
    // Placement
    // =========================================================================

    /**
     * @brief Register the text field element and wire text editing onto it.
     *
     * @param in_buf       Pre-created buffer for the text quad - must have
     *                     an additional_textures slot at index 0
     *                     (Element::with_text's requirement).
     * @param surface      Surface to register the element on.
     * @param bounds       NDC region: hit-test bounds and the text quad.
     * @param in_params    Shared render params. A default-constructed one
     *                     is allocated if null.
     * @param initial_text Starting text. Cursor starts at its end.
     * @return *this, with results populated.
     */
    MAYAFLUX_API TextField& place(
        std::shared_ptr<Buffers::FormaBuffer> in_buf,
        Surface& surface,
        Kinesis::AABB2D bounds,
        std::shared_ptr<Portal::Text::PressParams> in_params,
        std::string initial_text = {});

    /**
     * @brief Fluent creator: place this field inside an already-placed
     *        Scrollable, growing itself taller than its viewport and
     *        track()ing itself in, wired to scroll past a fixed clip.
     *
     * scroller is entirely the caller's: build and configure it first -
     * Scrollable::place(), and optionally indicator()/background_color()/
     * wheel_speed(), all fluent - then pass it here by reference. This
     * method reads scroller.bounds() for the viewport region, places this
     * field at a content region content_multiplier times that viewport's
     * height (top-aligned, same width), and calls scroller.track() with
     * this field's own buf as clip_buf and reposition() as the reflow
     * callback. The same three-call composition shown in the class doc,
     * done once here so callers building the common single-field case do
     * not have to repeat it - nothing about the viewport, its buffer, or
     * any indicator passes through this call, because Scrollable already
     * owns all of that.
     *
     * Also wires scroller.wheel_handler() onto this field's own element:
     * the field sits on top of the viewport and is added to the Layer
     * after it, so it hit-tests first, and Context::handle_scroll only
     * ever dispatches to the topmost element under the cursor - without
     * this, the wheel would do nothing while the cursor sits directly over
     * the field's own text instead of the viewport's empty margin.
     *
     * Also relate()s element_id under scroller.viewport_id, so
     * Portal::Forma::destroy(surface, element_id) tears down the
     * scroller's viewport too, even though scroller itself is never owned
     * or stored here - only its Layer-side element needs freeing this way.
     *
     * @param in_buf              Pre-created buffer for the field's text quad.
     * @param surface             Surface to register the field's element on -
     *                            must be the same Surface scroller was placed on.
     * @param scroller            Already-placed Scrollable to grow and track into.
     * @param in_params           Shared render params. A default-constructed
     *                            one is allocated if null.
     * @param initial_text        Starting text. Cursor starts at its end.
     * @param content_multiplier  Content height as a multiple of scroller's
     *                            viewport height. Default 3x: enough headroom
     *                            to type past the viewport before hitting the
     *                            limit, not so much that most content sits
     *                            unreachable below a near-empty field.
     * @return *this, with results populated.
     */
    MAYAFLUX_API TextField& scrollable(
        std::shared_ptr<Buffers::FormaBuffer> in_buf,
        Surface& surface,
        Scrollable& scroller,
        std::shared_ptr<Portal::Text::PressParams> in_params,
        std::string initial_text = {},
        float content_multiplier = 3.F);

    // =========================================================================
    // Post-placement
    // =========================================================================

    /**
     * @brief Move the field to a new NDC region, keeping content and the
     *        bound texture unchanged.
     *
     * The visual half of a scroll reflow - resubmits the text quad and the
     * caret quad together at the new region (see the class doc's note on
     * why both are always resubmitted together), nothing else. Wire
     * directly as a Scrollable::track() reposition callback (see the class
     * doc example); Scrollable's own reflow() already pushes the matching
     * bounds_hint to the Layer, so hit-testing stays correct without this
     * needing a Layer of its own.
     *
     * @param new_bounds Shifted NDC region.
     */
    MAYAFLUX_API void reposition(Kinesis::AABB2D new_bounds);

private:
    Element m_element;

    /// @brief Same value as field_bounds, behind a shared_ptr so wire()'s
    ///        already-captured Context closures see a reposition() that
    ///        happens after they were registered - the same reason params
    ///        is a shared_ptr instead of a captured-by-value copy.
    std::shared_ptr<Kinesis::AABB2D> m_live_bounds;

    void wire(Context& ctx);
};

} // namespace MayaFlux::Portal::Forma
