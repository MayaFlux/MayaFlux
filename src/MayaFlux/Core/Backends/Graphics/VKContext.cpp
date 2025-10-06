#include "VKContext.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

#include "MayaFlux/Core/Backends/Windowing/Glfw/GlfwSingleton.hpp"
#include "MayaFlux/Core/Backends/Windowing/Glfw/GlfwWindow.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace MayaFlux::Core {

bool VKContext::initialize(const GlobalGraphicsConfig& graphics_config, bool enable_validation,
    const std::vector<const char*>& required_extensions)
{
    m_graphics_config = graphics_config;

    if (graphics_config.requested_api != GlobalGraphicsConfig::GraphicsApi::VULKAN) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Vulkan context initialization requested, but graphics API is not set to Vulkan!");
        return false;
    }

    std::vector<const char*> extensions;
    if (graphics_config.windowing_backend == GlobalGraphicsConfig::WindowingBackend::GLFW) {
        extensions = GLFWSingleton::get_required_instance_extensions();

        for (const char* ext : required_extensions) {
            if (std::ranges::find(extensions, ext) == extensions.end()) {
                extensions.push_back(ext);
            }
        }
    } else {
        extensions = required_extensions;
    }

    if (!m_instance.initialize(enable_validation, extensions)) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend, "Failed to initialize Vulkan instance!");
        return false;
    }

    if (!m_device.initialize(m_instance.get_instance(), m_graphics_config.backend_info)) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend, "Failed to initialize Vulkan device!");
        cleanup();
        return false;
    }

    MF_INFO(Journal::Component::Core, Journal::Context::GraphicsBackend, "Vulkan context initialized successfully.");
    return true;
}

vk::SurfaceKHR VKContext::create_surface(std::shared_ptr<Window> window)
{
    if (!window) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot create surface: null window");
        return nullptr;
    }

    auto* glfw_window = dynamic_cast<GlfwWindow*>(window.get());
    if (!glfw_window) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot create surface: window is not a GlfwWindow");
        return nullptr;
    }

    GLFWwindow* glfw_handle = glfw_window->get_glfw_handle();
    if (!glfw_handle) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot create surface: null GLFW handle");
        return nullptr;
    }

    VkSurfaceKHR c_surface;
    VkResult result = glfwCreateWindowSurface(
        static_cast<VkInstance>(m_instance.get_instance()),
        glfw_handle,
        nullptr,
        &c_surface);

    if (result != VK_SUCCESS) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to create window surface");
        return nullptr;
    }

    vk::SurfaceKHR surface(c_surface);
    m_surfaces.push_back(surface);

    MF_INFO(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Surface created for window '{}'", window->get_create_info().title);

    return surface;
}

void VKContext::destroy_surface(vk::SurfaceKHR surface)
{
    if (!surface)
        return;

    auto it = std::ranges::find(m_surfaces, surface);
    if (it != m_surfaces.end()) {
        m_instance.get_instance().destroySurfaceKHR(*it);
        m_surfaces.erase(it);
    }
}

void VKContext::cleanup()
{
    for (auto surface : m_surfaces) {
        if (surface) {
            m_instance.get_instance().destroySurfaceKHR(surface);
        }
    }
    m_surfaces.clear();

    m_device.cleanup();
    m_instance.cleanup();
}

}
