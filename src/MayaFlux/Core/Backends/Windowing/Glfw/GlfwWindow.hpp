#pragma once

#include "MayaFlux/Core/Backends/Windowing/Window.hpp"
#include "MayaFlux/Vruta/EventSource.hpp"

#include <GLFW/glfw3.h>

namespace MayaFlux::Core {
/**
 * @class GlfwWindow
 * @brief Platform-agnostic window wrapper
 *
 * Wraps a GLFW window  and provides a unified interface
 * for window management, event handling, and state tracking.
 */
class MAYAFLUX_API GlfwWindow : public Window {
public:
    /**
     * @brief Creates a window with the given configuration
     * @param create_info Window creation parameters
     * @param surface_info Graphics surface parameters
     * @param api Requested graphics API
     * @param pre_init_config Optional pre-initialization configuration
     */
    GlfwWindow(const WindowCreateInfo& create_info,
        const GraphicsSurfaceInfo& surface_info, GlobalGraphicsConfig::GraphicsApi api, GlfwPreInitConfig pre_init_config = {});

    ~GlfwWindow() override;

    GlfwWindow(const GlfwWindow&) = delete;
    GlfwWindow& operator=(const GlfwWindow&) = delete;
    GlfwWindow(GlfwWindow&&) noexcept;
    GlfwWindow& operator=(GlfwWindow&&) noexcept;

    void show() override;

    void hide() override;

    [[nodiscard]] bool should_close() const override;

    [[nodiscard]] inline const WindowState& get_state() const override
    {
        return m_state;
    }

    [[nodiscard]] inline const WindowCreateInfo& get_create_info() const override
    {
        return m_create_info;
    }

    inline void set_input_config(const InputConfig& config) override
    {
        m_input_config = config;
    }

    [[nodiscard]] inline const InputConfig& get_input_config() const override
    {
        return m_input_config;
    }

    void set_event_callback(WindowEventCallback callback) override;

    [[nodiscard]] void* get_native_handle() const override;

    [[nodiscard]] void* get_native_display() const override;

    [[nodiscard]] inline GLFWwindow* get_glfw_handle() const { return m_window; }

    void set_title(const std::string& title) override;

    void set_size(uint32_t width, uint32_t height) override;

    void set_position(uint32_t x, uint32_t y) override;

    Vruta::EventSource& get_event_source() override { return m_event_source; }
    [[nodiscard]] const Vruta::EventSource& get_event_source() const override { return m_event_source; }

    /**
     * @brief Check if window is registered with graphics subsystem
     */
    [[nodiscard]] bool is_graphics_registered() const override { return m_graphics_registered; }

    /**
     * @brief Mark window as registered/unregistered with graphics
     * Called by GraphicsSubsystem during register/unregister
     */
    void set_graphics_registered(bool registered) override { m_graphics_registered = registered; }

private:
    GLFWwindow* m_window = nullptr;
    WindowCreateInfo m_create_info;
    WindowState m_state;
    InputConfig m_input_config;
    WindowEventCallback m_event_callback;

    bool m_graphics_registered {};

    Vruta::EventSource m_event_source;

    void configure_window_hints(const GraphicsSurfaceInfo& surface_info, GlobalGraphicsConfig::GraphicsApi api) const;
    void setup_callbacks();

    static void glfw_window_size_callback(GLFWwindow* window, int width, int height);
    static void glfw_window_close_callback(GLFWwindow* window);
    static void glfw_window_focus_callback(GLFWwindow* window, int focused);
    static void glfw_framebuffer_size_callback(GLFWwindow* window, int width, int height);
    static void glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void glfw_cursor_pos_callback(GLFWwindow* window, double xpos, double ypos);
    static void glfw_mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
    static void glfw_scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
};
}
