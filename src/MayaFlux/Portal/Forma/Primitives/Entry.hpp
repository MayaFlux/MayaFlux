#pragma once

#include "Collapsible.hpp"
#include "MayaFlux/Portal/Forma/Link.hpp"

namespace MayaFlux::Core {
class Window;
}

namespace MayaFlux::Buffers {
class FormaBuffer;
}

namespace MayaFlux::Portal::Forma {

/**
 * @brief A pre-created text-capable buffer, passed to make_entry.
 */
struct EntryBuffer {
    FormaTarget buf;
};

/**
 * @brief A single entry to display in a EntryGroup body row.
 *
 * @c reader is invoked once per graphics tick to produce the current
 * stringified entry. The row text becomes "label: <reader()>".
 */
struct EntrySpec {
    std::string label;
    std::function<std::string()> reader;
};

/**
 * @brief One entry row in a EntryGroup body.
 *
 * Owns the target the row draws into and the Link that redraws the text
 * each tick.
 */
struct Entry {
    uint32_t element_id {};
    FormaTarget buf;
    Link link;

    /// @brief NDC region occupied by this row. Valid after make_entry.
    Kinesis::AABB2D row_bounds {};

    /// @brief Row region, satisfying Kinesis::HasBounds.
    [[nodiscard]] Kinesis::AABB2D bounds() const noexcept { return row_bounds; }
};

/**
 * @brief Header collapsible with N entry rows related to it.
 *
 * Rows are added as elements to the layer and related to the header,
 * so the existing visibility cascade hides them on collapse.
 */
struct EntryGroup {
    Collapsible header;
    std::vector<Entry> rows;

    /**
     * @brief Union of the header strip and every row region.
     *
     * Collapses to the header region alone when the group has no rows.
     */
    [[nodiscard]] Kinesis::AABB2D bounds() const noexcept
    {
        Kinesis::AABB2D b = header.bounds();
        for (const auto& r : rows) {
            const auto rb = r.bounds();
            b.min = glm::min(b.min, rb.min);
            b.max = glm::max(b.max, rb.max);
        }
        return b;
    }
};

/**
 * @brief Convert an NDC row rect into integer pixel dimensions.
 *
 * Clamps width to at least 1 pixel and height to at least the default
 * atlas pixel size to avoid degenerate layouts.
 *
 * @param window  Target window for pixel dimension calculation.
 * @param x_min   Left edge in NDC.
 * @param x_max   Right edge in NDC.
 * @param row_h   Row height in NDC units.
 */
[[nodiscard]] MAYAFLUX_API glm::uvec2 row_pixel_dims(
    const std::shared_ptr<Core::Window>& window,
    float x_min, float x_max, float row_h);

/**
 * @brief Construct one labeled entry row, advance the cursor, and return it.
 *
 * The row is one slice of glyph quads: @p bg is drawn as the fill beneath
 * the text (Portal::Text::PressParams::background), so one slice carries
 * both. The link's compose function runs immediately (establish) so the row
 * shows correctly from the first frame, and again on every subsequent
 * link.tap(), drawing "label: entry" into the row's buffer. The buffer is
 * pre-created by the caller as @p row_buf.
 *
 * Bounds come from cursor.advance()'s return (scroll-offset aware, per
 * LayoutCursor::bind_scroll), with x_min/x_max substituted in afterward for
 * this call's own column override - not from cursor.y() read directly, so
 * a row placed through a scroll-bound cursor lands at the correct position
 * immediately.
 *
 * @param spec     Label and reader for the row.
 * @param row_buf  Pre-created text-capable buffer.
 * @param surface   Surface to register the row on.
 * @param cursor   Layout cursor. Advanced by @p row_h on return.
 * @param x_min    Left edge in NDC.
 * @param x_max    Right edge in NDC.
 * @param row_h    Row height in NDC units.
 * @param bg       Background fill drawn beneath the row's text.
 */
[[nodiscard]] MAYAFLUX_API Entry make_entry(
    const EntrySpec& spec,
    EntryBuffer row_buf,
    Surface& surface,
    LayoutCursor& cursor,
    float x_min, float x_max, float row_h,
    glm::vec3 bg = glm::vec3(0.15F));

/**
 * @brief make_entry using the cursor's column extents.
 *
 * Equivalent to the explicit-extent overload with x_min and x_max taken
 * from @p cursor.
 */
[[nodiscard]] MAYAFLUX_API Entry make_entry(
    const EntrySpec& spec,
    EntryBuffer row_buf,
    Surface& surface,
    LayoutCursor& cursor,
    float row_h,
    glm::vec3 bg = glm::vec3(0.15F));

/**
 * @brief Construct a collapsible header followed by N entry rows under it.
 *
 * The header draws @p header_label itself (Collapsible::label),
 * background included, so header_buf needs only a "text" additional_textures
 * slot. Once expanded, all
 * rows in @p entrys become visible via the relation cascade. Visibility
 * tracks the header's open state.
 *
 * @param entrys         Specs for each body row. Order is preserved top-to-bottom.
 * @param header_label   Text drawn into the header strip.
 * @param header_buf     Pre-created buffer for the header strip, "text" slot empty.
 * @param row_bufs       Pre-created text-capable buffers, one per entry in @p entrys.
 * @param surface        Surface to register the header and rows on.
 * @param cursor         Layout cursor. Advanced across header and all rows on return.
 * @param x_min          Left edge in NDC.
 * @param x_max          Right edge in NDC.
 * @param row_h          Row height in NDC units.
 * @param initially_open Default false so deep trees stay collapsed at construction.
 */
[[nodiscard]] MAYAFLUX_API EntryGroup make_entry_group(
    std::span<const EntrySpec> entrys,
    std::string_view header_label,
    FormaTarget header_buf,
    std::span<const EntryBuffer> row_bufs,
    Surface& surface,
    LayoutCursor& cursor,
    float x_min, float x_max, float row_h,
    bool initially_open = false);

/**
 * @brief make_entry_group using the cursor's column extents.
 *
 * Equivalent to the explicit-extent overload with x_min and x_max taken
 * from @p cursor.
 */
[[nodiscard]] MAYAFLUX_API EntryGroup make_entry_group(
    std::span<const EntrySpec> entrys,
    std::string_view header_label,
    FormaTarget header_buf,
    std::span<const EntryBuffer> row_bufs,
    Surface& surface,
    LayoutCursor& cursor,
    float row_h,
    bool initially_open = false);

} // namespace MayaFlux::Portal::Forma
