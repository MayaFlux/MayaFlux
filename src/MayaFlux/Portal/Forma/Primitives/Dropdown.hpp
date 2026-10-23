#pragma once

#include "LayoutCursor.hpp"

namespace MayaFlux::Portal::Forma {

class Surface;

/**
 * @struct Dropdown
 * @brief A header showing the current choice that opens into a list of options.
 *
 * Configure with the setters, then place() once. After place(), selected,
 * open, header_id, header_bounds and cursor_out are populated. The list
 * overlays whatever lies below the header and does not advance the cursor.
 * The header reads "v" before the current choice while closed and "^" while
 * open. Choosing a row writes its index to selected, updates the header and
 * closes the list. Lists longer than max_visible() scroll with the mouse wheel.
 *
 * selected is registered with the Bridge, so Forma::bridge().at(selected)
 * reads the index like any other state. A Bridge inbound write changes the
 * index but not the header caption.
 *
 * @code
 * auto wave = Dropdown {}
 *     .options({ "sine", "saw", "square" })
 *     .on_select([](size_t index) { })
 *     .place(surface, cursor, 0.08F);
 * @endcode
 */
struct Dropdown {
    /// @brief Element id of the header strip. Valid after place().
    uint32_t header_id {};

    /// @brief Index of the chosen option. Valid after place().
    std::shared_ptr<MappedState<size_t>> selected;

    /// @brief Whether the list is showing. Valid after place().
    std::shared_ptr<MappedState<bool>> open;

    /// @brief Cursor positioned below the header. Valid after place().
    LayoutCursor cursor_out;

    /// @brief NDC region occupied by the header strip. Valid after place().
    Kinesis::AABB2D header_bounds {};

    /// @brief Header strip region, satisfying Kinesis::HasBounds.
    [[nodiscard]] Kinesis::AABB2D bounds() const noexcept { return header_bounds; }

    /// @brief Option captions, in display order.
    std::vector<std::string> m_labels;

    /// @brief Index chosen before any interaction. Default: 0.
    size_t m_initial {};

    /// @brief Rows shown at once before the list scrolls. Default: 8.
    uint32_t m_max_visible { 8 };

    /// @brief Called with the index each time a row is chosen.
    std::function<void(size_t)> m_on_select;

    Dropdown& options(std::vector<std::string> labels)
    {
        m_labels = std::move(labels);
        return *this;
    }

    Dropdown& initial(size_t index)
    {
        m_initial = index;
        return *this;
    }

    Dropdown& max_visible(uint32_t rows)
    {
        m_max_visible = std::max<uint32_t>(rows, 1);
        return *this;
    }

    Dropdown& on_select(std::function<void(size_t)> fn)
    {
        m_on_select = std::move(fn);
        return *this;
    }

    /**
     * @brief Register the header and its option rows and wire their input.
     *
     * Advances @p cursor by @p row_h. With no options, logs a warning and
     * leaves the struct unplaced.
     *
     * @param surface Surface to register on.
     * @param cursor  Layout cursor. Advanced by row_h on return.
     * @param x_min   Left edge in NDC.
     * @param x_max   Right edge in NDC.
     * @param row_h   Height of the header and of each option row, in NDC units.
     * @return *this, with results populated.
     */
    MAYAFLUX_API Dropdown& place(
        Surface& surface,
        LayoutCursor& cursor,
        float x_min, float x_max, float row_h);

    /**
     * @brief Register using the cursor's column extents.
     */
    MAYAFLUX_API Dropdown& place(
        Surface& surface,
        LayoutCursor& cursor,
        float row_h);
};

} // namespace MayaFlux::Portal::Forma
