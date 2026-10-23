#pragma once

#include "LayoutCursor.hpp"

namespace MayaFlux::Portal::Forma {

class Surface;

/**
 * @struct ColorPicker
 * @brief A saturation and value square, a hue strip and a swatch, drawn into one buffer.
 *
 * Configure with the setters, then place() once. After place(), picker_id,
 * color, hsv, picker_bounds and cursor_out are populated. Pressing or dragging
 * the square sets saturation and value, and the strip sets hue. Hue is kept
 * in hsv, so it survives picking a grey or black. color holds the same value
 * as red, green and blue, each in [0, 1].
 *
 * color is registered with the Bridge, so Forma::bridge().at(color).write(sink)
 * receives three floats; hsv is registered the same way and receives hue,
 * saturation and value. A Bridge inbound write changes the value but does not
 * move the markers. Forma::destroy(surface, picker_id) removes the picker.
 *
 * @code
 * auto picker = ColorPicker {}
 *     .initial({ 0.9F, 0.4F, 0.1F })
 *     .on_change([](glm::vec3 rgb) { })
 *     .place(surface, cursor, 0.4F);
 * @endcode
 */
struct ColorPicker {
    /// @brief Element id of the drawn picker. Valid after place().
    uint32_t picker_id {};

    /// @brief Chosen colour as red, green and blue. Valid after place().
    std::shared_ptr<MappedState<glm::vec3>> color;

    /// @brief Chosen colour as hue, saturation and value. Valid after place().
    std::shared_ptr<MappedState<glm::vec3>> hsv;

    /// @brief Cursor positioned below the picker. Valid after place().
    LayoutCursor cursor_out;

    /// @brief NDC region occupied by the picker. Valid after place().
    Kinesis::AABB2D picker_bounds {};

    /// @brief Picker region, satisfying Kinesis::HasBounds.
    [[nodiscard]] Kinesis::AABB2D bounds() const noexcept { return picker_bounds; }

    /// @brief Colour shown before any interaction, as red, green and blue. Default: red.
    glm::vec3 m_initial { 1.F, 0.F, 0.F };

    /// @brief Called with the new red, green and blue on every pick.
    std::function<void(glm::vec3)> m_on_change;

    ColorPicker& initial(glm::vec3 rgb)
    {
        m_initial = rgb;
        return *this;
    }

    ColorPicker& on_change(std::function<void(glm::vec3)> fn)
    {
        m_on_change = std::move(fn);
        return *this;
    }

    /**
     * @brief Register the picker and wire its input.
     *
     * Advances @p cursor by @p height.
     *
     * @param surface Surface to register on.
     * @param cursor  Layout cursor. Advanced by height on return.
     * @param x_min   Left edge in NDC.
     * @param x_max   Right edge in NDC.
     * @param height  Picker height in NDC units.
     * @return *this, with results populated.
     */
    MAYAFLUX_API ColorPicker& place(
        Surface& surface,
        LayoutCursor& cursor,
        float x_min, float x_max, float height);

    /**
     * @brief Register using the cursor's column extents.
     */
    MAYAFLUX_API ColorPicker& place(
        Surface& surface,
        LayoutCursor& cursor,
        float height);
};

} // namespace MayaFlux::Portal::Forma
