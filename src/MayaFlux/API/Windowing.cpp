// Windowing.cpp
#include "Windowing.hpp"

#include "Core.hpp"
#include "MayaFlux/Core/Engine.hpp"
#include "MayaFlux/Core/Windowing/WindowManager.hpp"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace MayaFlux {

Core::WindowManager& get_window_manager()
{
    return *get_context().get_window_manager();
}

std::shared_ptr<Core::Window> create_window(const Core::WindowCreateInfo& create_info)
{
    return get_window_manager().create_window(create_info);
}

glm::vec3 normalize_coords(double window_x, double window_y,
    uint32_t window_width, uint32_t window_height)
{
    float norm_x = (static_cast<float>(window_x) / static_cast<float>(window_width)) * 2.0F - 1.0F;
    float norm_y = 1.F - (static_cast<float>(window_y) / static_cast<float>(window_height)) * 2.0F;
    return { norm_x, norm_y, 0.0F };
}

glm::vec3 normalize_coords(double window_x, double window_y,
    const std::shared_ptr<Core::Window>& window)
{
    const auto& state = window->get_state();
    return normalize_coords(window_x, window_y, state.current_width, state.current_height);
}

glm::vec2 window_coords(double ndc_x, double ndc_y, double ndc_z,
    uint32_t window_width, uint32_t window_height)
{
    (void)ndc_z;
    float window_x = (static_cast<float>(ndc_x) + 1.0F) * 0.5F * static_cast<float>(window_width);
    float window_y = (1.0F - static_cast<float>(ndc_y)) * 0.5F * static_cast<float>(window_height);
    return { window_x, window_y };
}

glm::vec2 window_coords(double ndc_x, double ndc_y, double ndc_z,
    const std::shared_ptr<Core::Window>& window)
{
    const auto& state = window->get_state();
    return window_coords(ndc_x, ndc_y, ndc_z, state.current_width, state.current_height);
}

glm::vec2 window_coords(const glm::vec3& ndc_pos,
    uint32_t window_width, uint32_t window_height)
{
    return window_coords(ndc_pos.x, ndc_pos.y, ndc_pos.z, window_width, window_height);
}

glm::vec2 window_coords(const glm::vec3& ndc_pos,
    const std::shared_ptr<Core::Window>& window)
{
    const auto& state = window->get_state();
    return window_coords(ndc_pos.x, ndc_pos.y, ndc_pos.z, state.current_width, state.current_height);
}

float aspect_ratio(uint32_t window_width, uint32_t window_height)
{
    return static_cast<float>(window_width) / static_cast<float>(window_height);
}

float aspect_ratio(const std::shared_ptr<Core::Window>& window)
{
    const auto& state = window->get_state();
    return aspect_ratio(state.current_width, state.current_height);
}

glm::vec3 normalize_coords_aspect(double window_x, double window_y,
    uint32_t window_width, uint32_t window_height)
{
    float aspect = aspect_ratio(window_width, window_height);
    float norm_x = (static_cast<float>(window_x) / static_cast<float>(window_width)) * 2.0F - 1.0F;
    float norm_y = 1.0F - (static_cast<float>(window_y) / static_cast<float>(window_height)) * 2.0F;

    if (aspect > 1.0F) {
        norm_y /= aspect;
    } else {
        norm_x *= aspect;
    }

    return { norm_x, norm_y, 0.0F };
}

glm::vec3 normalize_coords_aspect(double window_x, double window_y,
    const std::shared_ptr<Core::Window>& window)
{
    const auto& state = window->get_state();
    return normalize_coords_aspect(window_x, window_y, state.current_width, state.current_height);
}

bool is_in_bounds(double window_x, double window_y,
    uint32_t window_width, uint32_t window_height)
{
    return window_x >= 0.0 && window_x < static_cast<double>(window_width) && window_y >= 0.0 && window_y < static_cast<double>(window_height);
}

bool is_in_bounds(double window_x, double window_y,
    const std::shared_ptr<Core::Window>& window)
{
    const auto& state = window->get_state();
    return is_in_bounds(window_x, window_y, state.current_width, state.current_height);
}

}
