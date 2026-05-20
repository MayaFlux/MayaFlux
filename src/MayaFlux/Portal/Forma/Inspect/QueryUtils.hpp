#pragma once

#include "MayaFlux/Portal/Forma/Context.hpp"
#include "MayaFlux/Portal/Forma/Layer.hpp"
#include "MayaFlux/Portal/Forma/Link.hpp"
#include "MayaFlux/Portal/Forma/Primitives/Collapsible.hpp"
#include "MayaFlux/Portal/Forma/Primitives/LayoutCursor.hpp"

namespace MayaFlux::Core {
class Window;
class VKImage;
}

namespace MayaFlux::Buffers {
class FormaBuffer;
}

namespace MayaFlux::Portal::Forma {

/**
 * @brief A pre-created buffer and its bound text image, passed to make_value_row.
 */
struct RowBuffer {
    std::shared_ptr<Buffers::FormaBuffer> buf;
    std::shared_ptr<Core::VKImage> text_image;
};

constexpr float k_inspect_indent = 0.03F;

/**
 * @brief A single value to display in a ValueGroup body row.
 *
 * @c reader is invoked once per graphics tick to produce the current
 * stringified value. The row text becomes "label: <reader()>".
 */
struct ValueSpec {
    std::string label;
    std::function<std::string()> reader;
};

/**
 * @brief One value row in a ValueGroup body.
 *
 * Owns the FormaBuffer for the row background, the VKImage holding the
 * composited text, and the Link that repress's the text each tick.
 */
struct ValueRow {
    uint32_t element_id {};
    std::shared_ptr<Buffers::FormaBuffer> buf;
    std::shared_ptr<Core::VKImage> text;
    Link link;
};

/**
 * @brief Header collapsible with N value rows related to it.
 *
 * Rows are added as elements to the layer and related to the header,
 * so the existing visibility cascade hides them on collapse.
 */
struct ValueGroup {
    Collapsible header;
    std::vector<ValueRow> rows;
};

/**
 * @brief Convert an NDC row rect into integer pixel dimensions.
 *
 * Clamps width to at least 1 pixel and height to at least the default
 * atlas pixel size to avoid degenerate textures.
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
 * @brief Construct one labeled value row, advance the cursor, and return it.
 *
 * The returned row's link reads @p spec.reader each tick, represses
 * "label: value" into the text image, and binds the image to the row's
 * FormaBuffer. The buffer and text image are pre-created by the caller
 * and travel together as @p row_buf.
 *
 * @param spec     Label and reader for the row.
 * @param row_buf  Pre-created buffer and bound text image.
 * @param layer    Layer to register the element on.
 * @param cursor   Layout cursor. Advanced by @p row_h on return.
 * @param x_min    Left edge in NDC.
 * @param x_max    Right edge in NDC.
 * @param row_h    Row height in NDC units.
 * @param bg       Background color for the row quad.
 */
[[nodiscard]] ValueRow make_value_row(
    const ValueSpec& spec,
    RowBuffer row_buf,
    Layer& layer,
    LayoutCursor& cursor,
    float x_min, float x_max, float row_h,
    glm::vec3 bg = glm::vec3(0.15F));

/**
 * @brief Construct a collapsible header followed by N value rows under it.
 *
 * The header and all row buffers are pre-created by the caller. Once
 * expanded, all rows in @p values become visible via the relation cascade.
 * Visibility tracks the header's open state.
 *
 * @param values         Specs for each body row. Order is preserved top-to-bottom.
 * @param header_buf     Pre-created buffer and text image for the header strip.
 * @param row_bufs       Pre-created buffers and text images, one per entry in @p values.
 * @param layer          Layer to register all elements on.
 * @param context        Context to wire the header toggle handler.
 * @param cursor         Layout cursor. Advanced across header and all rows on return.
 * @param x_min          Left edge in NDC.
 * @param x_max          Right edge in NDC.
 * @param row_h          Row height in NDC units.
 * @param initially_open Default false so deep trees stay collapsed at construction.
 */
[[nodiscard]] ValueGroup make_value_group(
    std::span<const ValueSpec> values,
    RowBuffer header_buf,
    std::span<const RowBuffer> row_bufs,
    Layer& layer,
    Context& context,
    LayoutCursor& cursor,
    float x_min, float x_max, float row_h,
    bool initially_open = false);

} // namespace MayaFlux::Portal::Forma
