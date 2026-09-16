#include "TextField.hpp"

#include "MayaFlux/Portal/Forma/Surface.hpp"

#include "MayaFlux/Portal/Text/Text.hpp"

#include "MayaFlux/Buffers/Forma/FormaBuffer.hpp"
#include "MayaFlux/Kinesis/GeometryPrimitives.hpp"

namespace MayaFlux::Portal::Forma {

namespace {

    /// @brief Caret width in pixels, same space as PressParams::render_bounds.
    constexpr float k_caret_width_px = 2.F;

    Portal::Text::GlyphAtlas& resolve_atlas(const Portal::Text::PressParams& params)
    {
        return params.atlas ? *params.atlas : Portal::Text::get_default_atlas();
    }

    /**
     * @brief Caret's NDC rect for the current cursor, spanning one full
     *        line height on whatever line it falls on.
     *
     * Mirrors the pen origin press()/repress() rasterize text at
     * internally (pen_x=0, pen_y=ascender(), wrap_w=render_bounds.x) so the
     * caret lines up with the actually-rasterized glyphs, then maps that
     * pixel rect into field_bounds the same way the text quad's own UV
     * (0,0)-(1,1) already maps render_bounds onto it.
     */
    Kinesis::AABB2D caret_ndc(
        const Portal::Text::EditableText& edit,
        const Portal::Text::PressParams& params,
        Kinesis::AABB2D field_bounds)
    {
        auto& atlas = resolve_atlas(params);

        const auto pen = Portal::Text::x_at(
            edit.text, atlas, edit.cursor,
            0.F, static_cast<float>(atlas.ascender()),
            params.render_bounds.x, 4);

        const float top_px = pen.y - static_cast<float>(atlas.ascender());
        const float bottom_px = top_px + static_cast<float>(atlas.line_height());
        const glm::vec2 render_bounds(params.render_bounds);

        return Kinesis::AABB2D {
            .min = {
                field_bounds.min.x + (pen.x / render_bounds.x) * field_bounds.width(),
                field_bounds.max.y - (bottom_px / render_bounds.y) * field_bounds.height() },
            .max = { field_bounds.min.x + ((pen.x + k_caret_width_px) / render_bounds.x) * field_bounds.width(), field_bounds.max.y - (top_px / render_bounds.y) * field_bounds.height() }
        };
    }

    /**
     * @brief Resubmit the text quad and the caret quad together.
     *
     * FormaBuffer::submit() replaces the whole buffer and has no
     * partial-range write, so every call - keystroke or reposition() -
     * sends both quads (12 MeshVertex, 720 bytes) even though usually only
     * one of them actually moved. Caret color reuses params.color (the
     * text tint) rather than adding a new config field.
     */
    void render_geometry(
        Buffers::FormaBuffer& buf,
        const Portal::Text::EditableText& edit,
        const Portal::Text::PressParams& params,
        Kinesis::AABB2D field_bounds)
    {
        const auto text_quad = Kinesis::textured_mesh_rect(field_bounds, 1.F);
        const auto caret_quad = Kinesis::textured_mesh_rect(
            caret_ndc(edit, params, field_bounds), 0.F, glm::vec3(params.color));

        std::array<Kakshya::MeshVertex, 12> combined {};
        for (size_t i = 0; i < text_quad.size(); ++i)
            combined[i] = text_quad[i];
        for (size_t i = 0; i < caret_quad.size(); ++i)
            combined[text_quad.size() + i] = caret_quad[i];

        buf.submit(combined);
    }

    /// @brief Re-rasterize text content. Skip for a pure cursor move
    ///        (Left/Right): content is unchanged, only render_geometry()
    ///        (the caret) needs to run.
    void render_text(
        Element& el,
        const Portal::Text::EditableText& edit,
        const Portal::Text::PressParams& params)
    {
        el.set_text(edit.text, params);
    }

} // namespace

TextField& TextField::place(
    std::shared_ptr<Buffers::FormaBuffer> in_buf,
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

    element_id = surface.layer().add(m_element);
    m_element.id = element_id;

    state = std::make_shared<MappedState<Portal::Text::EditableText>>();
    state->value.cursor = initial_text.size();
    state->value.text = std::move(initial_text);

    render_geometry(*buf, state->value, *params, bounds);

    wire(surface.ctx());

    return *this;
}

void TextField::wire(Context& ctx)
{
    ctx.on_text(element_id, [el = m_element, s = state, p = params, b = buf, fb = m_live_bounds](uint32_t, uint32_t codepoint) mutable {
        Portal::Text::EditableText edit = s->value;
        Portal::Text::insert_codepoint(edit, codepoint);
        s->write(edit);
        render_text(el, edit, *p);
        render_geometry(*b, edit, *p, *fb);
    });

    ctx.press_and_hold(element_id, IO::Keys::Backspace, [el = m_element, s = state, p = params, b = buf, fb = m_live_bounds](uint32_t) mutable {
        Portal::Text::EditableText edit = s->value;
        Portal::Text::erase(edit, -1);
        s->write(edit);
        render_text(el, edit, *p);
        render_geometry(*b, edit, *p, *fb);
    });

    ctx.press_and_hold(element_id, IO::Keys::Delete, [el = m_element, s = state, p = params, b = buf, fb = m_live_bounds](uint32_t) mutable {
        Portal::Text::EditableText edit = s->value;
        Portal::Text::erase(edit, 1);
        s->write(edit);
        render_text(el, edit, *p);
        render_geometry(*b, edit, *p, *fb);
    });

    ctx.press_and_hold(element_id, IO::Keys::Tab, [el = m_element, s = state, p = params, b = buf, fb = m_live_bounds](uint32_t) mutable {
        Portal::Text::EditableText edit = s->value;
        Portal::Text::insert_literal(edit, '\t');
        s->write(edit);
        render_text(el, edit, *p);
        render_geometry(*b, edit, *p, *fb);
    });

    ctx.press_and_hold(element_id, IO::Keys::Enter, [el = m_element, s = state, p = params, b = buf, fb = m_live_bounds](uint32_t) mutable {
        Portal::Text::EditableText edit = s->value;
        Portal::Text::insert_literal(edit, '\n');
        s->write(edit);
        render_text(el, edit, *p);
        render_geometry(*b, edit, *p, *fb);
    });

    ctx.press_and_hold(element_id, IO::Keys::Left, [s = state, p = params, b = buf, fb = m_live_bounds](uint32_t) mutable {
        Portal::Text::EditableText edit = s->value;
        Portal::Text::move(edit, -1);
        s->write(edit);
        render_geometry(*b, edit, *p, *fb);
    });

    ctx.press_and_hold(element_id, IO::Keys::Right, [s = state, p = params, b = buf, fb = m_live_bounds](uint32_t) mutable {
        Portal::Text::EditableText edit = s->value;
        Portal::Text::move(edit, 1);
        s->write(edit);
        render_geometry(*b, edit, *p, *fb);
    });
}

TextField& TextField::scrollable(
    std::shared_ptr<Buffers::FormaBuffer> in_buf,
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
        element_id, content_bounds,
        [field](Kinesis::AABB2D shifted) mutable { field.reposition(shifted); },
        buf);

    return *this;
}

void TextField::reposition(Kinesis::AABB2D new_bounds)
{
    field_bounds = new_bounds;
    if (m_live_bounds)
        *m_live_bounds = new_bounds;

    if (buf && state && params)
        render_geometry(*buf, state->value, *params, new_bounds);
}

} // namespace MayaFlux::Portal::Forma
