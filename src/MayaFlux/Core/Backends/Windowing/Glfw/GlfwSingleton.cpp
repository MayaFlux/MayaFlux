#include "GlfwSingleton.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

#include "GLFW/glfw3.h"

namespace MayaFlux::Core {

bool GLFWSingleton::s_initialized {};
uint32_t GLFWSingleton::s_window_count {};
std::function<void(int, const char*)> GLFWSingleton::s_error_callback;

bool GLFWSingleton::s_configured {};

GlfwPreInitConfig GLFWSingleton::s_preinit_config {};

void GLFWSingleton::configure(const GlfwPreInitConfig& config)
{
    if (s_initialized) {
        MF_WARN(Journal::Component::Core, Journal::Context::WindowingSubsystem,
            "GLFWSingleton::configure() called after GLFW was initialized — pre-init hints will be ignored");
        return;
    }

    if (config.platform != GlfwPreInitConfig::Platform::Default) {
        int glfw_platform = GLFW_ANY_PLATFORM;
        switch (config.platform) {
        case GlfwPreInitConfig::Platform::Wayland:
            glfw_platform = GLFW_PLATFORM_WAYLAND;
            break;
        case GlfwPreInitConfig::Platform::X11:
            glfw_platform = GLFW_PLATFORM_X11;
            break;
        default:
            break;
        }
        glfwInitHint(GLFW_PLATFORM, glfw_platform);
    }

    glfwInitHint(GLFW_WAYLAND_LIBDECOR, config.disable_libdecor ? GLFW_FALSE : GLFW_TRUE);
    glfwInitHint(GLFW_COCOA_CHDIR_RESOURCES, config.cocoa_chdir_resources ? GLFW_TRUE : GLFW_FALSE);
    glfwInitHint(GLFW_COCOA_MENUBAR, config.cocoa_menubar ? GLFW_TRUE : GLFW_FALSE);

    s_configured = true;

    MF_INFO(Journal::Component::Core, Journal::Context::WindowingSubsystem,
        "GLFW pre-initialization configured: platform={}, libdecor={}, cocoa_chdir_resources={}, cocoa_menubar={}",
        (config.platform == GlfwPreInitConfig::Platform::Default
                ? "default"
                : config.platform == GlfwPreInitConfig::Platform::Wayland
                ? "wayland"
                : "x11"),
        config.disable_libdecor ? "disabled" : "enabled",
        config.cocoa_chdir_resources ? "enabled" : "disabled",
        config.cocoa_menubar ? "enabled" : "disabled");

    s_preinit_config = config;
}

bool GLFWSingleton::initialize()
{
    if (s_initialized)
        return true;

    if (!s_configured) {
        MF_WARN(Journal::Component::Core, Journal::Context::WindowingSubsystem,
            "GLFWSingleton::initialize() called without prior configure() — using default pre-init hints");

        configure(s_preinit_config);
    }

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

std::vector<const char*> GLFWSingleton::get_required_instance_extensions()
{
    if (!initialize()) {
        error<std::runtime_error>(Journal::Component::Core, Journal::Context::WindowingSubsystem,
            std::source_location::current(),
            "GLFW must be initialized before querying required instance extensions");
    }

    uint32_t count = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&count);
    if (!extensions || count == 0) {
        MF_WARN(Journal::Component::Core, Journal::Context::WindowingSubsystem,
            "No required instance extensions reported by GLFW");
        return {};
    }

    return { extensions, extensions + count };
}

}
