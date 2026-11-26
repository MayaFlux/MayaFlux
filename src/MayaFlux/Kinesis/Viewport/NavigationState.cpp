#include "NavigationState.hpp"

#include <glm/gtc/quaternion.hpp>

namespace MayaFlux::Kinesis {

namespace {

    glm::vec3 forward_of(const NavigationState& st)
    {
        return {
            std::cos(st.pitch) * std::sin(st.yaw),
            std::sin(st.pitch),
            std::cos(st.pitch) * std::cos(st.yaw)
        };
    }

    glm::vec3 level_right(float yaw)
    {
        return { -std::cos(yaw), 0.0F, std::sin(yaw) };
    }

    glm::vec3 up_of(const NavigationState& st)
    {
        const glm::vec3 forward = forward_of(st);
        const glm::vec3 up = glm::cross(level_right(st.yaw), forward);
        return st.roll == 0.0F ? up : glm::vec3(glm::rotate(glm::mat4(1.0F), st.roll, forward) * glm::vec4(up, 0.0F));
    }

}

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
    advance_navigation(st);
    return build_view_transform(st, aspect);
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

void advance_navigation(NavigationState& st)
{
    const auto now = std::chrono::steady_clock::now();
    const float dt = std::chrono::duration<float>(now - st.last_tick).count();
    st.last_tick = now;

    advance_navigation(st, dt);
}

void advance_navigation(NavigationState& st, float dt)
{
    const glm::vec3 forward {
        std::cos(st.pitch) * std::sin(st.yaw),
        std::sin(st.pitch),
        std::cos(st.pitch) * std::cos(st.yaw)
    };
    const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0F, 1.0F, 0.0F)));

    const float step = st.move_speed * dt;

    glm::vec3 candidate = st.eye;

    if (st.forward_held)
        candidate += forward * step;
    if (st.back_held)
        candidate -= forward * step;
    if (st.left_held)
        candidate -= right * step;
    if (st.right_held)
        candidate += right * step;
    if (st.down_held)
        candidate -= glm::vec3(0.0F, 1.0F, 0.0F) * step;
    if (st.up_held)
        candidate += glm::vec3(0.0F, 1.0F, 0.0F) * step;

    st.eye = st.eye_constraint ? st.eye_constraint(candidate) : candidate;
}

ViewTransform build_view_transform(const NavigationState& st, float aspect)
{
    return {
        .view = glm::lookAt(st.eye, st.eye + forward_of(st), up_of(st)),
        .projection = st.frustum
            ? frustum_projection(*st.frustum, aspect)
            : glm::perspective(st.fov_radians, aspect, st.near_plane, st.far_plane)
    };
}

Viewpoint viewpoint_of(const NavigationState& st)
{
    const Frustum frustum = st.frustum
        ? *st.frustum
        : frustum_from_fov(st.fov_radians, 1.5, st.near_plane, st.far_plane);
    return { glm::inverse(glm::lookAt(st.eye, st.eye + forward_of(st), up_of(st))), frustum };
}

void set_viewpoint(NavigationState& st, const Viewpoint& view)
{
    if (same_viewpoint(view, viewpoint_of(st))) {
        return;
    }

    const auto& [placement, frustum] = view;
    const glm::vec3 right = glm::normalize(glm::vec3(placement[0]));
    const glm::vec3 up = glm::normalize(glm::vec3(placement[1]));
    const glm::vec3 forward = -glm::normalize(glm::vec3(placement[2]));

    st.eye = glm::vec3(placement[3]);
    st.pitch = std::asin(glm::clamp(forward.y, -1.0F, 1.0F));
    st.yaw = std::abs(forward.y) < 0.999F
        ? std::atan2(forward.x, forward.z)
        : std::atan2(right.z, -right.x);

    const glm::vec3 level = glm::cross(level_right(st.yaw), forward_of(st));
    st.roll = std::atan2(glm::dot(glm::cross(level, up), forward), glm::dot(level, up));

    const bool plain = frustum.fit == AspectFit::Height && !frustum.orthographic
        && frustum.window.min.x == -frustum.window.max.x && frustum.window.min.y == -frustum.window.max.y;
    st.frustum = plain ? std::nullopt : std::optional<Frustum> { frustum };
    st.fov_radians = static_cast<float>(frustum_vertical_fov(frustum));
    st.near_plane = static_cast<float>(frustum.near);
    st.far_plane = static_cast<float>(frustum.far);
}

Viewpoint mix_viewpoints(const Viewpoint& from, const Viewpoint& to, float t)
{
    const float at = std::clamp(t, 0.0F, 1.0F);
    const glm::quat turn = glm::slerp(
        glm::normalize(glm::quat_cast(glm::mat3(from.first))),
        glm::normalize(glm::quat_cast(glm::mat3(to.first))), at);

    glm::mat4 placement = glm::mat4_cast(turn);
    placement[3] = glm::vec4(glm::mix(glm::vec3(from.first[3]), glm::vec3(to.first[3]), at), 1.0F);

    const Frustum& a = from.second;
    const Frustum& b = to.second;
    Frustum frustum = at < 0.5F ? a : b;
    frustum.window.min = glm::mix(a.window.min, b.window.min, at);
    frustum.window.max = glm::mix(a.window.max, b.window.max, at);
    frustum.near = a.near + (b.near - a.near) * static_cast<double>(at);
    frustum.far = a.far + (b.far - a.far) * static_cast<double>(at);
    return { placement, frustum };
}

} // namespace MayaFlux::Kinesis
