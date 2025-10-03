#include "GlfwWindow.hpp"

#include "GlfwSingleton.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

#ifdef MAYAFLUX_PLATFORM_WINDOWS
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#elif MAYAFLUX_PLATFORM_LINUX
#include <GLFW/glfw3native.h>
#elif MAYAFLUX_PLATFORM_MACOS
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>
#endif

namespace MayaFlux::Core {

GlfwWindow::GlfwWindow(const WindowCreateInfo& create_info,
    const GraphicsSurfaceInfo& global_info)
    : m_create_info(create_info)
{
    setup_preinit_hints(global_info);

    if (!GLFWSingleton::initialize()) {
        error<std::runtime_error>(Journal::Component::Core, Journal::Context::WindowingSubsystem, std::source_location::current(),
            "Failed to initialize GLFW for window creation");
    }

    configure_window_hints(global_info);

    GLFWmonitor* monitor = nullptr;
    if (create_info.fullscreen) {
        if (create_info.monitor_id >= 0) {
            int count {};
            std::span<GLFWmonitor*> monitors(glfwGetMonitors(&count), count);
            if (create_info.monitor_id < count) {
                monitor = monitors[create_info.monitor_id];
            }
        } else {
            monitor = glfwGetPrimaryMonitor();
        }
    }

    m_window = glfwCreateWindow(
        create_info.width,
        create_info.height,
        create_info.title.c_str(),
        monitor,
        nullptr);

    if (!m_window) {
        error<std::runtime_error>(Journal::Component::Core, Journal::Context::WindowingSubsystem, std::source_location::current(),
            "Failed to create GLFW window: {}", create_info.title);
    }

    glfwSetWindowUserPointer(m_window, this);
    setup_callbacks();

    int w {}, h {};
    glfwGetWindowSize(m_window, &w, &h);
    m_state.current_width = w;
    m_state.current_height = h;
    m_state.is_visible = false;

    GLFWSingleton::mark_window_created();

    MF_INFO(Journal::Component::Core, Journal::Context::WindowingSubsystem,
        "Created window '{}' ({}x{})", create_info.title, w, h);
}

GlfwWindow::~GlfwWindow()
{
    if (m_window) {
        glfwDestroyWindow(m_window);
        GLFWSingleton::mark_window_destroyed();
        GLFWSingleton::terminate();
    }
}

GlfwWindow::GlfwWindow(GlfwWindow&& other) noexcept
    : m_window(other.m_window)
    , m_create_info(std::move(other.m_create_info))
    , m_state(other.m_state)
    , m_input_config(other.m_input_config)
    , m_event_callback(std::move(other.m_event_callback))
{
    other.m_window = nullptr;
    if (m_window) {
        glfwSetWindowUserPointer(m_window, this);
    }
}

GlfwWindow& GlfwWindow::operator=(GlfwWindow&& other) noexcept
{
    if (this != &other) {
        if (m_window) {
            glfwDestroyWindow(m_window);
            GLFWSingleton::mark_window_destroyed();
        }

        m_window = other.m_window;
        m_create_info = std::move(other.m_create_info);
        m_state = other.m_state;
        m_input_config = other.m_input_config;
        m_event_callback = std::move(other.m_event_callback);

        other.m_window = nullptr;
        if (m_window) {
            glfwSetWindowUserPointer(m_window, this);
        }
    }
    return *this;
}

void GlfwWindow::set_title(const std::string& title)
{
    if (m_window) {
        glfwSetWindowTitle(m_window, title.c_str());
        m_create_info.title = title;
    }
}

void GlfwWindow::setup_preinit_hints(const GraphicsSurfaceInfo& global_info)
{
#ifdef MAYAFLUX_PLATFORM_LINUX
    int desired_platform = GLFW_ANY_PLATFORM;

    if (global_info.linux_force_wayland) {
        if (glfwPlatformSupported(GLFW_PLATFORM_WAYLAND)) {
            desired_platform = GLFW_PLATFORM_WAYLAND;
        } else {
            MF_WARN(Journal::Component::Core, Journal::Context::WindowingSubsystem,
                "Wayland requested but not supported by GLFW, falling back to X11");
        }
    } else {
        if (glfwPlatformSupported(GLFW_PLATFORM_X11)) {
            desired_platform = GLFW_PLATFORM_X11;
        }
    }

    if (desired_platform != GLFW_ANY_PLATFORM) {
        glfwInitHint(GLFW_PLATFORM, desired_platform);
    }
#endif
}

void GlfwWindow::configure_window_hints(const GraphicsSurfaceInfo& global_info) const
{
    glfwDefaultWindowHints();

    glfwWindowHint(GLFW_RESIZABLE, m_create_info.resizable ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_DECORATED, m_create_info.decorated ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_FLOATING, m_create_info.floating ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, m_create_info.transparent ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    if (global_info.requested_api == GraphicsSurfaceInfo::VisualApi::VULKAN) {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    } else if (global_info.requested_api == GraphicsSurfaceInfo::VisualApi::OPENGL) {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    }
}

void GlfwWindow::setup_callbacks()
{
    glfwSetWindowSizeCallback(m_window, glfw_window_size_callback);
    glfwSetWindowCloseCallback(m_window, glfw_window_close_callback);
    glfwSetWindowFocusCallback(m_window, glfw_window_focus_callback);
    glfwSetFramebufferSizeCallback(m_window, glfw_framebuffer_size_callback);
    glfwSetKeyCallback(m_window, glfw_key_callback);
    glfwSetCursorPosCallback(m_window, glfw_cursor_pos_callback);
    glfwSetMouseButtonCallback(m_window, glfw_mouse_button_callback);
    glfwSetScrollCallback(m_window, glfw_scroll_callback);
}

void GlfwWindow::show()
{
    if (m_window) {
        glfwShowWindow(m_window);
        m_state.is_visible = true;
    }
}

void GlfwWindow::hide()
{
    if (m_window) {
        glfwHideWindow(m_window);
        m_state.is_visible = false;
    }
}

bool GlfwWindow::should_close() const
{
    return m_window ? glfwWindowShouldClose(m_window) : true;
}

void GlfwWindow::set_event_callback(WindowEventCallback callback)
{
    m_event_callback = std::move(callback);
}

void* GlfwWindow::get_native_handle() const
{
    if (!m_window)
        return nullptr;

#ifdef MAYAFLUX_PLATFORM_WINDOWS
    return glfwGetWin32Window(m_window);
#elif MAYAFLUX_PLATFORM_LINUX
#if defined(GLFW_EXPOSE_NATIVE_WAYLAND)
    if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND) {
        return glfwGetWaylandWindow(m_window);
    }
#endif
#if defined(GLFW_EXPOSE_NATIVE_X11)
    if (glfwGetPlatform() == GLFW_PLATFORM_X11) {
        return reinterpret_cast<void*>(glfwGetX11Window(m_window));
    }
#endif
    return nullptr;
#elif MAYAFLUX_PLATFORM_MACOS
    return glfwGetCocoaWindow(m_window);
#else
    return m_window;
#endif
}

void* GlfwWindow::get_native_display() const
{
#ifdef MAYAFLUX_PLATFORM_LINUX
#if defined(GLFW_EXPOSE_NATIVE_WAYLAND)
    if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND) {
        return glfwGetWaylandDisplay();
    }
#endif
#if defined(GLFW_EXPOSE_NATIVE_X11)
    if (glfwGetPlatform() == GLFW_PLATFORM_X11) {
        return glfwGetX11Display();
    }
#endif
#endif
    return nullptr;
}

void GlfwWindow::glfw_window_size_callback(GLFWwindow* window, int width, int height)
{
    auto* win = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));
    if (win && win->m_event_callback) {
        win->m_state.current_width = width;
        win->m_state.current_height = height;

        if (win->m_event_callback) {
            WindowEvent event;
            event.type = WindowEventType::WINDOW_RESIZED;
            event.timestamp = glfwGetTime();
            event.data = WindowEvent::ResizeData {
                .width = static_cast<u_int32_t>(width),
                .height = static_cast<u_int32_t>(height)
            };
            win->m_event_callback(event);
        }
    }
}

void GlfwWindow::set_size(u_int32_t width, u_int32_t height)
{
    if (m_window) {
        glfwSetWindowSize(m_window, static_cast<int>(width), static_cast<int>(height));
    }

    m_create_info.width = width;
    m_create_info.height = height;
}

void GlfwWindow::set_position(u_int32_t x, u_int32_t y)
{
    if (m_window) {
        glfwSetWindowPos(m_window, static_cast<int>(x), static_cast<int>(y));
    }
}

void GlfwWindow::glfw_window_close_callback(GLFWwindow* window)
{
    auto* win = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));
    if (!win)
        return;

    WindowEvent event;
    event.type = WindowEventType::WINDOW_CLOSED;
    event.timestamp = glfwGetTime();

    win->m_event_source.signal(event);

    if (win->m_event_callback) {
        win->m_event_callback(event);
    }
}

void GlfwWindow::glfw_window_focus_callback(GLFWwindow* window, int focused)
{
    auto* win = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));
    if (!win)
        return;

    win->m_state.is_focused = (focused == GLFW_TRUE);

    WindowEvent event;
    event.type = focused ? WindowEventType::WINDOW_FOCUS_GAINED
                         : WindowEventType::WINDOW_FOCUS_LOST;
    event.timestamp = glfwGetTime();

    win->m_event_source.signal(event);

    if (win->m_event_callback) {
        win->m_event_callback(event);
    }
}

void GlfwWindow::glfw_framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    auto* win = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));
    if (!win)
        return;

    WindowEvent event;
    event.type = WindowEventType::FRAMEBUFFER_RESIZED;
    event.timestamp = glfwGetTime();
    event.data = WindowEvent::ResizeData {
        .width = static_cast<u_int32_t>(width),
        .height = static_cast<u_int32_t>(height)
    };

    win->m_event_source.signal(event);

    if (win->m_event_callback) {
        win->m_event_callback(event);
    }
}

void GlfwWindow::glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    auto* win = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));
    if (!win)
        return;

    WindowEventType type;
    switch (action) {
    case GLFW_PRESS:
        type = WindowEventType::KEY_PRESSED;
        break;
    case GLFW_RELEASE:
        type = WindowEventType::KEY_RELEASED;
        break;
    case GLFW_REPEAT:
        type = WindowEventType::KEY_REPEAT;
        break;
    default:
        return;
    }

    WindowEvent event;
    event.type = type;
    event.timestamp = glfwGetTime();
    event.data = WindowEvent::KeyData {
        .key = key,
        .scancode = scancode,
        .mods = mods
    };

    win->m_event_source.signal(event);

    if (win->m_event_callback) {
        win->m_event_callback(event);
    }
}

void GlfwWindow::glfw_cursor_pos_callback(GLFWwindow* window, double xpos, double ypos)
{
    auto* win = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));
    if (!win)
        return;

    WindowEvent event;
    event.type = WindowEventType::MOUSE_MOVED;
    event.timestamp = glfwGetTime();

    event.data = WindowEvent::MousePosData {
        .x = xpos,
        .y = ypos
    };

    win->m_event_source.signal(event);

    if (win->m_event_callback) {
        win->m_event_callback(event);
    }
}

void GlfwWindow::glfw_mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    auto* win = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));
    if (!win)
        return;

    WindowEvent event;
    event.type = (action == GLFW_PRESS) ? WindowEventType::MOUSE_BUTTON_PRESSED
                                        : WindowEventType::MOUSE_BUTTON_RELEASED;
    event.timestamp = glfwGetTime();

    event.data = WindowEvent::MouseButtonData {
        .button = button,
        .mods = mods
    };

    win->m_event_source.signal(event);

    if (win->m_event_callback) {
        win->m_event_callback(event);
    }
}

void GlfwWindow::glfw_scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    auto* win = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));
    if (!win)
        return;

    WindowEvent event;
    event.type = WindowEventType::MOUSE_SCROLLED;
    event.timestamp = glfwGetTime();
    event.data = WindowEvent::ScrollData {
        .x_offset = xoffset,
        .y_offset = yoffset
    };

    win->m_event_source.signal(event);

    if (win->m_event_callback) {
        win->m_event_callback(event);
    }
}

}
