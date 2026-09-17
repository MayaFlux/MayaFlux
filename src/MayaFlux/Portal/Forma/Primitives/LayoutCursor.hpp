#pragma once

#include "Mapped.hpp"

namespace MayaFlux::Portal::Forma {

/**
 * @brief Reactive Y-position accumulator for vertical primitive stacking.
 *
 * Holds a shared MappedState<float> carrying the current NDC Y baseline.
 * Any primitive whose GeometryFn closes over the state pointer will reflow
 * automatically when the cursor advances, via the normal version-bump path.
 *
 * NDC Y runs +1 (top) to -1 (bottom). advance() subtracts height.
 *
 * Default construction places the cursor at the top of the screen (y=1).
 * This matches the most common starting position and allows LayoutCursor
 * to be a plain member of structs that use aggregate or value initialization.
 */
class LayoutCursor {
public:
    LayoutCursor()
        : LayoutCursor(1.F)
    {
    }

    /**
     * @brief Construct a cursor at @p y_start scoped to a horizontal column.
     *
     * Bounds returned by advance() span [@p x_min, @p x_max] rather than the
     * full NDC width. The default extents reproduce the previous behaviour,
     * so existing call sites are unaffected.
     *
     * @param y_start Starting NDC Y baseline.
     * @param x_min   Left edge of the column in NDC.
     * @param x_max   Right edge of the column in NDC.
     */
    explicit LayoutCursor(float y_start, float x_min = -1.F, float x_max = 1.F)
        : m_x_min(x_min)
        , m_x_max(x_max)
    {
        m_state = std::make_shared<MappedState<float>>();
        m_state->write(y_start);
    }

    /**
     * @brief Shared baseline state. Close over this in GeometryFn to reflow
     *        when any preceding primitive changes height.
     */
    [[nodiscard]] std::shared_ptr<MappedState<float>> state() const { return m_state; }

    [[nodiscard]] float y() const { return m_state->value; }

    [[nodiscard]] float x_min() const noexcept { return m_x_min; }
    [[nodiscard]] float x_max() const noexcept { return m_x_max; }

    /**
     * @brief Bind a live scroll offset. advance() folds its current value
     *        into every bounds it returns from this point on.
     *
     * The offset itself is Kinesis::ScrollState's concern (content bounds,
     * clamping, wheel/drag input); this only consumes the resulting value.
     * Binding does not retroactively shift bounds already handed out by
     * earlier advance() calls - those were snapshots, same as advance()
     * has always returned. A geometry function that needs those existing
     * placements to reflow live as the user scrolls (not just new ones
     * placed after a scroll already happened) should close over
     * scroll_offset() itself and add it to its own captured bounds inside
     * the GeometryFn body, the same reflow-via-shared-state idiom every
     * other Mapped<T> primitive already uses for its own value.
     *
     * @param offset Shared scroll offset, written elsewhere (typically from
     *               Kinesis::apply_scroll/apply_wheel_scroll via an
     *               on_scroll callback).
     */
    void bind_scroll(std::shared_ptr<MappedState<glm::vec2>> offset)
    {
        m_scroll_offset = std::move(offset);
    }

    /**
     * @brief The bound scroll offset, or nullptr if none was bound.
     *
     * Exposed so a geometry function can close over the same live value
     * advance() itself reads, for reflowing bounds placed before scrolling
     * began.
     */
    [[nodiscard]] const std::shared_ptr<MappedState<glm::vec2>>& scroll_offset() const
    {
        return m_scroll_offset;
    }

    /**
     * @brief Reset both cursors to the lower of their two current Y values.
     *
     * Used after a row spanning several columns so that independent column
     * cursors resume from a common baseline.
     *
     * @note LayoutCursor copies share one MappedState. Calling sync_to on two
     *       cursors copied from one another is a no-op, since both already
     *       reference the same baseline.
     */
    void sync_to(LayoutCursor& other)
    {
        const float y = std::min(m_state->value, other.m_state->value);
        m_state->write(y);
        other.m_state->write(y);
    }

    /**
     * @brief Advance the cursor downward by @p height and return the NDC
     *        AABB occupied by the primitive just placed, scoped to the
     *        cursor's column extents.
     */
    Kinesis::AABB2D advance(float height)
    {
        const float top = m_state->value;
        const float bot = top - height;
        m_state->write(bot);

        Kinesis::AABB2D bounds { .min = { m_x_min, bot }, .max = { m_x_max, top } };
        if (m_scroll_offset)
            bounds = bounds.translated(m_scroll_offset->value);
        return bounds;
    }

    /**
     * @brief Advance without returning bounds. Use for padding between primitives.
     */
    void skip(float height) { m_state->write(m_state->value - height); }

    /**
     * @brief Reset to @p y_start. Existing closures over state() will reflow
     *        on their next sync() tick.
     */
    void reset(float y_start = 1.F) { m_state->write(y_start); }

private:
    std::shared_ptr<MappedState<float>> m_state;
    std::shared_ptr<MappedState<glm::vec2>> m_scroll_offset;
    float m_x_min { -1.F };
    float m_x_max { 1.F };
};

} // namespace MayaFlux::Portal::Forma
