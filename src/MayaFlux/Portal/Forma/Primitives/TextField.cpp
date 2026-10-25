#include "TextField.hpp"

#include "MayaFlux/Portal/Forma/Internal/TextSlice.hpp"
#include "MayaFlux/Portal/Forma/Surface.hpp"

#include "MayaFlux/Portal/Text/Text.hpp"

#include "MayaFlux/Buffers/Forma/FormaBuffer.hpp"
#include "MayaFlux/Kinesis/GeometryPrimitives.hpp"

namespace MayaFlux::Portal::Forma {

namespace {

    /// @brief Caret width in pixels, same space as PressParams::render_bounds.
    constexpr float k_caret_width_px = 2.F;

    /**
     * @brief Redraw the caret for the current cursor, after the glyphs.
     *
     * The slice's run already holds the field's text, so its own caret rect
     * and map put the caret exactly where the glyphs were laid out. Caret
     * color reuses params.color (the text tint) rather than adding a new
     * config field. This is the one write that also shows any text change
     * made to the run just before it.
     */
    void render_caret(
        internal::TextSlice& slice,
        const Portal::Text::EditableText& edit,
        const Portal::Text::PressParams& params)
    {
        const Kinesis::AABB2D caret = slice.run().caret(edit.cursor, k_caret_width_px);
        const auto map = slice.map();
        const glm::vec2 a = map(caret.min);
        const glm::vec2 b = map(caret.max);

        const auto quad = Kinesis::textured_mesh_rect(
            { .min = glm::min(a, b), .max = glm::max(a, b) }, 0.F, params.color);
        slice.set_tail({ quad.begin(), quad.end() });
    }

    /// @brief Lay the edited text out in the slice's run. Nothing is written
    ///        until render_caret(), which follows every call.
    void render_text(
        internal::TextSlice& slice,
        const Portal::Text::EditableText& edit,
        const Portal::Text::PressParams& params)
    {
        slice.run().assign(edit.text, params);
    }

    /// @brief Resolve a cursor position on a line that may have no glyph quads.
    size_t index_on_line(Portal::Text::TextRun& run, glm::vec2 px, float target_top)
    {
        const size_t nearest = run.index_at(px);
        const auto* atlas = run.atlas();
        if (!atlas || atlas->line_height() == 0)
            return nearest;

        const float half_line = static_cast<float>(atlas->line_height()) * 0.5F;
        if (std::abs(run.caret(nearest, 0.F).min.y - target_top) < half_line)
            return nearest;

        const auto& text = run.text();
        size_t line_start = 0;
        while (true) {
            if (std::abs(run.caret(line_start, 0.F).min.y - target_top) < half_line)
                return line_start;
            const size_t newline = text.find('\n', line_start);
            if (newline == std::string::npos)
                break;
            line_start = newline + 1;
        }

        return nearest;
    }

    /**
     * @brief Move the cursor to the nearest codepoint boundary one line
     *        above (line_delta = -1) or below (+1) its current line, at
     *        the same x position it currently sits at.
     *
     * Uses the run's caret positions to reach lines with no glyph quads.
     */
    void move_vertical(
        Portal::Text::EditableText& edit,
        internal::TextSlice& slice,
        int line_delta)
    {
        auto& run = slice.run();
        const auto* atlas = run.atlas();
        if (!atlas)
            return;

        const auto caret = run.caret(edit.cursor, 0.F);
        const float line_height = static_cast<float>(atlas->line_height());
        const float target_top = caret.min.y + static_cast<float>(line_delta) * line_height;
        const float target_y = target_top + static_cast<float>(atlas->ascender());
        edit.cursor = index_on_line(run, { caret.min.x, target_y }, target_top);
    }

    /// @brief Inverse of the slice map's pixel-to-NDC mapping.
    glm::vec2 pixel_at(glm::vec2 ndc, Kinesis::AABB2D field_bounds, glm::vec2 extent)
    {
        return {
            (ndc.x - field_bounds.min.x) / field_bounds.width() * extent.x,
            (field_bounds.max.y - ndc.y) / field_bounds.height() * extent.y
        };
    }

    /// @brief Move the cursor to the codepoint boundary nearest an NDC
    ///        click position.
    void move_to_click(
        Portal::Text::EditableText& edit,
        internal::TextSlice& slice,
        Kinesis::AABB2D field_bounds,
        glm::vec2 ndc)
    {
        auto& run = slice.run();
        const glm::vec2 px = pixel_at(ndc, field_bounds, run.extent());
        const auto* atlas = run.atlas();
        if (!atlas || atlas->line_height() == 0) {
            edit.cursor = run.index_at(px);
            return;
        }

        const float line_height = static_cast<float>(atlas->line_height());
        const float target_top = std::floor(px.y / line_height) * line_height;
        edit.cursor = index_on_line(run, px, target_top);
    }

} // namespace

TextField& TextField::place(
    FormaTarget in_buf,
    Surface& surface,
    Kinesis::AABB2D bounds,
    std::shared_ptr<Portal::Text::PressParams> in_params,
    std::string initial_text)
{
    buf = std::move(in_buf);
    field_bounds = bounds;
    m_live_bounds = std::make_shared<Kinesis::AABB2D>(bounds);

    params = std::move(in_params);
    if (!params)
        params = std::make_shared<Portal::Text::PressParams>();

    m_element = Element {}
                    .with_name("text_field")
                    .with_bounds(bounds)
                    .with_buffer(buf)
                    .with_text(initial_text, *params, bounds);
    slice = m_element.text_slice;

    element_id = surface.layer().add(m_element);
    m_element.id = element_id;

    state = std::make_shared<MappedState<Portal::Text::EditableText>>();
    state->value.cursor = initial_text.size();
    state->value.text = std::move(initial_text);

    render_caret(*slice, state->value, *params);

    wire(surface.ctx());

    return *this;
}

void TextField::wire(Context& ctx)
{
    ctx.on_text(element_id, [sl = slice, s = state, p = params](uint32_t, uint32_t codepoint) mutable {
        Portal::Text::EditableText edit = s->value;
        Portal::Text::insert_codepoint(edit, codepoint);
        s->write(edit);
        render_text(*sl, edit, *p);
        render_caret(*sl, edit, *p);
    });

    ctx.press_and_hold(element_id, IO::Keys::Backspace, [sl = slice, s = state, p = params](uint32_t) mutable {
        Portal::Text::EditableText edit = s->value;
        Portal::Text::erase(edit, -1);
        s->write(edit);
        render_text(*sl, edit, *p);
        render_caret(*sl, edit, *p);
    });

    ctx.press_and_hold(element_id, IO::Keys::Delete, [sl = slice, s = state, p = params](uint32_t) mutable {
        Portal::Text::EditableText edit = s->value;
        Portal::Text::erase(edit, 1);
        s->write(edit);
        render_text(*sl, edit, *p);
        render_caret(*sl, edit, *p);
    });

    ctx.press_and_hold(element_id, IO::Keys::Tab, [sl = slice, s = state, p = params](uint32_t) mutable {
        Portal::Text::EditableText edit = s->value;
        Portal::Text::insert_literal(edit, '\t');
        s->write(edit);
        render_text(*sl, edit, *p);
        render_caret(*sl, edit, *p);
    });

    ctx.press_and_hold(element_id, IO::Keys::Enter, [sl = slice, s = state, p = params](uint32_t) mutable {
        Portal::Text::EditableText edit = s->value;
        Portal::Text::insert_literal(edit, '\n');
        s->write(edit);
        render_text(*sl, edit, *p);
        render_caret(*sl, edit, *p);
    });

    ctx.press_and_hold(element_id, IO::Keys::Left, [sl = slice, s = state, p = params](uint32_t) mutable {
        Portal::Text::EditableText edit = s->value;
        Portal::Text::move(edit, -1);
        s->write(edit);
        render_caret(*sl, edit, *p);
    });

    ctx.press_and_hold(element_id, IO::Keys::Right, [sl = slice, s = state, p = params](uint32_t) mutable {
        Portal::Text::EditableText edit = s->value;
        Portal::Text::move(edit, 1);
        s->write(edit);
        render_caret(*sl, edit, *p);
    });

    ctx.press_and_hold(element_id, IO::Keys::Up, [sl = slice, s = state, p = params](uint32_t) mutable {
        Portal::Text::EditableText edit = s->value;
        move_vertical(edit, *sl, -1);
        s->write(edit);
        render_caret(*sl, edit, *p);
    });

    ctx.press_and_hold(element_id, IO::Keys::Down, [sl = slice, s = state, p = params](uint32_t) mutable {
        Portal::Text::EditableText edit = s->value;
        move_vertical(edit, *sl, 1);
        s->write(edit);
        render_caret(*sl, edit, *p);
    });

    ctx.on_press(element_id, IO::MouseButtons::Left,
        [sl = slice, s = state, p = params, fb = m_live_bounds](uint32_t, glm::vec2 ndc) mutable {
            Portal::Text::EditableText edit = s->value;
            move_to_click(edit, *sl, *fb, ndc);
            s->write(edit);
            render_caret(*sl, edit, *p);
        });
}

TextField& TextField::scrollable(
    FormaTarget in_buf,
    Surface& surface,
    Scrollable& scroller,
    std::shared_ptr<Portal::Text::PressParams> in_params,
    std::string initial_text,
    float content_multiplier)
{
    const Kinesis::AABB2D viewport = scroller.bounds();
    const Kinesis::AABB2D content_bounds {
        .min = glm::vec2(viewport.min.x, viewport.max.y - viewport.height() * content_multiplier),
        .max = glm::vec2(viewport.max.x, viewport.max.y)
    };

    place(std::move(in_buf), surface, content_bounds, std::move(in_params), std::move(initial_text));

    TextField field = *this;
    scroller.track(
        surface.layer(), element_id, content_bounds,
        [field](Kinesis::AABB2D shifted) mutable { field.reposition(shifted); },
        buf);

    surface.layer().relate(element_id, scroller.viewport_id);
    surface.ctx().on_scroll(element_id, scroller.wheel_handler(surface));

    return *this;
}

void TextField::reposition(Kinesis::AABB2D new_bounds)
{
    field_bounds = new_bounds;
    if (m_live_bounds)
        *m_live_bounds = new_bounds;

    if (slice && state && params) {
        slice->set_bounds(new_bounds);
        render_caret(*slice, state->value, *params);
    }
}

} // namespace MayaFlux::Portal::Forma
