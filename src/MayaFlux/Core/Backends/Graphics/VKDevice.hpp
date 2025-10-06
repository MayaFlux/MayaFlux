#pragma once

#include "VKInstance.hpp"

namespace MayaFlux::Core {

struct GraphicsBackendInfo;

/**
 * @struct QueueFamilyIndices
 * @brief Stores indices of queue families we need
 */
struct QueueFamilyIndices {
    std::optional<uint32_t> graphics_family;
    std::optional<uint32_t> compute_family;
    std::optional<uint32_t> transfer_family;

    [[nodiscard]] bool is_complete() const
    {
        return graphics_family.has_value();
    }
};

/**
 * @class VKDevice
 * @brief Manages Vulkan physical device selection and logical device creation
 *
 * Handles GPU selection and creates the logical device interface
 * for executing commands.
 */
class VKDevice {
public:
    VKDevice() = default;
    ~VKDevice();

    VKDevice(const VKDevice&) = delete;
    VKDevice& operator=(const VKDevice&) = delete;
    VKDevice(VKDevice&&) noexcept;
    VKDevice& operator=(VKDevice&&) noexcept;

    /**
     * @brief Initialize device (pick physical device and create logical device)
     * @param instance Vulkan instance
     * @param backend_info Graphics surface configuration
     * @return true if initialization succeeded
     */
    bool initialize(vk::Instance instance, const GraphicsBackendInfo& backend_info);

    /**
     * @brief Cleanup device resources
     */
    void cleanup();

    /**
     * @brief Get physical device handle
     */
    [[nodiscard]] vk::PhysicalDevice get_physical_device() const { return m_physical_device; }

    /**
     * @brief Get logical device handle
     */
    [[nodiscard]] vk::Device get_device() const { return m_logical_device; }

    /**
     * @brief Get graphics queue
     */
    [[nodiscard]] vk::Queue get_graphics_queue() const { return m_graphics_queue; }

    /**
     * @brief Get compute queue (may be same as graphics)
     */
    [[nodiscard]] vk::Queue get_compute_queue() const { return m_compute_queue; }

    /**
     * @brief Get transfer queue (may be same as graphics)
     */
    [[nodiscard]] vk::Queue get_transfer_queue() const { return m_transfer_queue; }

    /**
     * @brief Get queue family indices
     */
    [[nodiscard]] const QueueFamilyIndices& get_queue_families() const { return m_queue_families; }

private:
    vk::PhysicalDevice m_physical_device; ///< Selected physical device (GPU)
    vk::Device m_logical_device; ///< Logical device handle

    vk::Queue m_graphics_queue; ///< Graphics queue handle
    vk::Queue m_compute_queue; ///< Compute queue handle
    vk::Queue m_transfer_queue; ///< Transfer queue handle

    QueueFamilyIndices m_queue_families; ///< Indices of required queue families

    /**
     * @brief Pick a suitable physical device (GPU)
     * @param instance Vulkan instance
     * @return true if a suitable device was found
     */
    bool pick_physical_device(vk::Instance instance);

    /**
     * @brief Find queue families on the given physical device
     * @param device Physical device to query
     * @return QueueFamilyIndices with found queue family indices
     */
    QueueFamilyIndices find_queue_families(vk::PhysicalDevice device);

    /**
     * @brief Create the logical device and retrieve queue handles
     * @param instance Vulkan instance
     * @param backend_info Graphics backend configuration

     * @return true if logical device creation succeeded
     */
    bool create_logical_device(vk::Instance instance, const GraphicsBackendInfo& backend_info);

    // Add more members and methods as needed for device management
};
}
