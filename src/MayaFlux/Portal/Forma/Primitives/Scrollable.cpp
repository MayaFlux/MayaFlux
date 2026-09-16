#include "Scrollable.hpp"

#include "MayaFlux/Portal/Forma/Surface.hpp"

#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Kinesis/GeometryPrimitives.hpp"

namespace MayaFlux::Portal::Forma {

namespace {

    /// @brief (position_frac, extent_frac) pair an indicator's geometry function reacts to.
    glm::vec2 scroll_extent(const Kinesis::ScrollState& scroll) noexcept
    {
        const float content_h = scroll.content_bounds.height();
        const float viewport_h = scroll.viewport_bounds.height();
        const float max_offset_y = std::max(0.F, content_h - viewport_h);

        const float extent_frac = content_h > 0.F ? std::clamp(viewport_h / content_h, 0.F, 1.F) : 1.F;
        const float position_frac = max_offset_y > 0.F ? std::clamp(scroll.offset.y / max_offset_y, 0.F, 1.F) : 0.F;

        return { position_frac, extent_frac };
    }

    void reflow(
        Kinesis::ScrollState& scroll,
        MappedState<glm::vec2>& offset,
        std::vector<Tracked>& tracked,
        Layer& layer,
        Mapped<glm::vec2>* indicator)
    {
        offset.write(scroll.offset);
        for (auto& t : tracked) {
            const Kinesis::AABB2D shifted = t.base_bounds.translated(scroll.offset);
            layer.set_bounds(t.id, shifted);
            t.reposition(shifted);
        }

        if (indicator && indicator->state) {
            indicator->state->write(scroll_extent(scroll));
            indicator->sync(&layer);
        }
    }

} // namespace

Scrollable& Scrollable::place(
    std::shared_ptr<Buffers::FormaBuffer> in_buf,
    Surface& surface,
    Kinesis::AABB2D viewport)
{
    buf = std::move(in_buf);
    viewport_bounds = viewport;

    buf->submit(Kinesis::filled_rect(viewport, m_background));

    Element el = Element {}
                     .with_name("scrollable_viewport")
                     .with_bounds(viewport)
                     .with_buffer(buf);

    viewport_id = surface.layer().add(el);

    scroll = std::make_shared<Kinesis::ScrollState>();
    scroll->viewport_bounds = viewport;
    scroll->content_bounds = viewport;

    offset = std::make_shared<MappedState<glm::vec2>>();

    m_tracked = std::make_shared<std::vector<Tracked>>();
    m_indicator = std::make_shared<Mapped<glm::vec2>>();

    cursor_out = LayoutCursor(viewport.max.y, viewport.min.x, viewport.max.x);
    cursor_out.bind_scroll(offset);

    const float speed = m_wheel_speed;

    surface.ctx().on_scroll(viewport_id,
        [scroll_state = scroll, scroll_offset = offset, tracked = m_tracked, indicator = m_indicator, surface, speed](
            uint32_t, glm::vec2, double dx, double dy) mutable {
            Kinesis::apply_scroll(*scroll_state,
                glm::vec2(static_cast<float>(dx), static_cast<float>(dy)) * speed);
            reflow(*scroll_state, *scroll_offset, *tracked, surface.layer(), indicator.get());
        });

    return *this;
}

void Scrollable::track(
    uint32_t id,
    Kinesis::AABB2D base_bounds,
    std::function<void(Kinesis::AABB2D)> reposition,
    const std::shared_ptr<Buffers::FormaBuffer>& clip_buf)
{
    if (!scroll || !m_tracked)
        return;

    base_bounds = base_bounds.translated(-scroll->offset);

    scroll->content_bounds.min = glm::min(scroll->content_bounds.min, base_bounds.min);
    scroll->content_bounds.max = glm::max(scroll->content_bounds.max, base_bounds.max);

    if (clip_buf)
        clip_buf->get_render_processor()->set_scissor(Kinesis::Scissor::from(viewport_bounds));

    m_tracked->push_back({ .id = id, .base_bounds = base_bounds, .reposition = std::move(reposition) });

    if (m_indicator && m_indicator->state) {
        m_indicator->state->write(scroll_extent(*scroll));
        m_indicator->sync();
    }
}

void Scrollable::scroll_by(Layer& layer, glm::vec2 delta)
{
    if (!scroll || !offset || !m_tracked)
        return;

    Kinesis::apply_scroll(*scroll, delta);
    reflow(*scroll, *offset, *m_tracked, layer, m_indicator.get());
}

void Scrollable::indicator(Surface& surface, Mapped<glm::vec2> el, Kinesis::AABB2D track)
{
    if (!scroll || !offset || !m_tracked || !m_indicator)
        return;

    *m_indicator = std::move(el);
    m_indicator->state->write(scroll_extent(*scroll));
    m_indicator->sync(&surface.layer());

    auto scroll_state = scroll;
    auto scroll_offset = offset;
    auto tracked = m_tracked;
    auto ind = m_indicator;

    surface.ctx().on_drag(ind->element.id, IO::MouseButtons::Left,
        [scroll_state, scroll_offset, tracked, ind, surface, track](uint32_t, glm::vec2 ndc) mutable {
            const float max_offset_y = std::max(0.F,
                scroll_state->content_bounds.height() - scroll_state->viewport_bounds.height());
            if (max_offset_y <= 0.F)
                return;

            const float frac = std::clamp((track.max.y - ndc.y) / track.height(), 0.F, 1.F);
            const float target = frac * max_offset_y;

            Kinesis::apply_scroll(*scroll_state, { 0.F, target - scroll_state->offset.y });
            reflow(*scroll_state, *scroll_offset, *tracked, surface.layer(), ind.get());
        });
}

Scrollable make_scrollable(
    std::shared_ptr<Buffers::FormaBuffer> buf,
    Surface& surface,
    Kinesis::AABB2D viewport,
    glm::vec3 background_color,
    float wheel_speed)
{
    return Scrollable {}
        .background_color(background_color)
        .wheel_speed(wheel_speed)
        .place(std::move(buf), surface, viewport);
}

} // namespace MayaFlux::Portal::Forma
