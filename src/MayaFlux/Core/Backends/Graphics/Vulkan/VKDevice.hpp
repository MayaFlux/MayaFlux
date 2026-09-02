#pragma once

#include "VKInstance.hpp"

namespace MayaFlux::Core {

struct GraphicsBackendInfo;

/**
 * @struct QueueFamilyIndices
 * @brief Stores indices of queue families we need
 *
 * Immutable after VKDevice::create_logical_device. Presentation is a property
 * of the device, the family, and the platform, resolved once at device
 * selection, so present_family_mask is never renegotiated per surface.
 */
struct QueueFamilyIndices {
    static constexpr uint32_t MAX_TRACKED_FAMILIES = 32; ///< Width of present_family_mask

    std::optional<uint32_t> graphics_family;
    std::optional<uint32_t> compute_family;
    std::optional<uint32_t> transfer_family;

    /** @brief Bit i set when family i passed the surfaceless presentation probe. */
    uint32_t present_family_mask {};

    [[nodiscard]] bool is_complete() const
    {
        return graphics_family.has_value();
    }

    /** @brief Whether family @p index can present. */
    [[nodiscard]] bool can_present(uint32_t index) const
    {
        return index < MAX_TRACKED_FAMILIES && (present_family_mask & (1U << index)) != 0;
    }

    /**
     * @brief Family the engine presents on by default.
     * @return The graphics family when it can present, otherwise the lowest
     *         presentation-capable family, otherwise empty.
     */
    [[nodiscard]] std::optional<uint32_t> preferred_present_family() const
    {
        if (graphics_family.has_value() && can_present(graphics_family.value()))
            return graphics_family;

        for (uint32_t i = 0; i < MAX_TRACKED_FAMILIES; ++i) {
            if (can_present(i))
                return i;
        }
        return std::nullopt;
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
     * @brief Select a physical device and create the logical device
     * @param instance Vulkan instance
     * @param backend_info Graphics backend configuration, including device selection
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

    /** @brief Name of the selected physical device */
    [[nodiscard]] const std::string& get_device_name() const { return m_device_name; }

    /** @brief Whether the graphics queue family passed the surfaceless presentation probe */
    [[nodiscard]] bool graphics_family_presents() const { return m_graphics_presents; }

    /**
     * @brief Confirm the graphics family presents to a concrete surface
     * @param surface Surface to check
     * @return true if presentation is supported
     *
     * Device selection already guarantees this when require_presentation is
     * set; this confirms the surfaceless probe against a real surface. Const
     * and non-mutating, so it is safe from any thread.
     */
    [[nodiscard]] bool graphics_family_can_present(vk::SurfaceKHR surface) const;

    /**
     * @brief Queue for a presentation-capable family
     * @param family_index Family index, must be set in present_family_mask
     * @return Queue handle, or null if the family has no presentation support
     *
     * A queue is created for every family in the mask, not only the one the
     * engine's default present path uses, so a caller driving its own
     * presentation has a queue available without recreating the device.
     */
    [[nodiscard]] vk::Queue get_present_queue(uint32_t family_index) const;

    /**
     * @brief Queue for the preferred presentation family
     * @return Queue handle, or null when no family can present
     */
    [[nodiscard]] vk::Queue get_preferred_present_queue() const;

    /**
     * @brief Wait for the device to become idle
     */
    void wait_idle() const
    {
        if (m_logical_device) {
            m_logical_device.waitIdle();
        }
    }

    /**
     * @brief Query and log supported device extensions
     */
    void query_supported_extensions();

    /**
     * @brief Check if the device supports mesh shaders
     * @return true if mesh shaders are supported
     */
    [[nodiscard]] bool supports_mesh_shaders() const { return m_supports_mesh_shaders; }

private:
    vk::PhysicalDevice m_physical_device; ///< Selected physical device (GPU)
    vk::Device m_logical_device; ///< Logical device handle

    vk::Queue m_graphics_queue; ///< Graphics queue handle
    vk::Queue m_compute_queue; ///< Compute queue handle
    vk::Queue m_transfer_queue; ///< Transfer queue handle

    QueueFamilyIndices m_queue_families; ///< Indices of required queue families

    /**
     * @brief Select a physical device by config selector or score
     * @param instance Vulkan instance
     * @param backend_info Graphics backend configuration
     * @return true if a suitable device was found
     */
    bool pick_physical_device(vk::Instance instance, const GraphicsBackendInfo& backend_info);

    /**
     * @brief Find queue families on the given physical device
     * @param device Physical device to query
     * @return QueueFamilyIndices with found queue family indices
     */
    static QueueFamilyIndices find_queue_families(vk::PhysicalDevice device);

    /**
     * @brief Create the logical device and retrieve queue handles
     * @param instance Vulkan instance
     * @param backend_info Graphics backend configuration

     * @return true if logical device creation succeeded
     */
    bool create_logical_device(vk::Instance instance, const GraphicsBackendInfo& backend_info);

    bool m_graphics_presents {}; ///< Graphics family passed the surfaceless presentation probe
    std::unordered_map<uint32_t, vk::Queue> m_present_queues; ///< One queue per presentation-capable family
    std::string m_device_name; ///< Selected device name, cached for logging
    std::array<uint8_t, VK_UUID_SIZE> m_device_uuid {}; ///< Selected device UUID
    bool m_supports_mesh_shaders {}; ///< Whether the device supports mesh shaders
};
}
