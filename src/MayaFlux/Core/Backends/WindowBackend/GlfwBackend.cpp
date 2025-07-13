#include "GlfwBackend.hpp"

#include "MayaFlux/Utils.hpp"

#ifdef GLFW_BACKEND
#include "GlfwSingleton.hpp"
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#elif defined(__APPLE__)
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>
#endif
#endif

namespace MayaFlux::Core {

std::unique_ptr<IWindowBackend> WindowingBackendFactory::create_backend(Utils::WindowingBackendType type)
{
    switch (type) {
    case Utils::WindowingBackendType::GLFW:
        return std::make_unique<GLFWBackend>();
    default:
        throw std::runtime_error("Unsupported windowing backend type");
    }
}

static MonitorInfo convert_monitor_info(GLFWmonitor* monitor, bool is_primary)
{
    MonitorInfo info;
    info.name = glfwGetMonitorName(monitor);
    info.is_primary = is_primary;

    glfwGetMonitorPhysicalSize(monitor, &info.width_mm, &info.height_mm);

    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    if (mode) {
        info.refresh_rate = mode->refreshRate;
    }

    int count;
    const GLFWvidmode* modes = glfwGetVideoModes(monitor, &count);
    for (int i = 0; i < count; ++i) {
        MonitorInfo::VideoMode vm;
        vm.width = modes[i].width;
        vm.height = modes[i].height;
        vm.red_bits = modes[i].redBits;
        vm.green_bits = modes[i].greenBits;
        vm.blue_bits = modes[i].blueBits;
        vm.refresh_rate = modes[i].refreshRate;
        info.video_modes.push_back(vm);

        bool found = false;
        for (int rate : info.supported_refresh_rates) {
            if (rate == vm.refresh_rate) {
                found = true;
                break;
            }
        }
        if (!found) {
            info.supported_refresh_rates.push_back(vm.refresh_rate);
        }
    }

    return info;
}

GLFWDevice::GLFWDevice()
    : m_primary_monitor {}
{
    if (!GLFWSingleton::initialize()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }
    enumerate_monitors();
}

GLFWDevice::~GLFWDevice()
{
    GLFWSingleton::terminate();
}

void GLFWDevice::enumerate_monitors()
{
    int count;
    GLFWmonitor** monitors = glfwGetMonitors(&count);
    GLFWmonitor* primary_monitor = glfwGetPrimaryMonitor();

    m_monitors.clear();
    m_primary_monitor = 0;

    for (size_t i = 0; i < count; i++) {
        bool is_primary = (monitors[i] == primary_monitor);
        if (is_primary) {
            m_primary_monitor = i;
        }
        m_monitors.push_back(convert_monitor_info(monitors[i], is_primary));
    }
}

MonitorInfo GLFWDevice::get_monitor_info(u_int32_t monitor_id) const
{
    if (monitor_id >= 0 && monitor_id < static_cast<int>(m_monitors.size())) {
        return m_monitors[monitor_id];
    }
    return m_monitors[m_primary_monitor];
}

GLFWContext::GLFWContext(GLFWmonitor* monitor, const GlobalWindowInfo& window_info)
    : m_window(nullptr)
    , m_monitor(monitor)
    , m_window_info(window_info)
    , m_is_created {}
{
    if (!GLFWSingleton::initialize()) {
        throw std::runtime_error("Failed to initialize GLFW for context");
    }
}

GLFWContext::~GLFWContext()
{
    if (is_created()) {
        destroy();
    }
}

void GLFWContext::configure_window_hints()
{
    glfwDefaultWindowHints();

    glfwWindowHint(GLFW_RESIZABLE, m_window_info.output.resizable ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_DECORATED, m_window_info.output.decorated ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_FLOATING, m_window_info.output.floating ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    auto context_version_major = m_window_info.backend_options.find("glfw.context_version_major");
    if (context_version_major != m_window_info.backend_options.end()) {
        try {
            int version = std::any_cast<int>(context_version_major->second);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, version);
        } catch (const std::bad_any_cast&) {
        }
    }

    auto context_version_minor = m_window_info.backend_options.find("glfw.context_version_minor");
    if (context_version_minor != m_window_info.backend_options.end()) {
        try {
            int version = std::any_cast<int>(context_version_minor->second);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, version);
        } catch (const std::bad_any_cast&) {
        }
    }

    auto client_api = m_window_info.backend_options.find("glfw.client_api");
    if (client_api != m_window_info.backend_options.end()) {
        try {
            std::string api = std::any_cast<std::string>(client_api->second);
            if (api == "vulkan") {
                glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            } else if (api == "opengl") {
                glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
            }
        } catch (const std::bad_any_cast&) {
        }
    }
}

void GLFWContext::create()
{
    if (is_created()) {
        return;
    }

    configure_window_hints();

    GLFWmonitor* monitor_for_fullscreen = nullptr;
    if (m_window_info.output.fullscreen && m_monitor) {
        monitor_for_fullscreen = m_monitor;
    }

    m_window = glfwCreateWindow(
        m_window_info.output.width,
        m_window_info.output.height,
        m_window_info.title.c_str(),
        monitor_for_fullscreen,
        nullptr);

    if (!m_window) {
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwSetWindowUserPointer(m_window, this);

    glfwSetWindowSizeCallback(m_window, window_size_callback);
    glfwSetWindowCloseCallback(m_window, window_close_callback);
    glfwSetWindowFocusCallback(m_window, window_focus_callback);
    glfwSetKeyCallback(m_window, key_callback);
    glfwSetCursorPosCallback(m_window, cursor_pos_callback);
    glfwSetMouseButtonCallback(m_window, mouse_button_callback);
    glfwSetScrollCallback(m_window, scroll_callback);

    // Configure input based on window_info
    if (!m_window_info.input.cursor_enabled) {
        glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
    if (m_window_info.input.sticky_keys) {
        glfwSetInputMode(m_window, GLFW_STICKY_KEYS, GLFW_TRUE);
    }
    if (m_window_info.input.sticky_mouse_buttons) {
        glfwSetInputMode(m_window, GLFW_STICKY_MOUSE_BUTTONS, GLFW_TRUE);
    }
    if (m_window_info.input.raw_mouse_motion) {
        if (glfwRawMouseMotionSupported()) {
            glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        }
    }

    GLFWSingleton::mark_window_created();
    m_is_created = true;
}

void GLFWContext::show()
{
    if (is_created()) {
        glfwShowWindow(m_window);
    }
}

void GLFWContext::hide()
{
    if (is_created()) {
        glfwHideWindow(m_window);
    }
}

void GLFWContext::destroy()
{
    if (!is_created()) {
        return;
    }

    glfwDestroyWindow(m_window);
    m_window = nullptr;
    m_is_created = false;

    GLFWSingleton::mark_window_destroyed();
    GLFWSingleton::terminate();
}

bool GLFWContext::is_created() const
{
    return m_is_created && m_window != nullptr;
}

bool GLFWContext::is_visible() const
{
    if (!is_created())
        return false;
    return glfwGetWindowAttrib(m_window, GLFW_VISIBLE) == GLFW_TRUE;
}

bool GLFWContext::should_close() const
{
    if (!is_created())
        return true;
    return glfwWindowShouldClose(m_window);
}

void GLFWContext::poll_events()
{
    glfwPollEvents();
}

void GLFWContext::wait_events()
{
    glfwWaitEvents();
}

void GLFWContext::set_event_callback(WindowEventCallback callback)
{
    m_event_callback = callback;
}

void* GLFWContext::get_native_handle() const
{
    if (!is_created())
        return nullptr;

#ifdef _WIN32
    return glfwGetWin32Window(m_window);
#elif defined(__linux__)
    return reinterpret_cast<void*>(glfwGetX11Window(m_window));
#elif defined(__APPLE__)
    return glfwGetCocoaWindow(m_window);
#else
    return m_window; // Fallback to GLFW handle
#endif
}

void* GLFWContext::get_native_display() const
{
#ifdef __linux__
    return glfwGetX11Display();
#else
    return nullptr;
#endif
}

void GLFWContext::set_size(u_int32_t width, u_int32_t height)
{
    if (is_created()) {
        glfwSetWindowSize(m_window, width, height);
    }
}

void GLFWContext::set_position(u_int32_t x, u_int32_t y)
{
    if (is_created()) {
        glfwSetWindowPos(m_window, x, y);
    }
}

void GLFWContext::set_title(const std::string& title)
{
    if (is_created()) {
        glfwSetWindowTitle(m_window, title.c_str());
    }
}

std::pair<u_int32_t, u_int32_t> GLFWContext::get_size() const
{
    if (!is_created())
        return { 0, 0 };

    int width, height;
    glfwGetWindowSize(m_window, &width, &height);
    return { width, height };
}

std::pair<u_int32_t, u_int32_t> GLFWContext::get_position() const
{
    if (!is_created())
        return { 0, 0 };

    int x, y;
    glfwGetWindowPos(m_window, &x, &y);
    return { x, y };
}

void GLFWContext::window_size_callback(GLFWwindow* window, int width, int height)
{
    GLFWContext* context = static_cast<GLFWContext*>(glfwGetWindowUserPointer(window));
    if (context && context->m_event_callback) {
        WindowEvent event;
        event.type = WindowEvent::WINDOW_RESIZE;
        event.timestamp = glfwGetTime();
        event.data["width"] = width;
        event.data["height"] = height;
        context->m_event_callback(event);
    }
}

void GLFWContext::window_close_callback(GLFWwindow* window)
{
    GLFWContext* context = static_cast<GLFWContext*>(glfwGetWindowUserPointer(window));
    if (context && context->m_event_callback) {
        WindowEvent event;
        event.type = WindowEvent::WINDOW_CLOSE;
        event.timestamp = glfwGetTime();
        context->m_event_callback(event);
    }
}

void GLFWContext::window_focus_callback(GLFWwindow* window, int focused)
{
    GLFWContext* context = static_cast<GLFWContext*>(glfwGetWindowUserPointer(window));
    if (context && context->m_event_callback) {
        WindowEvent event;
        event.type = WindowEvent::WINDOW_FOCUS;
        event.timestamp = glfwGetTime();
        event.data["focused"] = (focused == GLFW_TRUE);
        context->m_event_callback(event);
    }
}

void GLFWContext::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    GLFWContext* context = static_cast<GLFWContext*>(glfwGetWindowUserPointer(window));
    if (context && context->m_event_callback) {
        WindowEvent event;
        event.type = (action == GLFW_PRESS) ? WindowEvent::KEY_PRESS : WindowEvent::KEY_RELEASE;
        event.timestamp = glfwGetTime();
        event.data["key"] = key;
        event.data["scancode"] = scancode;
        event.data["action"] = action;
        event.data["mods"] = mods;
        context->m_event_callback(event);
    }
}

void GLFWContext::cursor_pos_callback(GLFWwindow* window, double xpos, double ypos)
{
    GLFWContext* context = static_cast<GLFWContext*>(glfwGetWindowUserPointer(window));
    if (context && context->m_event_callback) {
        WindowEvent event;
        event.type = WindowEvent::MOUSE_MOVE;
        event.timestamp = glfwGetTime();
        event.data["x"] = xpos;
        event.data["y"] = ypos;
        context->m_event_callback(event);
    }
}

void GLFWContext::mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    GLFWContext* context = static_cast<GLFWContext*>(glfwGetWindowUserPointer(window));
    if (context && context->m_event_callback) {
        WindowEvent event;
        event.type = WindowEvent::MOUSE_BUTTON;
        event.timestamp = glfwGetTime();
        event.data["button"] = button;
        event.data["action"] = action;
        event.data["mods"] = mods;
        context->m_event_callback(event);
    }
}

void GLFWContext::scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    GLFWContext* context = static_cast<GLFWContext*>(glfwGetWindowUserPointer(window));
    if (context && context->m_event_callback) {
        WindowEvent event;
        event.type = WindowEvent::MOUSE_SCROLL;
        event.timestamp = glfwGetTime();
        event.data["x_offset"] = xoffset;
        event.data["y_offset"] = yoffset;
        context->m_event_callback(event);
    }
}

GLFWBackend::GLFWBackend()
{
    if (!GLFWSingleton::initialize()) {
        throw std::runtime_error("Failed to initialize GLFW backend");
    }
    m_initialized = true;
}

GLFWBackend::~GLFWBackend()
{
    if (m_initialized) {
        GLFWSingleton::terminate();
    }
}

std::unique_ptr<IWindowDevice> GLFWBackend::create_device_manager()
{
    return std::make_unique<GLFWDevice>();
}

std::unique_ptr<IWindowContext> GLFWBackend::create_window_context(
    u_int32_t monitor_id,
    const GlobalWindowInfo& window_info)
{
    GLFWmonitor* monitor = nullptr;
    if (monitor_id >= 0) {
        int count;
        GLFWmonitor** monitors = glfwGetMonitors(&count);
        if (monitor_id < count) {
            monitor = monitors[monitor_id];
        }
    }

    return std::make_unique<GLFWContext>(monitor, window_info);
}

}
