#include "GlfwSingleton.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

#include "GLFW/glfw3.h"

namespace MayaFlux::Core {

bool GLFWSingleton::initialize()
{
    if (s_initialized)
        return true;

    glfwSetErrorCallback([](int error, const char* description) {
        MF_ERROR(Journal::Component::Core, Journal::Context::WindowingBackend, "GLFW Error {}: {}", error, description);
    });

    if (!glfwInit()) {
        MF_ERROR(Journal::Component::Core, Journal::Context::WindowingBackend, "Failed to initialize GLFW");
        return false;
    }

    s_initialized = true;
    s_window_count = 0;
    return true;
}

void GLFWSingleton::terminate()
{
    if (s_initialized && s_window_count == 0) {
        glfwTerminate();
        s_initialized = false;
    }
}

std::vector<MonitorInfo> GLFWSingleton::enumerate_monitors()
{
    if (!s_initialized)
        return {};

    int count {};
    GLFWmonitor** monitors = glfwGetMonitors(&count);

    std::vector<MonitorInfo> infos;
    infos.reserve(count);

    GLFWmonitor* primary = glfwGetPrimaryMonitor();

    for (int i = 0; i < count; ++i) {
        auto* mode = glfwGetVideoMode(monitors[i]);
        int w_mm, h_mm;
        glfwGetMonitorPhysicalSize(monitors[i], &w_mm, &h_mm);

        infos.push_back({ .id = i,
            .name = glfwGetMonitorName(monitors[i]),
            .width_mm = w_mm,
            .height_mm = h_mm,
            .current_mode = {
                .width = static_cast<uint32_t>(mode->width),
                .height = static_cast<uint32_t>(mode->height),
                .refresh_rate = static_cast<uint32_t>(mode->refreshRate),
                .red_bits = static_cast<uint8_t>(mode->redBits),
                .green_bits = static_cast<uint8_t>(mode->greenBits),
                .blue_bits = static_cast<uint8_t>(mode->blueBits) },
            .is_primary = (monitors[i] == primary) });
    }

    return infos;
}

MonitorInfo GLFWSingleton::get_primary_monitor()
{
    auto monitors = enumerate_monitors();
    for (const auto& m : monitors) {
        if (m.is_primary)
            return m;
    }
    return {};
}

}
