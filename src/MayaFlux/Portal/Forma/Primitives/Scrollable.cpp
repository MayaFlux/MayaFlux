#include "Scrollable.hpp"

#include "MayaFlux/Portal/Forma/Surface.hpp"

#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Kinesis/GeometryPrimitives.hpp"

namespace MayaFlux::Portal::Forma {

namespace {

    void reflow(
        Kinesis::ScrollState& scroll,
        MappedState<glm::vec2>& offset,
        std::vector<ScrollableChild>& children,
        Layer& layer)
    {
        offset.write(scroll.offset);
        for (auto& child : children) {
            const Kinesis::AABB2D shifted = child.base_bounds.translated(scroll.offset);
            layer.set_bounds(child.id, shifted);
            child.reposition(shifted);
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

    m_children = std::make_shared<std::vector<ScrollableChild>>();

    cursor_out = LayoutCursor(viewport.max.y, viewport.min.x, viewport.max.x);
    cursor_out.bind_scroll(offset);

    const float speed = m_wheel_speed;

    surface.ctx().on_scroll(viewport_id,
        [scroll_state = scroll, scroll_offset = offset, children = m_children, surface, speed](
            uint32_t, glm::vec2, double dx, double dy) mutable {
            Kinesis::apply_scroll(*scroll_state,
                glm::vec2(static_cast<float>(dx), static_cast<float>(dy)) * speed);
            reflow(*scroll_state, *scroll_offset, *children, surface.layer());
        });

    return *this;
}

void Scrollable::track(
    uint32_t child_id,
    Kinesis::AABB2D base_bounds,
    std::function<void(Kinesis::AABB2D)> reposition,
    const std::shared_ptr<Buffers::FormaBuffer>& clip_buf)
{
    if (!scroll || !m_children)
        return;

    base_bounds = base_bounds.translated(-scroll->offset);

    scroll->content_bounds.min = glm::min(scroll->content_bounds.min, base_bounds.min);
    scroll->content_bounds.max = glm::max(scroll->content_bounds.max, base_bounds.max);

    if (clip_buf)
        clip_buf->get_render_processor()->set_scissor(Kinesis::Scissor::from(viewport_bounds));

    m_children->push_back({ .id = child_id, .base_bounds = base_bounds, .reposition = std::move(reposition) });
}

void Scrollable::scroll_by(Layer& layer, glm::vec2 delta)
{
    if (!scroll || !offset || !m_children)
        return;

    Kinesis::apply_scroll(*scroll, delta);
    reflow(*scroll, *offset, *m_children, layer);
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
