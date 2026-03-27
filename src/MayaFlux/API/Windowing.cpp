#include "Windowing.hpp"

#include "Core.hpp"
#include "MayaFlux/Core/Engine.hpp"
#include "MayaFlux/Core/Windowing/WindowManager.hpp"
#include "MayaFlux/Kinesis/ViewTransform.hpp"

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
    return Kinesis::to_ndc(window_x, window_y, window_width, window_height);
}

glm::vec3 normalize_coords(double window_x, double window_y,
    const std::shared_ptr<Core::Window>& window)
{
    const auto& s = window->get_state();
    return Kinesis::to_ndc(window_x, window_y, s.current_width, s.current_height);
}

glm::vec2 window_coords(double ndc_x, double ndc_y, double ndc_z,
    uint32_t window_width, uint32_t window_height)
{
    return Kinesis::to_window({ static_cast<float>(ndc_x), static_cast<float>(ndc_y), static_cast<float>(ndc_z) },
        window_width, window_height);
}

glm::vec2 window_coords(double ndc_x, double ndc_y, double ndc_z,
    const std::shared_ptr<Core::Window>& window)
{
    const auto& s = window->get_state();
    return Kinesis::to_window({ static_cast<float>(ndc_x), static_cast<float>(ndc_y), static_cast<float>(ndc_z) },
        s.current_width, s.current_height);
}

glm::vec2 window_coords(const glm::vec3& ndc_pos,
    uint32_t window_width, uint32_t window_height)
{
    return Kinesis::to_window(ndc_pos, window_width, window_height);
}

glm::vec2 window_coords(const glm::vec3& ndc_pos,
    const std::shared_ptr<Core::Window>& window)
{
    const auto& s = window->get_state();
    return Kinesis::to_window(ndc_pos, s.current_width, s.current_height);
}

float aspect_ratio(uint32_t window_width, uint32_t window_height)
{
    return Kinesis::aspect_ratio(window_width, window_height);
}

float aspect_ratio(const std::shared_ptr<Core::Window>& window)
{
    const auto& s = window->get_state();
    return Kinesis::aspect_ratio(s.current_width, s.current_height);
}

glm::vec3 normalize_coords_aspect(double window_x, double window_y,
    uint32_t window_width, uint32_t window_height)
{
    return Kinesis::to_ndc_aspect(window_x, window_y, window_width, window_height);
}

glm::vec3 normalize_coords_aspect(double window_x, double window_y,
    const std::shared_ptr<Core::Window>& window)
{
    const auto& s = window->get_state();
    return Kinesis::to_ndc_aspect(window_x, window_y, s.current_width, s.current_height);
}

bool is_in_bounds(double window_x, double window_y,
    uint32_t window_width, uint32_t window_height)
{
    return Kinesis::in_bounds(window_x, window_y, window_width, window_height);
}

bool is_in_bounds(double window_x, double window_y,
    const std::shared_ptr<Core::Window>& window)
{
    const auto& s = window->get_state();
    return Kinesis::in_bounds(window_x, window_y, s.current_width, s.current_height);
}

} // namespace MayaFlux
