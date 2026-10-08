#include "VKContext.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

#include "MayaFlux/Core/Backends/Windowing/Cocoa/CocoaWindow.hpp"
#include "MayaFlux/Core/Backends/Windowing/Wayland/WaylandWindow.hpp"
#include "MayaFlux/Core/Backends/Windowing/Win32/Win32Window.hpp"

#if defined(COCOA_BACKEND)
#include <vulkan/vulkan_metal.h>
#endif

#if defined(WIN32_BACKEND)
#include <vulkan/vulkan_win32.h>
#endif

#if defined(WAYLAND_BACKEND)
#include <vulkan/vulkan_wayland.h>
#endif

#include "MayaFlux/Transitive/Parallel/Dispatch.hpp"

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

    std::vector<const char*> extensions = required_extensions;

#if defined(COCOA_BACKEND)
    if (graphics_config.windowing_backend == GlobalGraphicsConfig::WindowingBackend::COCOA) {
        for (const char* ext : { VK_KHR_SURFACE_EXTENSION_NAME, VK_EXT_METAL_SURFACE_EXTENSION_NAME }) {
            if (std::ranges::none_of(extensions, [ext](const char* e) { return std::strcmp(e, ext) == 0; }))
                extensions.push_back(ext);
        }
    }
#endif

#if defined(WIN32_BACKEND)
    extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(WAYLAND_BACKEND)
    extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    extensions.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#endif

    if (!m_instance.initialize(enable_validation, extensions)) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to initialize Vulkan instance!");
        return false;
    }

    if (!m_device.initialize(m_instance.get_instance(), m_graphics_config.backend_info)) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to initialize Vulkan device!");
        cleanup();
        return false;
    }

    MF_INFO(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Vulkan context initialized successfully.");
    return true;
}

vk::SurfaceKHR VKContext::create_surface(std::shared_ptr<Window> window)
{
    if (!window) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot create surface: null window");
        return nullptr;
    }

#if defined(COCOA_BACKEND)
    if (m_graphics_config.windowing_backend == GlobalGraphicsConfig::WindowingBackend::COCOA) {
        auto* cocoa_window = dynamic_cast<CocoaWindow*>(window.get());
        if (!cocoa_window) {
            MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
                "Cannot create surface: window is not a CocoaWindow");
            return nullptr;
        }

        const void* layer = cocoa_window->get_metal_layer();
        if (!layer) {
            MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
                "Cannot create surface: null CAMetalLayer");
            return nullptr;
        }

        auto instance = static_cast<VkInstance>(m_instance.get_instance());
        auto create_metal_surface = reinterpret_cast<PFN_vkCreateMetalSurfaceEXT>(
            vkGetInstanceProcAddr(instance, "vkCreateMetalSurfaceEXT"));
        if (!create_metal_surface) {
            MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
                "Cannot create surface: vkCreateMetalSurfaceEXT unavailable");
            return nullptr;
        }

        VkMetalSurfaceCreateInfoEXT info {};
        info.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
        info.pLayer = static_cast<const CAMetalLayer*>(layer);

        VkSurfaceKHR c_surface {};
        VkResult result = Parallel::dispatch_main_sync([&]() {
            return create_metal_surface(instance, &info, nullptr, &c_surface);
        });

        if (result != VK_SUCCESS) {
            MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
                "Failed to create Metal surface for window '{}'",
                window->get_create_info().title);
            return nullptr;
        }

        vk::SurfaceKHR surface(c_surface);
        m_surfaces.push_back(surface);

        MF_INFO(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Metal surface created for window '{}'", window->get_create_info().title);

        return surface;
    }
#endif

#if defined(WIN32_BACKEND)
    if (m_graphics_config.windowing_backend == GlobalGraphicsConfig::WindowingBackend::WINDOWS) {
        auto hwnd = static_cast<HWND>(window->get_native_handle());
        if (!hwnd) {
            MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
                "Cannot create surface: null HWND");
            return nullptr;
        }

        VkWin32SurfaceCreateInfoKHR info {};
        info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        info.hinstance = GetModuleHandleW(nullptr);
        info.hwnd = hwnd;

        VkSurfaceKHR c_surface {};
        if (vkCreateWin32SurfaceKHR(
                static_cast<VkInstance>(m_instance.get_instance()),
                &info, nullptr, &c_surface)
            != VK_SUCCESS) {
            MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
                "Failed to create Win32 Vulkan surface for window '{}'",
                window->get_create_info().title);
            return nullptr;
        }

        vk::SurfaceKHR surface(c_surface);
        m_surfaces.push_back(surface);
        MF_INFO(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Win32 surface created for window '{}'", window->get_create_info().title);
        return surface;
    }
#elif defined(WAYLAND_BACKEND)
    {
        auto* wl_display = static_cast<struct wl_display*>(window->get_native_display());
        auto* wl_surface = static_cast<struct wl_surface*>(window->get_native_handle());
        if (!wl_display || !wl_surface) {
            MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
                "Cannot create surface: null Wayland display or surface handle");
            return nullptr;
        }

        VkWaylandSurfaceCreateInfoKHR info {};
        info.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
        info.display = wl_display;
        info.surface = wl_surface;

        VkSurfaceKHR c_surface {};
        if (vkCreateWaylandSurfaceKHR(
                static_cast<VkInstance>(m_instance.get_instance()),
                &info, nullptr, &c_surface)
            != VK_SUCCESS) {
            MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
                "Failed to create Wayland Vulkan surface for window '{}'",
                window->get_create_info().title);
            return nullptr;
        }

        vk::SurfaceKHR surface(c_surface);
        m_surfaces.push_back(surface);
        MF_INFO(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Wayland surface created for window '{}'", window->get_create_info().title);
        return surface;
    }
#endif

    MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "No windowing backend available for surface creation");
    return nullptr;
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
