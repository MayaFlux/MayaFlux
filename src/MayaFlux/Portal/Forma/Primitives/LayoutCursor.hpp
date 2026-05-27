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

    explicit LayoutCursor(float y_start)
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

    /**
     * @brief Advance the cursor downward by @p height and return the NDC
     *        AABB occupied by the primitive just placed.
     */
    Kinesis::AABB2D advance(float height)
    {
        const float top = m_state->value;
        const float bot = top - height;
        m_state->write(bot);
        return Kinesis::AABB2D { .min = { -1.F, bot }, .max = { 1.F, top } };
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
};

} // namespace MayaFlux::Portal::Forma
