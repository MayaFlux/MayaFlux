#pragma once

#include "MayaFlux/Buffers/Forma/FormaBuffer.hpp"
#include "MayaFlux/Kinesis/Spatial/Bounds.hpp"

namespace MayaFlux::Core {
class VKImage;
}
namespace MayaFlux::Buffers {
class TextureBuffer;
}
namespace MayaFlux::Portal::Text {
struct PressParams;
}

namespace MayaFlux::Portal::Forma {

/**
 * @struct Element
 * @brief A bounded, renderable region on a window surface.
 *
 * An Element pairs a spatial description with a GPU buffer whose output
 * occupies that region. The spatial description is deliberately open:
 *
 * - @c bounds_hint: optional AABB2D in NDC space, used as a fast-reject
 *   pre-filter before @c contains is evaluated. Not required.
 *
 * - @c contains: authoritative point-in-element test. If absent and
 *   @c bounds_hint is set, @c bounds_hint::contains is used directly.
 *   If both are absent the element never hits. For a rectangle, set only
 *   @c bounds_hint. For a circle, arbitrary polygon, or curve region, set
 *   @c contains (and optionally @c bounds_hint as the circumscribed rect
 *   for performance).
 *
 * @c buffer is optional. An Element with no buffer is a pure spatial
 * registration: it participates in hit testing and relation cascades but
 * contributes no rendered output. A voice node with a spatial position,
 * a named region driving a compute shader, an invisible trigger zone; all
 * are valid Elements with no buffer.
 *
 * Fluent setters return @c Element& for chained construction:
 * @code
 * auto el = Element{}
 *     .with_name("kick_voice")
 *     .with_bounds(region)
 *     .non_interactive();
 * @endcode
 *
 * Layer::add() assigns the stable id. All setters that affect spatial or
 * visibility state can also be applied post-registration via Layer::Slot.
 *
 * @note Elements do not participate in the graphics subsystem scheduler
 *       directly. The caller registers the underlying buffer with the
 *       BufferManager and attaches a RenderProcessor as usual. Element
 *       holds only the spatial and identity metadata that Layer and
 *       Context need.
 */
struct Element {
    /// @brief Stable id assigned by Layer::add. Never zero.
    uint32_t id { 0 };

    /// @brief Optional fast-reject bounds in NDC space.
    ///        When set, Layer evaluates this before @c contains.
    std::optional<Kinesis::AABB2D> bounds_hint;

    /// @brief Authoritative containment test in NDC space.
    ///        When absent, @c bounds_hint is used as the sole test.
    std::function<bool(glm::vec2)> contains;

    /// @brief Buffer whose rendered output occupies this region.
    ///        nullptr for non-rendering elements (pure hit-test regions).
    std::shared_ptr<Buffers::FormaBuffer> buffer;

    /// @brief Optional GPU texture to bind to the attached buffer. Used for
    std::shared_ptr<Core::VKImage> texture;

    /// @brief When false, hit testing skips this element.
    bool interactive { true };

    /// @brief When false, the element is excluded from Layer iteration.
    bool visible { true };

    /// @brief Optional human-readable label for Lila introspection and
    ///        debug logging. Not used by Layer or Context internally.
    std::string name;

    // =========================================================================
    // Spatial
    // =========================================================================

    /**
     * @brief Set the fast-reject AABB. For rectangular regions this is
     *        sufficient on its own — leave @c with_contains unset.
     */
    Element& with_bounds(Kinesis::AABB2D b)
    {
        bounds_hint = b;
        return *this;
    }

    /**
     * @brief Set an explicit containment predicate.
     *
     * Use for any non-rectangular region: circles, polygons, strokes,
     * Voronoi cells, audio waveform envelopes. Pair with @c with_bounds
     * for a circumscribed AABB fast-reject when the predicate is expensive.
     *
     * Kinesis::circular_bounds, polygon_bounds, stroke_bounds, union_bounds,
     * subtract_bounds, and intersect_bounds are all valid arguments.
     */
    Element& with_contains(std::function<bool(glm::vec2)> fn)
    {
        contains = std::move(fn);
        return *this;
    }

    /**
     * @brief Convenience: rectangular region specified as two NDC corners.
     *        Equivalent to with_bounds(AABB2D{min, max}).
     */
    Element& with_rect(glm::vec2 ndc_min, glm::vec2 ndc_max)
    {
        bounds_hint = Kinesis::AABB2D { .min = ndc_min, .max = ndc_max };
        return *this;
    }

    /**
     * @brief Set a circular containment predicate.
     *
     * @c bounds_hint is independent — set it separately if a cheap pre-filter
     * is useful. For a plain circle it is often the circumscribed rect via
     * AABB2D::from_ndc(center, glm::vec2(radius)), but that decision belongs
     * to the caller.
     */
    Element& with_circle(glm::vec2 center, float radius)
    {
        contains = Kinesis::circular_bounds(center, radius);
        return *this;
    }

    /**
     * @brief Set a polygon containment predicate.
     *
     * Uses the winding number algorithm, covering both convex and non-convex
     * shapes. Vertices are in NDC, ordered CW or CCW.
     *
     * @c bounds_hint is independent — set it separately if a cheap pre-filter
     * is useful for this region.
     */
    Element& with_polygon(std::span<const glm::vec2> verts)
    {
        contains = Kinesis::polygon_bounds(verts);
        return *this;
    }

    /**
     * @brief Set a stroke-based containment predicate.
     *
     * A point is inside if its perpendicular distance to any segment of
     * @p pts is within @p half_thickness NDC units. Covers paths, wires,
     * cable routes, waveform traces, and any open or closed curve.
     *
     * @c bounds_hint is independent — set it separately if a cheap pre-filter
     * is useful for this region.
     */
    Element& with_stroke(std::span<const glm::vec2> pts, float half_thickness)
    {
        contains = Kinesis::stroke_bounds(pts, half_thickness);
        return *this;
    }

    // =========================================================================
    // Buffer
    // =========================================================================

    /**
     * @brief Attach a FormaBuffer as the rendered output for this region.
     *        nullptr is valid for pure hit-test or pure-spatial elements.
     */
    Element& with_buffer(std::shared_ptr<Buffers::FormaBuffer> buf)
    {
        buffer = std::move(buf);
        return *this;
    }

    // =========================================================================
    // Buffer
    // =========================================================================

    /**
     * @brief Submit a UV quad covering @p region and bind @p image as
     *        "texSampler" on the attached FormaBuffer.
     *
     * @p buffer must already be set and created with an additional_textures slot at index 0.
     * Any source is valid: a loaded PNG, a render target, a TextureBuffer
     * or TextBuffer via get_texture().
     *
     * @param image  GPU-resident VKImage in shader-read layout.
     * @param region NDC quad extent. Defaults to fullscreen.
     */
    Element& with_texture(
        const std::shared_ptr<Core::VKImage>& image,
        Kinesis::AABB2D region = { .min = glm::vec2(-1.F), .max = glm::vec2(1.F) });

    /**
     * @brief Convenience overload extracting the GPU texture from a
     *        TextureBuffer (or TextBuffer, which inherits it).
     *
     * Equivalent to with_texture(buf->get_texture(), region).
     */
    Element& with_texture(
        const std::shared_ptr<Buffers::TextureBuffer>& buf,
        Kinesis::AABB2D region = { .min = glm::vec2(-1.F), .max = glm::vec2(1.F) });

    /**
     * @brief Press @p text into a new GPU texture and bind it to the
     *        attached FormaBuffer. The VKImage is retained internally
     *        for subsequent set_text() calls.
     *
     * @p buffer must already be set and created with an additional_textures slot at index 0.
     *
     * @param text   UTF-8 string to composite.
     * @param params PressParams (color, render_bounds, atlas, budget_h). Defaults apply when omitted.
     * @param region NDC quad extent. Defaults to fullscreen.
     */
    Element& with_text(
        std::string_view text,
        std::optional<Portal::Text::PressParams> params,
        Kinesis::AABB2D region = { .min = glm::vec2(-1.F), .max = glm::vec2(1.F) });

    /**
     * @brief Re-composite @p text into the retained GPU texture.
     *
     * No-op when texture is null (element was not constructed via with_text()).
     * Calls Portal::Text::repress(VKImage&, ...) which updates the image
     * in-place.
     * Rebinds the texture after repress in case repress reallocated the VKImage.
     *
     * @param text   New UTF-8 string.
     * @param params PressParams. Defaults apply when omitted.
     */
    void set_text(std::string_view text, std::optional<Portal::Text::PressParams> params);

    // =========================================================================
    // Flags
    // =========================================================================

    /**
     * @brief Exclude from hit testing. Use for passive outputs — particle
     *        emitters, waveform displays, camera regions — that have spatial
     *        identity but accept no pointer events.
     */
    Element& non_interactive()
    {
        interactive = false;
        return *this;
    }

    /**
     * @brief Start hidden. Layer::set_visible or a relation cascade will
     *        reveal it. Use for elements that are conditionally visible or
     *        controlled by a parent collapsible.
     */
    Element& hidden()
    {
        visible = false;
        return *this;
    }

    // =========================================================================
    // Identity
    // =========================================================================

    /**
     * @brief Set the human-readable name used in Lila introspection and
     *        debug output. Has no effect on Layer or Context behaviour.
     */
    Element& with_name(std::string n)
    {
        name = std::move(n);
        return *this;
    }
};

} // namespace MayaFlux::Portal::Forma
