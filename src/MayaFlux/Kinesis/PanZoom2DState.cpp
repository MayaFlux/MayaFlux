#include "PanZoom2DState.hpp"

namespace MayaFlux::Kinesis {

PanZoom2DState make_pan_zoom_state(const PanZoom2DConfig& config)
{
    PanZoom2DState st;
    st.pan = config.initial_pan;
    st.zoom = config.initial_zoom;
    st.scroll_speed = config.scroll_speed;
    st.min_zoom = config.min_zoom;
    st.max_zoom = config.max_zoom;
    st.near_plane = config.near_plane;
    st.far_plane = config.far_plane;
    return st;
}

ViewTransform compute_pan_zoom_view_transform(const PanZoom2DState& st, float aspect)
{
    const float hw = st.zoom * aspect;
    const float hh = st.zoom;
    return {
        .view = glm::mat4(1.0F),
        .projection = glm::ortho(
            st.pan.x - hw, st.pan.x + hw,
            st.pan.y - hh, st.pan.y + hh,
            st.near_plane, st.far_plane)
    };
}

void apply_pan_zoom_pan(PanZoom2DState& st, float dx, float dy,
    float viewport_width, float viewport_height)
{
    const float aspect = viewport_width / viewport_height;
    st.pan.x -= (dx / viewport_width) * (2.0F * st.zoom * aspect);
    st.pan.y += (dy / viewport_height) * (2.0F * st.zoom);
}

void apply_pan_zoom_scroll(PanZoom2DState& st, float ticks)
{
    st.zoom *= (1.0F - ticks * st.scroll_speed);
    st.zoom = glm::clamp(st.zoom, st.min_zoom, st.max_zoom);
}

} // namespace MayaFlux::Kinesis
