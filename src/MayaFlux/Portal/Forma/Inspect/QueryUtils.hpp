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
 * Clamps each dimension to at least 1 pixel to avoid degenerate textures.
 */
[[nodiscard]] std::pair<uint32_t, uint32_t> row_pixel_dims(
    const std::shared_ptr<Core::Window>& window,
    float x_min, float x_max, float row_h);

/**
 * @brief Construct one labeled value row, advance the cursor, and return it.
 *
 * The returned row's link reads @p spec.reader each tick, repress's
 * "label: value" into the text image, and binds the image to the row's
 * FormaBuffer.
 */
[[nodiscard]] ValueRow make_value_row(
    const ValueSpec& spec,
    Layer& layer,
    const std::shared_ptr<Core::Window>& window,
    LayoutCursor& cursor,
    float x_min, float x_max, float row_h,
    glm::vec3 bg = glm::vec3(0.15F));

/**
 * @brief Construct a collapsible header followed by N value rows under it.
 *
 * The header label may include its own dynamic data if the caller wants
 * it visible while collapsed; once expanded, all rows in @p values
 * become visible via the relation cascade.
 *
 * Each value row is a separate Element relate'd to the header. Visibility
 * tracks the header's open state.
 *
 * @param header_label   Static text composited into the header's label image.
 * @param values         Specs for each body row. Order is preserved top-to-bottom.
 * @param initially_open Default false so deep trees stay collapsed at construction.
 */
[[nodiscard]] ValueGroup make_value_group(
    std::string_view header_label,
    std::span<const ValueSpec> values,
    Layer& layer,
    Context& context,
    const std::shared_ptr<Core::Window>& window,
    LayoutCursor& cursor,
    float x_min, float x_max, float row_h,
    bool initially_open = false);

} // namespace MayaFlux::Portal::Forma
