#include "ColorPicker.hpp"
#include "Geometry.hpp"

#include "MayaFlux/Core/Backends/Windowing/Window.hpp"
#include "MayaFlux/Kinesis/ColorSpace.hpp"
#include "MayaFlux/Kinesis/Spatial/Projection.hpp"
#include "MayaFlux/Portal/Forma/Internal/Atelier.hpp"
#include "MayaFlux/Portal/Forma/Surface.hpp"

namespace MayaFlux::Portal::Forma {

namespace {

    constexpr uint32_t k_square_cells = 12;
    constexpr uint32_t k_hue_segments = 6;
    constexpr float k_marker_half_height = 0.018F;

    void append_quad(
        std::vector<Kakshya::MeshVertex>& out,
        Kinesis::AABB2D region,
        glm::vec3 bl, glm::vec3 br, glm::vec3 tl, glm::vec3 tr)
    {
        using V = Kakshya::MeshVertex;
        const V vbl { .position = { region.min.x, region.min.y, 0.F }, .color = bl };
        const V vbr { .position = { region.max.x, region.min.y, 0.F }, .color = br };
        const V vtl { .position = { region.min.x, region.max.y, 0.F }, .color = tl };
        const V vtr { .position = { region.max.x, region.max.y, 0.F }, .color = tr };
        out.insert(out.end(), { vbl, vbr, vtl, vbr, vtr, vtl });
    }

    void append_quad(std::vector<Kakshya::MeshVertex>& out, Kinesis::AABB2D region, glm::vec3 color)
    {
        append_quad(out, region, color, color, color, color);
    }

    void append_marker(std::vector<Kakshya::MeshVertex>& out, glm::vec2 centre, glm::vec2 half)
    {
        append_quad(out, { .min = centre - half, .max = centre + half }, glm::vec3(0.F));
        append_quad(out, { .min = centre - half * 0.6F, .max = centre + half * 0.6F }, glm::vec3(1.F));
    }

}

ColorPicker& ColorPicker::place(
    Surface& surface,
    LayoutCursor& cursor,
    float x_min, float x_max, float height)
{
    auto& ctx = surface.ctx();
    const auto layer_ptr = surface.layer_ptr();

    const Kinesis::AABB2D advanced = cursor.advance(height);
    const float width = x_max - x_min;
    const float y_min = advanced.min.y;
    const float y_max = advanced.max.y;

    const Kinesis::AABB2D whole { .min = { x_min, y_min }, .max = { x_max, y_max } };
    const Kinesis::AABB2D square_rect { .min = { x_min, y_min }, .max = { x_min + width * 0.62F, y_max } };
    const Kinesis::AABB2D hue_rect { .min = { x_min + width * 0.66F, y_min }, .max = { x_min + width * 0.78F, y_max } };
    const Kinesis::AABB2D swatch_rect { .min = { x_min + width * 0.82F, y_min }, .max = { x_max, y_max } };

    GeometryFn<glm::vec3> geometry = [square_rect, hue_rect, swatch_rect, window = std::weak_ptr<Core::Window>(surface.window())](
                                         glm::vec3 value, std::vector<uint8_t>& out, Element&) {
        float aspect = 1.F;
        if (const auto locked = window.lock()) {
            const auto& state = locked->get_state();
            if (state.current_width > 0)
                aspect = static_cast<float>(state.current_height) / static_cast<float>(state.current_width);
        }
        const glm::vec2 marker { k_marker_half_height * aspect, k_marker_half_height };

        std::vector<Kakshya::MeshVertex> verts;
        verts.reserve(static_cast<size_t>(k_square_cells * k_square_cells + k_hue_segments + 6) * 6);

        append_quad(verts, swatch_rect, Kinesis::hsv_to_rgb(value));

        const float cells = static_cast<float>(k_square_cells);
        for (uint32_t i = 0; i < k_square_cells; ++i) {
            for (uint32_t j = 0; j < k_square_cells; ++j) {
                const float s0 = static_cast<float>(i) / cells;
                const float s1 = static_cast<float>(i + 1) / cells;
                const float v0 = static_cast<float>(j) / cells;
                const float v1 = static_cast<float>(j + 1) / cells;
                const Kinesis::AABB2D cell {
                    .min = { square_rect.min.x + s0 * square_rect.width(), square_rect.min.y + v0 * square_rect.height() },
                    .max = { square_rect.min.x + s1 * square_rect.width(), square_rect.min.y + v1 * square_rect.height() },
                };
                append_quad(verts, cell,
                    Kinesis::hsv_to_rgb({ value.x, s0, v0 }),
                    Kinesis::hsv_to_rgb({ value.x, s1, v0 }),
                    Kinesis::hsv_to_rgb({ value.x, s0, v1 }),
                    Kinesis::hsv_to_rgb({ value.x, s1, v1 }));
            }
        }

        const float segments = static_cast<float>(k_hue_segments);
        for (uint32_t k = 0; k < k_hue_segments; ++k) {
            const float h0 = static_cast<float>(k) / segments;
            const float h1 = static_cast<float>(k + 1) / segments;
            const Kinesis::AABB2D segment {
                .min = { hue_rect.min.x, hue_rect.min.y + h0 * hue_rect.height() },
                .max = { hue_rect.max.x, hue_rect.min.y + h1 * hue_rect.height() },
            };
            const glm::vec3 low = Kinesis::hsv_to_rgb({ h0, 1.F, 1.F });
            const glm::vec3 high = Kinesis::hsv_to_rgb({ h1, 1.F, 1.F });
            append_quad(verts, segment, low, low, high, high);
        }

        const float s = std::clamp(value.y, 0.F, 1.F);
        const float v = std::clamp(value.z, 0.F, 1.F);
        append_marker(verts,
            { square_rect.min.x + s * square_rect.width(), square_rect.min.y + v * square_rect.height() },
            marker);

        const float hue_y = hue_rect.min.y + std::clamp(value.x, 0.F, 1.F) * hue_rect.height();
        const glm::vec2 hue_half { hue_rect.width() * 0.5F + marker.x, marker.y * 0.5F };
        append_marker(verts, { hue_rect.min.x + hue_rect.width() * 0.5F, hue_y }, hue_half);

        Geometry::write_verts(out, verts);
    };

    const glm::vec3 initial_hsv = Kinesis::rgb_to_hsv(m_initial);
    auto drawn = internal::atelier().create_element<glm::vec3>(
        surface, std::move(geometry), initial_hsv, Graphics::PrimitiveTopology::TRIANGLE_LIST);

    auto color_state = std::make_shared<MappedState<glm::vec3>>();
    color_state->write(m_initial);

    const uint32_t square_id = surface.add(Element {}.with_name("color_square").with_bounds(square_rect));
    const uint32_t hue_id = surface.add(Element {}.with_name("color_hue").with_bounds(hue_rect));
    layer_ptr->relate(drawn.element.id, square_id);
    layer_ptr->relate(drawn.element.id, hue_id);

    auto commit = [hsv_state = drawn.state, color_state, callback = m_on_change](glm::vec3 next) {
        hsv_state->write(next);
        const glm::vec3 rgb = Kinesis::hsv_to_rgb(next);
        color_state->write(rgb);
        if (callback)
            callback(rgb);
    };

    const auto square_fraction = Kinesis::unit_square(square_rect);
    auto pick_square = [commit, hsv_state = drawn.state, square_fraction](glm::vec2 ndc) {
        const glm::vec2 fraction = square_fraction(ndc);
        commit({ hsv_state->value.x, fraction.x, fraction.y });
    };
    auto pick_hue = [commit, hsv_state = drawn.state, hue_rect](glm::vec2 ndc) {
        if (hue_rect.height() <= 0.F)
            return;
        const float hue = std::clamp((ndc.y - hue_rect.min.y) / hue_rect.height(), 0.F, 1.F);
        commit({ hue, hsv_state->value.y, hsv_state->value.z });
    };

    ctx.on_press(square_id, IO::MouseButtons::Left,
        [pick_square](uint32_t, glm::vec2 ndc) { pick_square(ndc); });
    ctx.wire_drag(square_id, pick_square, IO::MouseButtons::Left);
    ctx.on_press(hue_id, IO::MouseButtons::Left,
        [pick_hue](uint32_t, glm::vec2 ndc) { pick_hue(ndc); });
    ctx.wire_drag(hue_id, pick_hue, IO::MouseButtons::Left);

    internal::atelier().bridge().register_element<glm::vec3>(
        ElementRef { layer_ptr, square_id }, color_state, nullptr);

    picker_id = drawn.element.id;
    color = std::move(color_state);
    hsv = drawn.state;
    picker_bounds = whole;
    cursor_out = cursor;
    return *this;
}

ColorPicker& ColorPicker::place(
    Surface& surface,
    LayoutCursor& cursor,
    float height)
{
    return place(surface, cursor, cursor.x_min(), cursor.x_max(), height);
}

} // namespace MayaFlux::Portal::Forma
