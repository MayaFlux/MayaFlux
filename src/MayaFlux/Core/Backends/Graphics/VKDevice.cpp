#include "VKDevice.hpp"
#include "MayaFlux/Core/GlobalGraphicsInfo.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

#include "set"

namespace MayaFlux::Core {

VKDevice::~VKDevice()
{
    cleanup();
}

VKDevice::VKDevice(VKDevice&& other) noexcept
    : m_physical_device(other.m_physical_device)
    , m_logical_device(other.m_logical_device)
    , m_graphics_queue(other.m_graphics_queue)
    , m_compute_queue(other.m_compute_queue)
    , m_transfer_queue(other.m_transfer_queue)
    , m_queue_families(other.m_queue_families)
{
    other.m_physical_device = VK_NULL_HANDLE;
    other.m_logical_device = VK_NULL_HANDLE;
    other.m_graphics_queue = VK_NULL_HANDLE;
    other.m_compute_queue = VK_NULL_HANDLE;
    other.m_transfer_queue = VK_NULL_HANDLE;
}

VKDevice& VKDevice::operator=(VKDevice&& other) noexcept
{
    if (this != &other) {
        cleanup();
        m_physical_device = other.m_physical_device;
        m_logical_device = other.m_logical_device;
        m_graphics_queue = other.m_graphics_queue;
        m_compute_queue = other.m_compute_queue;
        m_transfer_queue = other.m_transfer_queue;
        m_queue_families = other.m_queue_families;

        other.m_physical_device = VK_NULL_HANDLE;
        other.m_logical_device = VK_NULL_HANDLE;
        other.m_graphics_queue = VK_NULL_HANDLE;
        other.m_compute_queue = VK_NULL_HANDLE;
        other.m_transfer_queue = VK_NULL_HANDLE;
    }
    return *this;
}

bool VKDevice::initialize(vk::Instance instance, const GraphicsBackendInfo& backend_info)
{
    if (!pick_physical_device(instance)) {
        return false;
    }

    return create_logical_device(instance, backend_info);
}

void VKDevice::cleanup()
{
    if (m_logical_device) {
        m_logical_device.destroy();
        m_logical_device = nullptr;
        MF_INFO(Journal::Component::Core, Journal::Context::GraphicsBackend, "Vulkan logical device destroyed.");
    }
    m_physical_device = nullptr;
    m_graphics_queue = nullptr;
    m_compute_queue = nullptr;
    m_transfer_queue = nullptr;
    m_queue_families = {};
}

bool VKDevice::pick_physical_device(vk::Instance instance)
{
    vk::Instance vk_instance(instance);
    auto devices = vk_instance.enumeratePhysicalDevices();

    if (devices.empty()) {
        error<std::runtime_error>(Journal::Component::Core, Journal::Context::GraphicsBackend,
            std::source_location::current(),
            "Failed to find GPUs with Vulkan support!");
    }

    for (const auto& device : devices) {
        QueueFamilyIndices indices = find_queue_families(device);

        if (indices.is_complete()) {
            m_physical_device = device;
            m_queue_families = indices;

            vk::PhysicalDeviceProperties props = device.getProperties();
            MF_PRINT(Journal::Component::Core, Journal::Context::GraphicsBackend,
                "Selected GPU: {}", props.deviceName.data());
            return true;
        }
    }

    error<std::runtime_error>(Journal::Component::Core, Journal::Context::GraphicsBackend,
        std::source_location::current(),
        "Failed to find a suitable GPU!");
    return false;
}

QueueFamilyIndices VKDevice::find_queue_families(vk::PhysicalDevice device)
{
    QueueFamilyIndices indices;

    auto queue_families = device.getQueueFamilyProperties();

    int i = 0;
    for (const auto& queue_family : queue_families) {
        if (queue_family.queueCount > 0 && queue_family.queueFlags & vk::QueueFlagBits::eGraphics) {
            indices.graphics_family = i;
        }

        if (queue_family.queueCount > 0 && queue_family.queueFlags & vk::QueueFlagBits::eCompute && !(queue_family.queueFlags & vk::QueueFlagBits::eGraphics)) {
            indices.compute_family = i;
        }

        if (queue_family.queueCount > 0 && queue_family.queueFlags & vk::QueueFlagBits::eTransfer && !(queue_family.queueFlags & vk::QueueFlagBits::eGraphics) && !(queue_family.queueFlags & vk::QueueFlagBits::eCompute)) {
            indices.transfer_family = i;
        }

        i++;
    }

    if (indices.graphics_family.has_value()) {
        if (!indices.compute_family.has_value()) {
            indices.compute_family = indices.graphics_family;
        }
        if (!indices.transfer_family.has_value()) {
            indices.transfer_family = indices.graphics_family;
        }
    }

    return indices;
}

bool VKDevice::create_logical_device(vk::Instance instance, const GraphicsBackendInfo& backend_info)
{
    if (!m_queue_families.graphics_family.has_value()) {
        error<std::runtime_error>(Journal::Component::Core, Journal::Context::GraphicsBackend,
            std::source_location::current(),
            "No graphics queue family found!");
    }

    std::set<u_int32_t> unique_queue_families;
    unique_queue_families.insert(m_queue_families.graphics_family.value());

    if (m_queue_families.compute_family.has_value()) {
        unique_queue_families.insert(m_queue_families.compute_family.value());
    }

    if (m_queue_families.transfer_family.has_value() && m_queue_families.transfer_family != m_queue_families.graphics_family) {
        unique_queue_families.insert(m_queue_families.transfer_family.value());
    }

    std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
    float queue_priority = 1.0F;

    for (uint32_t queue_family : unique_queue_families) {
        vk::DeviceQueueCreateInfo queue_create_info {};
        queue_create_info.queueFamilyIndex = queue_family;
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = &queue_priority;
        queue_create_infos.push_back(queue_create_info);
    }

    vk::PhysicalDeviceFeatures device_features {};
    device_features.samplerAnisotropy = backend_info.required_features.sampler_anisotropy;
    device_features.geometryShader = backend_info.required_features.geometry_shaders;
    device_features.tessellationShader = backend_info.required_features.tessellation_shaders;
    device_features.multiViewport = backend_info.required_features.multi_viewport;
    device_features.fillModeNonSolid = backend_info.required_features.fill_mode_non_solid;

    std::vector<const char*> device_extensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    for (const auto& ext : backend_info.required_extensions) {
        device_extensions.push_back(ext.c_str());
    }

    vk::DeviceCreateInfo create_info {};
    create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
    create_info.pQueueCreateInfos = queue_create_infos.data();
    create_info.pEnabledFeatures = &device_features;
    create_info.enabledExtensionCount = 0;
    create_info.enabledLayerCount = 0;

    try {
        m_logical_device = m_physical_device.createDevice(create_info);
    } catch (const std::exception& e) {
        error<std::runtime_error>(Journal::Component::Core, Journal::Context::GraphicsBackend,
            std::source_location::current(),
            "Failed to create logical device: {}", e.what());
    }

    m_graphics_queue = m_logical_device.getQueue(m_queue_families.graphics_family.value(), 0);

    if (backend_info.enable_compute_queue && m_queue_families.compute_family.has_value()) {
        m_compute_queue = m_logical_device.getQueue(m_queue_families.compute_family.value(), 0);
    } else {
        m_compute_queue = m_graphics_queue;
    }

    if (backend_info.enable_transfer_queue && m_queue_families.transfer_family.has_value()) {
        m_transfer_queue = m_logical_device.getQueue(m_queue_families.transfer_family.value(), 0);
    } else {
        m_transfer_queue = m_graphics_queue;
    }

    return true;
}

}
