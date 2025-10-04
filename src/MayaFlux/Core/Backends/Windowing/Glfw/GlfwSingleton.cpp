#include "GlfwSingleton.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

#include "GLFW/glfw3.h"

namespace MayaFlux::Core {

bool GLFWSingleton::s_initialized {};
uint32_t GLFWSingleton::s_window_count {};
std::function<void(int, const char*)> GLFWSingleton::s_error_callback;

bool GLFWSingleton::initialize()
{
    if (s_initialized)
        return true;

    glfwSetErrorCallback([](int error, const char* description) {
        MF_ERROR(Journal::Component::Core, Journal::Context::WindowingSubsystem, "GLFW Error {}: {}", error, description);
    });

    if (!glfwInit()) {
        MF_ERROR(Journal::Component::Core, Journal::Context::WindowingSubsystem, "Failed to initialize GLFW");
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

    std::span<GLFWmonitor*> monitor_span(monitors, count);

    std::vector<MonitorInfo> infos;
    infos.reserve(count);

    GLFWmonitor* primary = glfwGetPrimaryMonitor();

    for (int i = 0; i < count; ++i) {
        auto* mode = glfwGetVideoMode(monitor_span[i]);
        int w_mm {}, h_mm {};
        glfwGetMonitorPhysicalSize(monitor_span[i], &w_mm, &h_mm);

        infos.push_back({ .id = i,
            .name = glfwGetMonitorName(monitor_span[i]),
            .width_mm = w_mm,
            .height_mm = h_mm,
            .current_mode = {
                .width = static_cast<uint32_t>(mode->width),
                .height = static_cast<uint32_t>(mode->height),
                .refresh_rate = static_cast<uint32_t>(mode->refreshRate),
                .red_bits = static_cast<uint8_t>(mode->redBits),
                .green_bits = static_cast<uint8_t>(mode->greenBits),
                .blue_bits = static_cast<uint8_t>(mode->blueBits) },
            .is_primary = (monitor_span[i] == primary) });
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

std::string GLFWSingleton::get_platform()
{
    if (!s_initialized)
        return "";

#if GLFW_VERSION_MAJOR >= 3 && GLFW_VERSION_MINOR >= 4
    int platform = glfwGetPlatform();
    switch (platform) {
    case GLFW_PLATFORM_WAYLAND:
        return "wayland";
    case GLFW_PLATFORM_X11:
        return "x11";
    case GLFW_PLATFORM_WIN32:
        return "win32";
    case GLFW_PLATFORM_COCOA:
        return "cocoa";
    default:
        return "unknown";
    }
#else
    const char* platform = glfwGetPlatformName();
    if (platform)
        return platform;

#ifdef MAYAFLUX_PLATFORM_LINUX
#ifdef GLFW_USE_WAYLAND
    return "wayland";
#else
    return "x11";
#endif
#elif MAYAFLUX_PLATFORM_WINDOWS
    return "win32";
#elif MAYAFLUX_PLATFORM_MACOS
    return "cocoa";
#else
    return "unknown";
#endif
#endif
}

bool GLFWSingleton::is_wayland()
{
    return get_platform() == "wayland";
}

MonitorInfo GLFWSingleton::get_monitor(int32_t id)
{
    auto monitors = enumerate_monitors();
    if (id >= 0 && static_cast<size_t>(id) < monitors.size()) {
        return monitors[id];
    }
    return {};
}

void GLFWSingleton::set_error_callback(std::function<void(int, const char*)> callback)
{
    s_error_callback = std::move(callback);
}

}
