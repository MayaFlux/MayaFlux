#include "TextField.hpp"

#include "MayaFlux/Portal/Forma/Context.hpp"
#include "MayaFlux/Portal/Forma/Surface.hpp"

namespace MayaFlux::Portal::Forma {

namespace {

    void render(
        Element& el,
        const std::shared_ptr<MappedState<Portal::Text::EditableText>>& state,
        const std::shared_ptr<Portal::Text::PressParams>& params)
    {
        el.set_text(state->value.text, *params);
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

    wire(surface.ctx());

    return *this;
}

void TextField::wire(Context& ctx)
{
    ctx.on_text(element_id, [el = m_element, s = state, p = params](uint32_t, uint32_t codepoint) mutable {
        Portal::Text::EditableText edit = s->value;
        Portal::Text::insert_codepoint(edit, codepoint);
        s->write(edit);
        render(el, s, p);
    });

    ctx.press_and_hold(element_id, IO::Keys::Backspace, [el = m_element, s = state, p = params](uint32_t) mutable {
        Portal::Text::EditableText edit = s->value;
        Portal::Text::erase(edit, -1);
        s->write(edit);
        render(el, s, p);
    });

    ctx.press_and_hold(element_id, IO::Keys::Delete, [el = m_element, s = state, p = params](uint32_t) mutable {
        Portal::Text::EditableText edit = s->value;
        Portal::Text::erase(edit, 1);
        s->write(edit);
        render(el, s, p);
    });

    // TEXT_INPUT never carries '\t' (is_text_input_codepoint excludes < 0x20),
    // so a literal tab has to come from the key path, same as Backspace/Delete.
    // TypeSetter::lay_out() expands it at render time via its tab_width param.
    ctx.press_and_hold(element_id, IO::Keys::Tab, [el = m_element, s = state, p = params](uint32_t) mutable {
        Portal::Text::EditableText edit = s->value;
        Portal::Text::insert_literal(edit, '\t');
        s->write(edit);
        render(el, s, p);
    });

    // Same reasoning as Tab: '\n' never reaches TEXT_INPUT either, and
    // lay_out() already treats it as a line break.
    ctx.press_and_hold(element_id, IO::Keys::Enter, [el = m_element, s = state, p = params](uint32_t) mutable {
        Portal::Text::EditableText edit = s->value;
        Portal::Text::insert_literal(edit, '\n');
        s->write(edit);
        render(el, s, p);
    });

    ctx.press_and_hold(element_id, IO::Keys::Left, [s = state](uint32_t) {
        Portal::Text::EditableText edit = s->value;
        Portal::Text::move(edit, -1);
        s->write(edit);
    });

    ctx.press_and_hold(element_id, IO::Keys::Right, [s = state](uint32_t) {
        Portal::Text::EditableText edit = s->value;
        Portal::Text::move(edit, 1);
        s->write(edit);
    });
}

void TextField::reposition(Kinesis::AABB2D new_bounds)
{
    m_element.retarget(new_bounds);
    field_bounds = new_bounds;
}

} // namespace MayaFlux::Portal::Forma
