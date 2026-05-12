#pragma once

#include "LayoutCursor.hpp"
#include "MayaFlux/Kakshya/NDData/VertexLayout.hpp"
#include "MayaFlux/Portal/Forma/Bridge.hpp"
#include "MayaFlux/Portal/Forma/Context.hpp"
#include "MayaFlux/Portal/Forma/Forma.hpp"
#include "MayaFlux/Portal/Forma/Layer.hpp"

namespace MayaFlux::Portal::Forma {

/**
 * @brief Result of make_collapsible().
 *
 * A named primitive: a header quad whose open/closed MappedState drives
 * both geometry color and Layer visibility of all related elements.
 */
struct Collapsible {
    uint32_t header_id {};
    std::shared_ptr<MappedState<bool>> open;
    LayoutCursor cursor_out;
};

/**
 * @brief Geometry function for a collapsible header strip.
 *
 * Writes four vertices at Kakshya::VertexLayout::for_meshes() stride (60 bytes).
 * position at offset 0 (vec3, 12 bytes), color at offset 12 (vec3, 12 bytes),
 * remaining 36 bytes zeroed. Closes over @p cursor_in for reflow.
 */
[[nodiscard]] inline GeometryFn<bool> collapsible_header_geom(
    std::shared_ptr<MappedState<float>> cursor_in,
    float x_min,
    float x_max,
    float row_h,
    glm::vec3 color_closed = glm::vec3(0.25F),
    glm::vec3 color_open = glm::vec3(0.35F))
{
    return [cursor_in, x_min, x_max, row_h, color_closed, color_open](
               bool open, std::vector<uint8_t>& out, Element& el) {
        const float top = cursor_in->value;
        const float bot = top - row_h;
        const glm::vec3 col = open ? color_open : color_closed;

        const auto layout = Kakshya::VertexLayout::for_meshes();
        const size_t stride = layout.stride_bytes;

        const std::array<glm::vec3, 4> positions = {
            glm::vec3 { x_min, bot, 0.F },
            glm::vec3 { x_max, bot, 0.F },
            glm::vec3 { x_min, top, 0.F },
            glm::vec3 { x_max, top, 0.F },
        };

        out.assign(4 * stride, 0);
        for (size_t i = 0; i < 4; ++i) {
            auto* v = out.data() + i * stride;
            std::memcpy(v, &positions[i], 12);
            std::memcpy(v + 12, &col, 12);
        }

        el.bounds_hint = Kinesis::AABB2D {
            .min = { x_min, bot },
            .max = { x_max, top },
        };
    };
}

/**
 * @brief Construct a Collapsible and register its header element in @p layer.
 *
 * Advances @p cursor by @p row_h. cursor_out sits immediately below the header.
 *
 * Caller responsibilities:
 *   - layer.relate(result.header_id, content_id) for each content element
 *   - context.on_press to write result.open and call layer.set_visible
 *
 * @param layer          Layer to register the header element on.
 * @param window         Target window.
 * @param bridge         Bridge owning the sync loop.
 * @param cursor         Layout cursor. Advanced by row_h internally.
 * @param x_min          Left edge in NDC.
 * @param x_max          Right edge in NDC.
 * @param row_h          Header strip height in NDC units.
 * @param initially_open Starting toggle state.
 * @param color_closed   Header color when closed.
 * @param color_open     Header color when open.
 */
[[nodiscard]] inline Collapsible make_collapsible(
    Layer& layer,
    const std::shared_ptr<Core::Window>& window,
    Bridge& bridge,
    LayoutCursor& cursor,
    float x_min,
    float x_max,
    float row_h,
    bool initially_open = true,
    glm::vec3 color_closed = glm::vec3(0.25F),
    glm::vec3 color_open = glm::vec3(0.35F))
{
    auto cursor_state = cursor.state();
    cursor.advance(row_h);

    auto open_state = std::make_shared<MappedState<bool>>();
    open_state->write(initially_open);

    const size_t buf_capacity = 4 * Kakshya::VertexLayout::for_meshes().stride_bytes;

    auto buf = create_buffer(window, buf_capacity,
        Graphics::PrimitiveTopology::TRIANGLE_STRIP);

    Mapped<bool> mapped;
    mapped.state = open_state;
    mapped.geometry_fn = collapsible_header_geom(
        cursor_state, x_min, x_max, row_h, color_closed, color_open);
    mapped.element.buffer = buf;
    mapped.element.bounds_hint = Kinesis::AABB2D {
        .min = { x_min, cursor.y() },
        .max = { x_max, cursor.y() + row_h },
    };

    const uint32_t hid = layer.add(mapped.element);
    open_state->id = hid;
    bridge.register_element(std::move(mapped));

    return Collapsible {
        .header_id = hid,
        .open = open_state,
        .cursor_out = LayoutCursor(cursor.y()),
    };
}

} // namespace MayaFlux::Portal::Forma
