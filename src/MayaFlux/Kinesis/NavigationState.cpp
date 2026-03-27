#include "NavigationState.hpp"

namespace MayaFlux::Kinesis {

NavigationState make_navigation_state(const NavigationConfig& config)
{
    NavigationState st;
    st.eye = config.initial_eye;
    st.move_speed = config.move_speed;
    st.mouse_sensitivity = config.mouse_sensitivity;
    st.scroll_speed = config.scroll_speed;
    st.fov_radians = config.fov_radians;
    st.near_plane = config.near_plane;
    st.far_plane = config.far_plane;

    const glm::vec3 dir = glm::normalize(config.initial_target - config.initial_eye);
    st.yaw = std::atan2(dir.x, dir.z);
    st.pitch = std::asin(glm::clamp(dir.y, -1.0F, 1.0F));

    return st;
}

ViewTransform compute_view_transform(NavigationState& st, float aspect)
{
    const auto now = std::chrono::steady_clock::now();
    const float dt = std::chrono::duration<float>(now - st.last_tick).count();
    st.last_tick = now;

    const glm::vec3 forward {
        std::cos(st.pitch) * std::sin(st.yaw),
        std::sin(st.pitch),
        std::cos(st.pitch) * std::cos(st.yaw)
    };
    const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0F, 1.0F, 0.0F)));

    const float step = st.move_speed * dt;

    if (st.forward_held)
        st.eye += forward * step;
    if (st.back_held)
        st.eye -= forward * step;
    if (st.left_held)
        st.eye -= right * step;
    if (st.right_held)
        st.eye += right * step;
    if (st.down_held)
        st.eye -= glm::vec3(0.0F, 1.0F, 0.0F) * step;
    if (st.up_held)
        st.eye += glm::vec3(0.0F, 1.0F, 0.0F) * step;

    return {
        .view = glm::lookAt(st.eye, st.eye + forward, glm::vec3(0.0F, 1.0F, 0.0F)),
        .projection = glm::perspective(st.fov_radians, aspect, st.near_plane, st.far_plane)
    };
}

void apply_mouse_delta(NavigationState& st, float dx, float dy)
{
    st.yaw += dx * st.mouse_sensitivity;
    st.pitch += dy * st.mouse_sensitivity;

    static constexpr float k_limit = glm::radians(89.0F);
    st.pitch = glm::clamp(st.pitch, -k_limit, k_limit);
}

void apply_scroll(NavigationState& st, float ticks)
{
    const glm::vec3 forward {
        std::cos(st.pitch) * std::sin(st.yaw),
        std::sin(st.pitch),
        std::cos(st.pitch) * std::cos(st.yaw)
    };
    st.eye += forward * (ticks * st.scroll_speed);
}

void snap_ortho(NavigationState& st, int view)
{
    const float dist = glm::length(st.eye);

    switch (view) {
    case 0: // front: camera on +Z looking toward -Z
        st.eye = glm::vec3(0.0F, 0.0F, dist);
        st.yaw = 0.0F;
        st.pitch = 0.0F;
        break;
    case 1: // right: camera on +X looking toward -X
        st.eye = glm::vec3(dist, 0.0F, 0.0F);
        st.yaw = glm::radians(-90.0F);
        st.pitch = 0.0F;
        break;
    case 2: // top: camera on +Y looking straight down
        st.eye = glm::vec3(0.0F, dist, 0.0F);
        st.yaw = 0.0F;
        st.pitch = glm::radians(-89.0F);
        break;
    case 3: // flip: mirror through origin
        st.eye = -st.eye;
        st.yaw = st.yaw + glm::pi<float>();
        st.pitch = -st.pitch;
        break;
    default:
        break;
    }
}

} // namespace MayaFlux::Kinesis
