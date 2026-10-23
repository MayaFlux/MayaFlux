#pragma once

#include "MayaFlux/Portal/Forma/Element.hpp"
#include "MayaFlux/Portal/Text/InkPress.hpp"

namespace MayaFlux::Portal::Forma {

class Surface;

}

namespace MayaFlux::Portal::Forma::internal {

[[nodiscard]] glm::vec4 row_idle_color();

[[nodiscard]] glm::vec4 row_hover_color();

/**
 * @brief Caption pressed for a row labelled @p label.
 */
[[nodiscard]] std::string row_caption(std::string_view label);

/**
 * @brief White text over @p background, sized to @p dims pixels.
 */
[[nodiscard]] Text::PressParams row_press_params(glm::uvec2 dims, glm::vec4 background);

/**
 * @brief Register a clickable text row on @p surface.
 *
 * Creates the row's buffer, presses @p label over the idle background and
 * adds the element to the surface's layer.
 *
 * @param surface Surface to register on.
 * @param label   Row text.
 * @param bounds  NDC region of the row.
 * @param dims    Pixel size to press the text at.
 * @return Element id on the layer, and the row element for later set_text().
 */
[[nodiscard]] std::pair<uint32_t, std::shared_ptr<Element>> add_text_row(
    Surface& surface,
    std::string_view label,
    Kinesis::AABB2D bounds,
    glm::uvec2 dims);

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
