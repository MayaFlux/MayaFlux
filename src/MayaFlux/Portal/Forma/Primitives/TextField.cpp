#include "TextField.hpp"

#include "MayaFlux/Portal/Forma/Internal/TextSlice.hpp"
#include "MayaFlux/Portal/Forma/Surface.hpp"

#include "MayaFlux/Portal/Text/Text.hpp"

#include "MayaFlux/Buffers/Forma/FormaBuffer.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kinesis/GeometryPrimitives.hpp"

namespace MayaFlux::Portal::Forma {

namespace {

    /// @brief Caret width in pixels, same space as PressParams::render_bounds.
    constexpr float k_caret_width_px = 2.F;

    /// @brief Selection fill: the text tint at this fraction of its alpha.
    constexpr float k_selection_alpha = 0.35F;

    /// @brief Highlight width given to a selected line break, in pixels.
    constexpr float k_newline_highlight_px = 4.F;

    /// @brief Modifier of the clipboard and select-all shortcuts: Command on macOS, Control elsewhere.
#ifdef MAYAFLUX_PLATFORM_MACOS
    constexpr IO::Modifiers k_shortcut_modifier = IO::Modifiers::Super;
#else
    constexpr IO::Modifiers k_shortcut_modifier = IO::Modifiers::Ctrl;
#endif

    /// @brief Whether Shift is held on the window the context listens to.
    bool shift_held(const Context& ctx)
    {
        return IO::has_flag(ctx.event_source().modifiers(), IO::Modifiers::Shift);
    }

    /**
     * @brief Pixel rectangles covering the bytes [begin, end) of the run's text, one per line.
     *
     * Walks the codepoint boundaries of the range and finds each line break by
     * binary search on the caret's y, which never decreases along the text, so
     * the cost is a few caret lookups per line rather than one per character.
     * A hard break is covered by k_newline_highlight_px. A wrapped line ends
     * where the pen was before the character that wrapped, which is drawn at
     * the start of the next line.
     */
    std::vector<Kinesis::AABB2D> selection_rects(
        Portal::Text::TextRun& run,
        const std::string& text,
        size_t begin,
        size_t end)
    {
        std::vector<size_t> bounds { begin };
        while (bounds.back() < end) {
            const size_t next = Portal::Text::next_codepoint_offset(text, bounds.back());
            bounds.push_back(std::min(next, end));
        }
        if (bounds.size() < 2)
            return {};

        const float line_h = run.caret(begin, 0.F).height();
        std::vector<std::optional<glm::vec2>> cached(bounds.size());
        const auto at = [&](size_t k) {
            if (!cached[k])
                cached[k] = run.caret(bounds[k], 0.F).min;
            return *cached[k];
        };

        std::vector<Kinesis::AABB2D> rects;
        const auto emit = [&](float x0, float x1, float y) {
            if (x1 > x0)
                rects.push_back({ .min = { x0, y }, .max = { x1, y + line_h } });
        };

        const size_t last = bounds.size() - 1;
        size_t start = 0;
        float left = at(0).x;
        float line_y = at(0).y;

        while (true) {
            size_t lo = start + 1;
            size_t hi = last + 1;
            while (lo < hi) {
                const size_t mid = lo + (hi - lo) / 2;
                if (at(mid).y > line_y)
                    hi = mid;
                else
                    lo = mid + 1;
            }

            if (lo > last) {
                emit(left, at(last).x, line_y);
                break;
            }

            const size_t brk = lo;
            const glm::vec2 before = at(brk - 1);
            if (text[bounds[brk - 1]] == '\n') {
                emit(left, before.x + k_newline_highlight_px, line_y);
                start = brk;
                left = at(brk).x;
            } else {
                emit(left, before.x, line_y);
                start = brk - 1;
                left = 0.F;
            }
            line_y = at(brk).y;
        }

        return rects;
    }

    /**
     * @brief Redraw the selection and the caret, after the glyphs.
     *
     * The slice's run already holds the field's text, so its own caret rect
     * and map put the caret exactly where the glyphs were laid out. The
     * selection is one translucent quad per line, in the text tint at
     * k_selection_alpha, written before the caret so the caret stays on top.
     * Caret and selection reuse params.color rather than adding config
     * fields. This is the one write that also shows any text change made to
     * the run just before it.
     */
    void render_tail(
        internal::TextSlice& slice,
        const Portal::Text::EditableText& edit,
        const Portal::Text::PressParams& params)
    {
        const auto map = slice.map();
        std::vector<Kakshya::MeshVertex> tail;

        if (const auto range = Portal::Text::selection(edit)) {
            glm::vec4 fill = params.color;
            fill.a *= k_selection_alpha;
            for (const auto& rect : selection_rects(slice.run(), edit.text, range->first, range->second)) {
                const glm::vec2 a = map(rect.min);
                const glm::vec2 b = map(rect.max);
                const auto quad = Kinesis::textured_mesh_rect(
                    { .min = glm::min(a, b), .max = glm::max(a, b) }, 0.F, fill);
                tail.insert(tail.end(), quad.begin(), quad.end());
            }
        }

        const Kinesis::AABB2D caret = slice.run().caret(edit.cursor, k_caret_width_px);
        const glm::vec2 a = map(caret.min);
        const glm::vec2 b = map(caret.max);
        const auto quad = Kinesis::textured_mesh_rect(
            { .min = glm::min(a, b), .max = glm::max(a, b) }, 0.F, params.color);
        tail.insert(tail.end(), quad.begin(), quad.end());

        slice.set_tail(std::move(tail));
    }

    /// @brief Lay the edited text out in the slice's run. Nothing is written
    ///        until render_tail(), which follows every call.
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
     *        the same x position it currently sits at, extending the
     *        selection when @p extend is set.
     *
     * Uses the run's caret positions to reach lines with no glyph quads.
     */
    void move_vertical(
        Portal::Text::EditableText& edit,
        internal::TextSlice& slice,
        int line_delta,
        bool extend)
    {
        auto& run = slice.run();
        const auto* atlas = run.atlas();
        if (!atlas)
            return;

        const auto caret = run.caret(edit.cursor, 0.F);
        const float line_height = static_cast<float>(atlas->line_height());
        const float target_top = caret.min.y + static_cast<float>(line_delta) * line_height;
        const float target_y = target_top + static_cast<float>(atlas->ascender());
        Portal::Text::move_to(edit, index_on_line(run, { caret.min.x, target_y }, target_top), extend);
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
        glm::vec2 ndc,
        bool extend)
    {
        auto& run = slice.run();
        const glm::vec2 px = pixel_at(ndc, field_bounds, run.extent());
        const auto* atlas = run.atlas();
        if (!atlas || atlas->line_height() == 0) {
            Portal::Text::move_to(edit, run.index_at(px), extend);
            return;
        }

        const float line_height = static_cast<float>(atlas->line_height());
        const float target_top = std::floor(px.y / line_height) * line_height;
        Portal::Text::move_to(edit, index_on_line(run, px, target_top), extend);
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

    render_tail(*slice, state->value, *params);

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
        render_tail(*sl, edit, *p);
    });

    ctx.press_and_hold(element_id, IO::Keys::Backspace, [sl = slice, s = state, p = params](uint32_t) mutable {
        Portal::Text::EditableText edit = s->value;
        Portal::Text::erase(edit, -1);
        s->write(edit);
        render_text(*sl, edit, *p);
        render_tail(*sl, edit, *p);
    });

    ctx.press_and_hold(element_id, IO::Keys::Delete, [sl = slice, s = state, p = params](uint32_t) mutable {
        Portal::Text::EditableText edit = s->value;
        Portal::Text::erase(edit, 1);
        s->write(edit);
        render_text(*sl, edit, *p);
        render_tail(*sl, edit, *p);
    });

    ctx.press_and_hold(element_id, IO::Keys::Tab, [sl = slice, s = state, p = params](uint32_t) mutable {
        Portal::Text::EditableText edit = s->value;
        Portal::Text::insert_literal(edit, '\t');
        s->write(edit);
        render_text(*sl, edit, *p);
        render_tail(*sl, edit, *p);
    });

    ctx.press_and_hold(element_id, IO::Keys::Enter, [sl = slice, s = state, p = params](uint32_t) mutable {
        Portal::Text::EditableText edit = s->value;
        Portal::Text::insert_literal(edit, '\n');
        s->write(edit);
        render_text(*sl, edit, *p);
        render_tail(*sl, edit, *p);
    });

    const Context* cx = &ctx;

    ctx.press_and_hold(element_id, IO::Keys::Left, [cx, sl = slice, s = state, p = params](uint32_t) mutable {
        Portal::Text::EditableText edit = s->value;
        if (shift_held(*cx))
            Portal::Text::extend(edit, -1);
        else
            Portal::Text::move(edit, -1);
        s->write(edit);
        render_tail(*sl, edit, *p);
    });

    ctx.press_and_hold(element_id, IO::Keys::Right, [cx, sl = slice, s = state, p = params](uint32_t) mutable {
        Portal::Text::EditableText edit = s->value;
        if (shift_held(*cx))
            Portal::Text::extend(edit, 1);
        else
            Portal::Text::move(edit, 1);
        s->write(edit);
        render_tail(*sl, edit, *p);
    });

    ctx.press_and_hold(element_id, IO::Keys::Up, [cx, sl = slice, s = state, p = params](uint32_t) mutable {
        Portal::Text::EditableText edit = s->value;
        move_vertical(edit, *sl, -1, shift_held(*cx));
        s->write(edit);
        render_tail(*sl, edit, *p);
    });

    ctx.press_and_hold(element_id, IO::Keys::Down, [cx, sl = slice, s = state, p = params](uint32_t) mutable {
        Portal::Text::EditableText edit = s->value;
        move_vertical(edit, *sl, 1, shift_held(*cx));
        s->write(edit);
        render_tail(*sl, edit, *p);
    });

    ctx.on_press(element_id, IO::MouseButtons::Left,
        [cx, sl = slice, s = state, p = params, fb = m_live_bounds](uint32_t, glm::vec2 ndc) mutable {
            Portal::Text::EditableText edit = s->value;
            move_to_click(edit, *sl, *fb, ndc, shift_held(*cx));
            s->write(edit);
            render_tail(*sl, edit, *p);
        });

    ctx.on_drag(element_id, IO::MouseButtons::Left,
        [sl = slice, s = state, p = params, fb = m_live_bounds](uint32_t, glm::vec2 ndc) mutable {
            Portal::Text::EditableText edit = s->value;
            move_to_click(edit, *sl, *fb, ndc, true);
            s->write(edit);
            render_tail(*sl, edit, *p);
        });

    ctx.on_press(element_id, IO::Keys::A | k_shortcut_modifier,
        [sl = slice, s = state, p = params](uint32_t) mutable {
            Portal::Text::EditableText edit = s->value;
            Portal::Text::select_all(edit);
            s->write(edit);
            render_tail(*sl, edit, *p);
        });

    ctx.on_press(element_id, IO::Keys::C | k_shortcut_modifier,
        [cx, s = state](uint32_t) {
            if (const std::string text = Portal::Text::selected_text(s->value); !text.empty())
                cx->set_clipboard_text(text);
        });

    ctx.on_press(element_id, IO::Keys::X | k_shortcut_modifier,
        [cx, sl = slice, s = state, p = params](uint32_t) mutable {
            Portal::Text::EditableText edit = s->value;
            const std::string text = Portal::Text::selected_text(edit);
            if (text.empty())
                return;
            cx->set_clipboard_text(text);
            Portal::Text::erase_selection(edit);
            s->write(edit);
            render_text(*sl, edit, *p);
            render_tail(*sl, edit, *p);
        });

    ctx.on_press(element_id, IO::Keys::V | k_shortcut_modifier,
        [cx, sl = slice, s = state, p = params](uint32_t) {
            cx->request_clipboard_text([sl, s, p](std::optional<std::string> text) {
                if (!text || text->empty())
                    return;
                Portal::Text::EditableText edit = s->value;
                Portal::Text::insert_text(edit, *text);
                s->write(edit);
                render_text(*sl, edit, *p);
                render_tail(*sl, edit, *p);
            });
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

TextField& TextField::scrollable(
    Surface& surface,
    Scrollable& scroller,
    std::shared_ptr<Portal::Text::PressParams> in_params,
    std::string initial_text,
    float content_multiplier)
{
    if (!scroller.buf) {
        MF_WARN(Journal::Component::Portal, Journal::Context::API,
            "TextField::scrollable: the scroller has not been placed");
        return *this;
    }

    return scrollable(
        FormaTarget::slice_of(scroller.buf.buffer(), Graphics::PrimitiveTopology::TRIANGLE_LIST),
        surface, scroller, std::move(in_params), std::move(initial_text), content_multiplier);
}

void TextField::reposition(Kinesis::AABB2D new_bounds)
{
    field_bounds = new_bounds;
    if (m_live_bounds)
        *m_live_bounds = new_bounds;

    if (slice && state && params) {
        slice->set_bounds(new_bounds);
        render_tail(*slice, state->value, *params);
    }
}

} // namespace MayaFlux::Portal::Forma
