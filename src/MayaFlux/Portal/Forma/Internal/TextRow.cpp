#include "TextRow.hpp"

#include "Atelier.hpp"

#include "MayaFlux/Portal/Forma/Surface.hpp"

namespace MayaFlux::Portal::Forma::internal {

glm::vec4 row_idle_color() { return { 0.18F, 0.18F, 0.20F, 1.F }; }

glm::vec4 row_hover_color() { return { 0.32F, 0.32F, 0.36F, 1.F }; }

std::string row_caption(std::string_view label)
{
    return "  " + std::string(label);
}

Text::PressParams row_press_params(glm::uvec2 dims, glm::vec4 background)
{
    return {
        .color = { 1.F, 1.F, 1.F, 1.F },
        .background = background,
        .render_bounds = dims,
        .budget_h = dims.y,
    };
}

std::pair<uint32_t, std::shared_ptr<Element>> add_text_row(
    Surface& surface,
    std::string_view label,
    Kinesis::AABB2D bounds,
    glm::uvec2 dims,
    const std::shared_ptr<Buffers::FormaBuffer>& batch)
{
    FormaTarget buf = batch
        ? FormaTarget::slice_of(batch, Graphics::PrimitiveTopology::TRIANGLE_LIST)
        : FormaTarget(atelier().create_buffer(
              surface.render_config({ .additional_textures = { { "text", nullptr } } })));

    auto row = std::make_shared<Element>();
    row->with_name(std::string(label))
        .with_bounds(bounds)
        .with_buffer(buf)
        .with_text(row_caption(label), row_press_params(dims, row_idle_color()), bounds);

    return { surface.add(*row), row };
}

void hover_text_row(
    Surface& surface,
    uint32_t id,
    const std::shared_ptr<Element>& row,
    std::string_view label,
    const std::shared_ptr<glm::uvec2>& dims)
{
    const std::string caption = row_caption(label);

    surface.ctx().on_enter(id, [row, caption, dims](uint32_t) {
        row->set_text(caption, row_press_params(*dims, row_hover_color()));
    });
    surface.ctx().on_leave(id, [row, caption, dims](uint32_t) {
        row->set_text(caption, row_press_params(*dims, row_idle_color()));
    });
}

} // namespace MayaFlux::Portal::Forma::internal
