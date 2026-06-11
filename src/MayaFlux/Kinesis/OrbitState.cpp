#include "OrbitState.hpp"

namespace MayaFlux::Kinesis {

OrbitState make_orbit_state(const OrbitConfig& config)
{
    OrbitState st;
    st.focal = config.focal_point;
    st.azimuth = config.initial_azimuth;
    st.elevation = config.initial_elevation;
    st.distance = config.initial_distance;
    st.fov_radians = config.fov_radians;
    st.near_plane = config.near_plane;
    st.far_plane = config.far_plane;
    st.rotate_sensitivity = config.rotate_sensitivity;
    st.pan_sensitivity = config.pan_sensitivity;
    st.scroll_speed = config.scroll_speed;
    st.min_distance = config.min_distance;
    st.max_distance = config.max_distance;
    return st;
}

glm::vec3 orbit_eye(const OrbitState& st)
{
    return st.focal + glm::vec3(std::cos(st.elevation) * std::sin(st.azimuth), std::sin(st.elevation), std::cos(st.elevation) * std::cos(st.azimuth)) * st.distance;
}

ViewTransform compute_orbit_view_transform(const OrbitState& st, float aspect)
{
    const glm::vec3 eye = orbit_eye(st);
    return {
        .view = glm::lookAt(eye, st.focal, glm::vec3(0.0F, 1.0F, 0.0F)),
        .projection = glm::perspective(st.fov_radians, aspect, st.near_plane, st.far_plane)
    };
}

void apply_orbit_rotate(OrbitState& st, float dx, float dy)
{
    st.azimuth += dx * st.rotate_sensitivity;
    st.elevation += dy * st.rotate_sensitivity;

    static constexpr float k_limit = glm::radians(89.0F);
    st.elevation = glm::clamp(st.elevation, -k_limit, k_limit);
}

void apply_orbit_scroll(OrbitState& st, float ticks)
{
    st.distance *= (1.0F - ticks * st.scroll_speed);
    st.distance = glm::clamp(st.distance, st.min_distance, st.max_distance);
}

void apply_orbit_pan(OrbitState& st, float dx, float dy)
{
    const glm::vec3 forward = glm::normalize(st.focal - orbit_eye(st));
    const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0F, 1.0F, 0.0F)));
    const glm::vec3 up = glm::normalize(glm::cross(right, forward));

    const float scale = st.distance * st.pan_sensitivity;
    st.focal -= right * (dx * scale);
    st.focal += up * (dy * scale);
}

void snap_orbit_ortho(OrbitState& st, int view)
{
    switch (view) {
    case 0:
        st.azimuth = 0.0F;
        st.elevation = 0.0F;
        break;
    case 1:
        st.azimuth = glm::radians(-90.0F);
        st.elevation = 0.0F;
        break;
    case 2:
        st.azimuth = 0.0F;
        st.elevation = glm::radians(-89.0F);
        break;
    case 3:
        st.azimuth += glm::pi<float>();
        st.elevation = -st.elevation;
        break;
    default:
        break;
    }
}

} // namespace MayaFlux::Kinesis
