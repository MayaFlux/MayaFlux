#pragma once

#include "MayaFlux/Portal/Forma/Element.hpp"
#include "MayaFlux/Portal/Text/InkPress.hpp"

namespace MayaFlux::Buffers {
class FormaBuffer;
}

namespace MayaFlux::Portal::Forma {

class Surface;

}

namespace MayaFlux::Portal::Forma::internal {

[[nodiscard]] glm::vec4 row_idle_color();

[[nodiscard]] glm::vec4 row_hover_color();

/**
 * @brief Caption drawn for a row labelled @p label.
 */
[[nodiscard]] std::string row_caption(std::string_view label);

/**
 * @brief White text over @p background, laid out at @p dims pixels.
 */
[[nodiscard]] Text::PressParams row_press_params(glm::uvec2 dims, glm::vec4 background);

/**
 * @brief Register a clickable text row on @p surface.
 *
 * Draws @p label over the idle background and adds the element to the
 * surface's layer. The row is a slice of @p batch when one is given, so rows
 * that share a buffer draw together; otherwise it gets a buffer of its own.
 *
 * @param surface Surface to register on.
 * @param label   Row text.
 * @param bounds  NDC region of the row.
 * @param dims    Pixel size to lay the text out at.
 * @param batch   Text-capable buffer to draw into as a slice, or empty.
 * @return Element id on the layer, and the row element for later set_text().
 */
[[nodiscard]] std::pair<uint32_t, std::shared_ptr<Element>> add_text_row(
    Surface& surface,
    std::string_view label,
    Kinesis::AABB2D bounds,
    glm::uvec2 dims,
    const std::shared_ptr<Buffers::FormaBuffer>& batch = {});

/**
 * @brief Highlight a row's background while the cursor is over it.
 *
 * @param dims Pixel size read on every enter and leave, so a resize that
 *             updates it is picked up.
 */
void hover_text_row(
    Surface& surface,
    uint32_t id,
    const std::shared_ptr<Element>& row,
    std::string_view label,
    const std::shared_ptr<glm::uvec2>& dims);

} // namespace MayaFlux::Portal::Forma::internal
