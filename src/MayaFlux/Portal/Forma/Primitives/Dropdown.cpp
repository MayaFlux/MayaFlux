#include "Dropdown.hpp"

#include "Entry.hpp"
#include "Scrollable.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Portal/Forma/Internal/Atelier.hpp"
#include "MayaFlux/Portal/Forma/Internal/TextRow.hpp"
#include "MayaFlux/Portal/Forma/Surface.hpp"

namespace MayaFlux::Portal::Forma {

namespace {

    std::string header_label(std::string_view label, bool open)
    {
        return std::string(open ? "^ " : "v ") + std::string(label);
    }

}

Dropdown& Dropdown::place(
    Surface& surface,
    LayoutCursor& cursor,
    float x_min, float x_max, float row_h)
{
    if (m_labels.empty()) {
        MF_WARN(Journal::Component::Portal, Journal::Context::API,
            "Dropdown::place: no options");
        return *this;
    }

    auto& ctx = surface.ctx();
    const auto layer_ptr = surface.layer_ptr();

    const Kinesis::AABB2D advanced = cursor.advance(row_h);
    const Kinesis::AABB2D header_rect {
        .min = { x_min, advanced.min.y },
        .max = { x_max, advanced.max.y },
    };

    const auto labels = std::make_shared<std::vector<std::string>>(m_labels);
    const size_t initial = std::min(m_initial, labels->size() - 1);
    const auto dims = std::make_shared<glm::uvec2>(
        row_pixel_dims(surface.window(), x_min, x_max, row_h));

    auto header = internal::add_text_row(surface, header_label(labels->at(initial), false), header_rect, *dims);
    const uint32_t hid = header.first;
    const auto& header_row = header.second;

    auto selected_state = std::make_shared<MappedState<size_t>>();
    selected_state->write(initial);
    auto open_state = std::make_shared<MappedState<bool>>();
    open_state->write(false);

    auto refresh_header = [header_row, labels, selected_state, open_state, dims]() {
        header_row->set_text(
            internal::row_caption(header_label(labels->at(selected_state->value), open_state->value)),
            internal::row_press_params(*dims, internal::row_idle_color()));
    };

    auto set_open = [layer_ptr, hid, open_state, refresh_header](bool next) {
        open_state->write(next);
        for (const uint32_t rel_id : layer_ptr->related_ids(hid))
            layer_ptr->set_visible(rel_id, next);
        if (next)
            layer_ptr->bring_to_front(hid);
        refresh_header();
    };

    auto choose = [selected_state, set_open, callback = m_on_select](size_t index) {
        selected_state->write(index);
        set_open(false);
        if (callback)
            callback(index);
    };

    const auto count = static_cast<uint32_t>(labels->size());
    const uint32_t visible = std::min(count, m_max_visible);
    const float list_top = header_rect.min.y;

    const auto text_batch = internal::atelier().create_buffer(
        surface.render_config({ .additional_textures = { { "text", nullptr } } }));

    std::optional<Scrollable> panel;
    std::function<void(uint32_t, glm::vec2, double, double)> wheel;
    if (count > visible) {
        auto panel_buf = FormaTarget::slice_of(text_batch, Graphics::PrimitiveTopology::TRIANGLE_STRIP);
        panel = make_scrollable(
            std::move(panel_buf), surface,
            { .min = { x_min, list_top - row_h * static_cast<float>(visible) }, .max = { x_max, list_top } },
            glm::vec3(internal::row_idle_color()));
        wheel = panel->wheel_handler(surface);
        layer_ptr->relate(hid, panel->viewport_id);
    }

    const auto rows = std::make_shared<std::vector<std::shared_ptr<Element>>>();

    for (uint32_t i = 0; i < count; ++i) {
        const float top = list_top - row_h * static_cast<float>(i);
        const Kinesis::AABB2D row_rect { .min = { x_min, top - row_h }, .max = { x_max, top } };

        auto made = internal::add_text_row(surface, labels->at(i), row_rect, *dims, text_batch);
        const uint32_t id = made.first;
        const auto& row = made.second;
        rows->push_back(row);

        internal::hover_text_row(surface, id, row, labels->at(i), dims);
        ctx.on_press(id, IO::MouseButtons::Left, [choose, i](uint32_t, glm::vec2) { choose(i); });

        if (panel) {
            ctx.on_scroll(id, wheel);
            panel->track(
                *layer_ptr, id, row_rect,
                [row](Kinesis::AABB2D shifted) { row->retarget(shifted); },
                row->buffer);
        } else {
            layer_ptr->relate(hid, id);
        }
    }

    ctx.on_press(hid, IO::MouseButtons::Left,
        [open_state, set_open](uint32_t, glm::vec2) { set_open(!open_state->value); });

    ctx.on_resize(hid,
        [window = surface.window(), x_min, x_max, row_h, dims, labels, rows, refresh_header](uint32_t, uint32_t) {
            *dims = row_pixel_dims(window, x_min, x_max, row_h);
            refresh_header();
            const auto idle = internal::row_press_params(*dims, internal::row_idle_color());
            for (size_t i = 0; i < rows->size(); ++i)
                rows->at(i)->set_text(internal::row_caption(labels->at(i)), idle);
        });

    set_open(false);

    internal::atelier().bridge().register_element<size_t>(
        ElementRef { layer_ptr, hid }, selected_state, header_row->buffer.buffer());

    header_id = hid;
    selected = std::move(selected_state);
    open = std::move(open_state);
    header_bounds = header_rect;
    cursor_out = cursor;
    return *this;
}

Dropdown& Dropdown::place(
    Surface& surface,
    LayoutCursor& cursor,
    float row_h)
{
    return place(surface, cursor, cursor.x_min(), cursor.x_max(), row_h);
}

} // namespace MayaFlux::Portal::Forma
