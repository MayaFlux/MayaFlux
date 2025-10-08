#pragma once

#include "MayaFlux/Core/GlobalGraphicsInfo.hpp"
#include "VKDevice.hpp"

namespace MayaFlux::Core {

class Window;

/**
 * @class VKContext
 * @brief High-level wrapper for Vulkan instance and device
 *
 * Manages the complete Vulkan context lifecycle.
 */
class VKContext {
public:
    VKContext() = default;
    ~VKContext() = default;

    /**
     * @brief Initialize Vulkan context
     * @param global_config Global graphics configuration
     * @param enable_validation Enable validation layers
     * @param required_extensions Required instance extensions
     * @return true if successful
     */
    bool initialize(const GlobalGraphicsConfig& graphics_config, bool enable_validation = true,
        const std::vector<const char*>& required_extensions = {});

    /**
     * @brief Cleanup all Vulkan resources
     */
    void cleanup();

    /**
     * @brief Get Vulkan instance
     */
    [[nodiscard]] vk::Instance get_instance() const { return m_instance.get_instance(); }

    /**
     * @brief Get physical device
     */
    [[nodiscard]] vk::PhysicalDevice get_physical_device() const { return m_device.get_physical_device(); }

    /**
     * @brief Get logical device
     */
    [[nodiscard]] vk::Device get_device() const { return m_device.get_device(); }

    /**
     * @brief Get graphics queue
     */
    [[nodiscard]] vk::Queue get_graphics_queue() const { return m_device.get_graphics_queue(); }

    /**
     * @brief Get compute queue
     */
    [[nodiscard]] vk::Queue get_compute_queue() const { return m_device.get_compute_queue(); }

    /**
     * @brief Get transfer queue
     */
    [[nodiscard]] vk::Queue get_transfer_queue() const { return m_device.get_transfer_queue(); }

    /**
     * @brief Get queue family indices
     */
    [[nodiscard]] const QueueFamilyIndices& get_queue_families() const
    {
        return m_device.get_queue_families();
    }

    /**
     * @brief Update presentation support for a surface
     * @param surface Surface to check
     * @return true if presentation is supported
     */
    bool update_present_family(vk::SurfaceKHR surface);

    /**
     * @brief Create surface from window's native handles
     * @param window Window to create surface for
     * @return Surface handle, or null on failure
     */
    vk::SurfaceKHR create_surface(std::shared_ptr<Window> window);

    /**
     * @brief Destroy a specific surface
     * Called when window is unregistered
     */
    void destroy_surface(vk::SurfaceKHR surface);

    /**
     * @brief Wait for device idle
     */
    void wait_idle() const { m_device.wait_idle(); }

    /**
     * @brief Get graphics surface info
     */
    [[nodiscard]] const GraphicsSurfaceInfo& get_surface_info() const { return m_graphics_config.surface_info; }

private:
    VKInstance m_instance;
    VKDevice m_device;

    GlobalGraphicsConfig m_graphics_config;

    std::vector<vk::SurfaceKHR> m_surfaces;
};

}
