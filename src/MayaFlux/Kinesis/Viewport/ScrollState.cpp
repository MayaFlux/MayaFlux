#include "ScrollState.hpp"

namespace MayaFlux::Kinesis {

glm::vec2 drag_to_offset_delta(
    glm::vec2 pixel_delta,
    glm::vec2 visible_extent,
    glm::vec2 viewport_size_pixels) noexcept
{
    return (pixel_delta / viewport_size_pixels) * visible_extent;
}

glm::vec2 clamp_offset_to_content(
    glm::vec2 offset,
    glm::vec2 viewport_extent,
    AABB2D content_bounds) noexcept
{
    const float max_x = std::max(0.F, content_bounds.width() - viewport_extent.x);
    const float max_y = std::max(0.F, content_bounds.height() - viewport_extent.y);

    return { std::clamp(offset.x, 0.F, max_x), std::clamp(offset.y, 0.F, max_y) };
}

void apply_scroll(ScrollState& state, glm::vec2 delta) noexcept
{
    state.offset = clamp_offset_to_content(
        state.offset + delta,
        glm::vec2(state.viewport_bounds.width(), state.viewport_bounds.height()),
        state.content_bounds);
}

void apply_wheel_scroll(ScrollState& state, double dx, double dy, float speed) noexcept
{
    apply_scroll(state, glm::vec2(static_cast<float>(dx), static_cast<float>(dy)) * speed);
}

ViewTransform scroll_view_transform(const ScrollState& state) noexcept
{
    return {
        .view = glm::translate(glm::mat4(1.0F), glm::vec3(state.offset, 0.F)),
        .projection = glm::mat4(1.0F),
    };
}

} // namespace MayaFlux::Kinesis
