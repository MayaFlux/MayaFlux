#pragma once

#include "VKDevice.hpp"

namespace MayaFlux::Core {

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
     * @param enable_validation Enable validation layers
     * @param required_extensions Required instance extensions
     * @return true if successful
     */
    bool initialize(bool enable_validation = true,
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

private:
    VKInstance m_instance;
    VKDevice m_device;
};

}
