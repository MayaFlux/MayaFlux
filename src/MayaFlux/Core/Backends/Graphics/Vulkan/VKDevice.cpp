#include "VKDevice.hpp"
#include "MayaFlux/Core/GlobalGraphicsInfo.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

#include "set"

#ifdef GLFW_BACKEND
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#endif

#if defined(WIN32_BACKEND)
#include <vulkan/vulkan_win32.h>
#endif

#if defined(WAYLAND_BACKEND)
#include <vulkan/vulkan_wayland.h>
#include <wayland-client.h>
#endif

namespace MayaFlux::Core {

namespace {
    /**
     * @brief Scratch native connection plus surfaceless presentation query.
     *
     * Device selection runs before any window exists, so
     * vkGetPhysicalDeviceSurfaceSupportKHR is unavailable: it needs a surface,
     * which needs a window. The platform entry points answer the same question
     * from a native display connection instead.
     *
     * On Wayland no compositor connection exists at selection time either, since
     * each WaylandWindow calls wl_display_connect in its own constructor. One
     * scratch connection is opened for the selection pass and closed after; the
     * query does not care which connection it is given.
     *
     * When the platform has no entry point, or the connection fails, available is
     * false and supports() returns true for every family so selection proceeds
     * unverified rather than rejecting every device.
     */
    struct PresentationProbe {
        vk::Instance instance;
        void* native_display {};
        bool available {};
        const char* mechanism { "unavailable" };

        explicit PresentationProbe(vk::Instance inst)
            : instance(inst)
        {
#if defined(GLFW_BACKEND)
            available = glfwVulkanSupported() == GLFW_TRUE;
            mechanism = available ? "glfw" : "unavailable";
#elif defined(WIN32_BACKEND)
            available = true;
            mechanism = "win32";
#elif defined(WAYLAND_BACKEND)
            native_display = wl_display_connect(nullptr);
            available = native_display != nullptr;
            mechanism = available ? "wayland" : "unavailable";
#endif
        }

        ~PresentationProbe()
        {
#if defined(WAYLAND_BACKEND)
            if (native_display)
                wl_display_disconnect(static_cast<wl_display*>(native_display));
#endif
        }

        PresentationProbe(const PresentationProbe&) = delete;
        PresentationProbe& operator=(const PresentationProbe&) = delete;
        PresentationProbe(PresentationProbe&&) = delete;
        PresentationProbe& operator=(PresentationProbe&&) = delete;

        [[nodiscard]] bool supports(vk::PhysicalDevice device, uint32_t family_index) const
        {
            if (!available)
                return true;

#if defined(GLFW_BACKEND)
            return glfwGetPhysicalDevicePresentationSupport(
                       static_cast<VkInstance>(instance),
                       static_cast<VkPhysicalDevice>(device),
                       family_index)
                == GLFW_TRUE;
#elif defined(WIN32_BACKEND)
            return vkGetPhysicalDeviceWin32PresentationSupportKHR(
                       static_cast<VkPhysicalDevice>(device), family_index)
                == VK_TRUE;
#elif defined(WAYLAND_BACKEND)
            return vkGetPhysicalDeviceWaylandPresentationSupportKHR(
                       static_cast<VkPhysicalDevice>(device), family_index,
                       static_cast<wl_display*>(native_display))
                == VK_TRUE;
#else
            return true;
#endif
        }
    };

    /** @brief Format a Vulkan UUID as 32 lowercase hex characters, no separators. */
    std::string uuid_to_hex(const uint8_t* uuid)
    {
        std::string out;
        out.reserve(VK_UUID_SIZE * 2);
        for (size_t i = 0; i < VK_UUID_SIZE; ++i) {
            out += std::format("{:02x}", uuid[i]);
        }
        return out;
    }

    /** @brief Case-insensitive substring test. */
    bool contains_nocase(std::string_view haystack, std::string_view needle)
    {
        if (needle.empty() || needle.size() > haystack.size())
            return false;

        auto it = std::search(haystack.begin(), haystack.end(),
            needle.begin(), needle.end(),
            [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b)); });

        return it != haystack.end();
    }

    /**
     * @brief Everything selection needs to know about one enumerated device.
     *
     * Gathered in a single pass so that scoring, selector matching, and the
     * candidate log all read the same values.
     */
    struct DeviceCandidate {
        vk::PhysicalDevice device;
        uint32_t index {};
        std::string name;
        std::string uuid_hex;
        std::array<uint8_t, VK_UUID_SIZE> uuid {};
        vk::PhysicalDeviceType type {};
        uint32_t api_version {};
        vk::DriverId driver_id {};
        std::string driver_name;
        QueueFamilyIndices families;
        bool has_swapchain {};
        bool has_mesh_shader {};
        uint32_t present_family_mask {};
        bool graphics_presents {};
        bool has_pci_info {};
        uint32_t pci_bus {};
        uint64_t device_local_bytes {};
        int64_t score {};
        const char* reject_reason {};

        [[nodiscard]] bool viable() const { return reject_reason == nullptr; }
    };

    /** @brief Base score by device class, before preference and capability bonuses. */
    int64_t type_base_score(vk::PhysicalDeviceType type)
    {
        switch (type) {
        case vk::PhysicalDeviceType::eDiscreteGpu:
            return 4000;
        case vk::PhysicalDeviceType::eIntegratedGpu:
            return 2000;
        case vk::PhysicalDeviceType::eVirtualGpu:
            return 1000;
        case vk::PhysicalDeviceType::eCpu:
            return 100;
        default:
            return 500;
        }
    }

    /**
     * @brief Read MAYAFLUX_GPU, an all-digits value being an index and anything
     *        else a name substring.
     * @param out_index Receives the parsed index, or -1.
     * @param out_name Receives the name substring, or empty.
     * @return true if the variable was set and non-empty.
     */
    bool read_gpu_env(int32_t& out_index, std::string& out_name)
    {
        const char* raw = std::getenv("MAYAFLUX_GPU");
        if (!raw || *raw == '\0')
            return false;

        std::string_view value(raw);
        if (std::ranges::all_of(value, [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; })) {
            out_index = static_cast<int32_t>(std::strtol(raw, nullptr, 10));
            out_name.clear();
        } else {
            out_index = -1;
            out_name = value;
        }
        return true;
    }

} // namespace

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
    , m_graphics_presents(other.m_graphics_presents)
    , m_present_queues(std::move(other.m_present_queues))
    , m_device_name(std::move(other.m_device_name))
    , m_device_uuid(other.m_device_uuid)
    , m_supports_mesh_shaders(other.m_supports_mesh_shaders)
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
        m_graphics_presents = other.m_graphics_presents;
        m_present_queues = std::move(other.m_present_queues);
        m_device_name = std::move(other.m_device_name);
        m_device_uuid = other.m_device_uuid;
        m_supports_mesh_shaders = other.m_supports_mesh_shaders;

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
    if (!pick_physical_device(instance, backend_info)) {
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
    m_present_queues.clear();
    m_device_name.clear();
    m_device_uuid = {};
    m_graphics_presents = false;
    m_supports_mesh_shaders = false;
}

bool VKDevice::pick_physical_device(vk::Instance instance, const GraphicsBackendInfo& backend_info)
{
    auto devices = instance.enumeratePhysicalDevices();

    if (devices.empty()) {
        error<std::runtime_error>(Journal::Component::Core, Journal::Context::GraphicsBackend,
            std::source_location::current(),
            "Failed to find GPUs with Vulkan support!");
    }

    const PresentationProbe probe(instance);

    if (backend_info.require_presentation && !probe.available) {
        MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Presentation support cannot be verified on this platform; "
            "device selection will not filter on it");
    }

    std::vector<DeviceCandidate> candidates;
    candidates.reserve(devices.size());

    for (uint32_t i = 0; i < devices.size(); ++i) {
        const auto& device = devices[i];

        DeviceCandidate cand;
        cand.device = device;
        cand.index = i;

        auto available_extensions = device.enumerateDeviceExtensionProperties();
        bool has_pci_ext = false;

        for (const auto& ext : available_extensions) {
            if (strcmp(ext.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0)
                cand.has_swapchain = true;
            if (strcmp(ext.extensionName, VK_EXT_MESH_SHADER_EXTENSION_NAME) == 0)
                cand.has_mesh_shader = true;
            if (strcmp(ext.extensionName, VK_EXT_PCI_BUS_INFO_EXTENSION_NAME) == 0)
                has_pci_ext = true;
        }

        auto prop_chain = vk::StructureChain {
            vk::PhysicalDeviceProperties2 {},
            vk::PhysicalDeviceVulkan11Properties {},
            vk::PhysicalDeviceVulkan12Properties {},
            vk::PhysicalDevicePCIBusInfoPropertiesEXT {}
        };

        if (!has_pci_ext) {
            prop_chain.unlink<vk::PhysicalDevicePCIBusInfoPropertiesEXT>();
        }

        device.getProperties2(&prop_chain.get<vk::PhysicalDeviceProperties2>());

        const auto& props = prop_chain.get<vk::PhysicalDeviceProperties2>().properties;
        const auto& props11 = prop_chain.get<vk::PhysicalDeviceVulkan11Properties>();
        const auto& props12 = prop_chain.get<vk::PhysicalDeviceVulkan12Properties>();

        cand.name = props.deviceName.data();
        cand.type = props.deviceType;
        cand.api_version = props.apiVersion;
        cand.driver_id = props12.driverID;
        cand.driver_name = props12.driverName.data();
        std::copy_n(props11.deviceUUID.data(), VK_UUID_SIZE, cand.uuid.begin());
        cand.uuid_hex = uuid_to_hex(cand.uuid.data());

        if (has_pci_ext) {
            cand.has_pci_info = true;
            cand.pci_bus = prop_chain.get<vk::PhysicalDevicePCIBusInfoPropertiesEXT>().pciBus;
        }

        auto memory_props = device.getMemoryProperties();
        for (uint32_t h = 0; h < memory_props.memoryHeapCount; ++h) {
            if (memory_props.memoryHeaps[h].flags & vk::MemoryHeapFlagBits::eDeviceLocal) {
                cand.device_local_bytes = std::max(cand.device_local_bytes,
                    static_cast<uint64_t>(memory_props.memoryHeaps[h].size));
            }
        }

        cand.families = find_queue_families(device);

        if (cand.has_mesh_shader) {
            vk::PhysicalDeviceMeshShaderFeaturesEXT mesh_features;
            vk::PhysicalDeviceFeatures2 features;
            features.pNext = &mesh_features;
            device.getFeatures2(&features);

            cand.has_mesh_shader = mesh_features.meshShader == VK_TRUE
                && mesh_features.taskShader == VK_TRUE;
        }

        {
            auto family_props = device.getQueueFamilyProperties();
            const uint32_t probe_count = std::min<uint32_t>(static_cast<uint32_t>(family_props.size()), QueueFamilyIndices::MAX_TRACKED_FAMILIES);

            for (uint32_t f = 0; f < probe_count; ++f) {
                if (probe.supports(device, f))
                    cand.present_family_mask |= (1U << f);
            }

            cand.families.present_family_mask = cand.present_family_mask;

            if (cand.families.graphics_family.has_value())
                cand.graphics_presents = cand.families.can_present(cand.families.graphics_family.value());
        }

        if (!cand.families.graphics_family.has_value()) {
            cand.reject_reason = "no graphics queue family";
        } else if (cand.api_version < VK_API_VERSION_1_3) {
            cand.reject_reason = "device API version below 1.3";
        } else if (!cand.has_swapchain) {
            cand.reject_reason = "no VK_KHR_swapchain";
        } else if (backend_info.require_presentation && !cand.graphics_presents) {
            cand.reject_reason = "graphics family cannot present";
        }

        cand.score = type_base_score(cand.type);

        switch (backend_info.device_preference) {
        case GraphicsBackendInfo::DevicePreference::DISCRETE:
            if (cand.type == vk::PhysicalDeviceType::eDiscreteGpu)
                cand.score += 10000;
            break;
        case GraphicsBackendInfo::DevicePreference::INTEGRATED:
            if (cand.type == vk::PhysicalDeviceType::eIntegratedGpu)
                cand.score += 10000;
            break;
        case GraphicsBackendInfo::DevicePreference::VIRTUAL:
            if (cand.type == vk::PhysicalDeviceType::eVirtualGpu)
                cand.score += 10000;
            break;
        case GraphicsBackendInfo::DevicePreference::EXTERNAL:
            if (cand.type == vk::PhysicalDeviceType::eDiscreteGpu) {
                cand.score += 10000;
                if (cand.has_pci_info)
                    cand.score += static_cast<int64_t>(cand.pci_bus) * 4;
            }
            break;
        case GraphicsBackendInfo::DevicePreference::AUTO:
        default:
            break;
        }

        if (cand.graphics_presents)
            cand.score += 500;
        if (cand.has_mesh_shader)
            cand.score += 100;

        cand.score += static_cast<int64_t>(cand.device_local_bytes >> 30U);

        candidates.push_back(std::move(cand));
    }

    for (const auto& cand : candidates) {
        MF_INFO(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "GPU [{}] {} | type={} driver={} ({}) api={}.{}.{} | uuid={} | "
            "gfx={} compute={} transfer={} | present=gfx:{} mask:{:#x} mesh={} vram={}MB pci_bus={} | score={}{}{}",
            cand.index, cand.name, vk::to_string(cand.type),
            vk::to_string(cand.driver_id), cand.driver_name,
            VK_API_VERSION_MAJOR(cand.api_version),
            VK_API_VERSION_MINOR(cand.api_version),
            VK_API_VERSION_PATCH(cand.api_version),
            cand.uuid_hex,
            cand.families.graphics_family.has_value() ? std::to_string(cand.families.graphics_family.value()) : "none",
            cand.families.compute_family.has_value() ? std::to_string(cand.families.compute_family.value()) : "none",
            cand.families.transfer_family.has_value() ? std::to_string(cand.families.transfer_family.value()) : "none",
            cand.graphics_presents ? "yes" : "no",
            cand.present_family_mask,
            cand.has_mesh_shader ? "yes" : "no",
            cand.device_local_bytes >> 20U,
            cand.has_pci_info ? std::to_string(cand.pci_bus) : "n/a",
            cand.score,
            cand.viable() ? "" : " | REJECTED: ",
            cand.reject_reason ? cand.reject_reason : "");
    }

    MF_INFO(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Presentation probe mechanism: {}, require_presentation={}, device_preference={}",
        probe.mechanism, backend_info.require_presentation ? "true" : "false",
        Reflect::enum_to_string(backend_info.device_preference));

    int32_t want_index = backend_info.device_index;
    std::string want_name = backend_info.device_name;
    std::string want_uuid = backend_info.device_uuid;

    if (int32_t env_index = -1; read_gpu_env(env_index, want_name)) {
        want_index = env_index;
        want_uuid.clear();
        MF_INFO(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "MAYAFLUX_GPU override active (index={}, name='{}')", want_index, want_name);
    }

    const DeviceCandidate* selected = nullptr;
    const char* selection_basis = "score";

    if (want_index >= 0) {
        for (const auto& cand : candidates) {
            if (static_cast<int32_t>(cand.index) == want_index && cand.viable()) {
                selected = &cand;
                selection_basis = "device_index";
                break;
            }
        }
    }

    if (!selected && !want_uuid.empty()) {
        for (const auto& cand : candidates) {
            if (cand.uuid_hex == want_uuid && cand.viable()) {
                selected = &cand;
                selection_basis = "device_uuid";
                break;
            }
        }
    }

    if (!selected && !want_name.empty()) {
        for (const auto& cand : candidates) {
            if (!cand.viable() || !contains_nocase(cand.name, want_name))
                continue;
            if (!selected || cand.score > selected->score) {
                selected = &cand;
                selection_basis = "device_name";
            }
        }
    }

    const bool selector_requested = want_index >= 0 || !want_uuid.empty() || !want_name.empty();

    if (selector_requested && !selected) {
        if (backend_info.strict_device_selection) {
            error<std::runtime_error>(Journal::Component::Core, Journal::Context::GraphicsBackend,
                std::source_location::current(),
                "No viable physical device matched the requested selector "
                "(index={}, uuid='{}', name='{}') and strict_device_selection is set",
                want_index, want_uuid, want_name);
        }

        MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "No viable physical device matched the requested selector "
            "(index={}, uuid='{}', name='{}'); falling back to score",
            want_index, want_uuid, want_name);
    }

    if (!selected) {
        for (const auto& cand : candidates) {
            if (!cand.viable())
                continue;
            if (!selected || cand.score > selected->score)
                selected = &cand;
        }
    }

    if (!selected) {
        error<std::runtime_error>(Journal::Component::Core, Journal::Context::GraphicsBackend,
            std::source_location::current(),
            "No suitable GPU found among {} enumerated device(s); "
            "see the candidate list above for rejection reasons",
            candidates.size());
    }

    m_physical_device = selected->device;
    m_queue_families = selected->families;
    m_supports_mesh_shaders = selected->has_mesh_shader;
    m_graphics_presents = selected->graphics_presents;
    m_device_name = selected->name;
    m_device_uuid = selected->uuid;

    MF_LOG(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Selected GPU [{}] {} by {} (uuid={}, score={})",
        selected->index, selected->name, selection_basis, selected->uuid_hex, selected->score);

    return true;
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

bool VKDevice::graphics_family_can_present(vk::SurfaceKHR surface) const
{
    if (!surface || !m_queue_families.graphics_family.has_value())
        return false;

    return m_physical_device.getSurfaceSupportKHR(
               m_queue_families.graphics_family.value(), surface)
        == VK_TRUE;
}

vk::Queue VKDevice::get_present_queue(uint32_t family_index) const
{
    auto it = m_present_queues.find(family_index);
    return it != m_present_queues.end() ? it->second : nullptr;
}

vk::Queue VKDevice::get_preferred_present_queue() const
{
    auto family = m_queue_families.preferred_present_family();
    return family.has_value() ? get_present_queue(family.value()) : nullptr;
}

void VKDevice::query_supported_extensions()
{
    std::vector<vk::ExtensionProperties> availableExtensions = m_physical_device.enumerateDeviceExtensionProperties();

    MF_LOG(Journal::Component::Core, Journal::Context::GraphicsBackend, "Available physical device extensions:");
    for (const auto& extension : availableExtensions) {
        std::cout << "\t- " << extension.extensionName << " (Version: " << extension.specVersion << ")\n";
    }
    MF_LOG(Journal::Component::Core, Journal::Context::GraphicsBackend, "End of list.");
}

bool VKDevice::create_logical_device(vk::Instance /*instance*/, const GraphicsBackendInfo& backend_info)
{
    if (!m_queue_families.graphics_family.has_value()) {
        error<std::runtime_error>(Journal::Component::Core, Journal::Context::GraphicsBackend,
            std::source_location::current(),
            "No graphics queue family found!");
    }

    std::set<uint32_t> unique_queue_families;
    unique_queue_families.insert(m_queue_families.graphics_family.value());

    if (m_queue_families.compute_family.has_value()) {
        unique_queue_families.insert(m_queue_families.compute_family.value());
    }

    if (m_queue_families.transfer_family.has_value() && m_queue_families.transfer_family != m_queue_families.graphics_family) {
        unique_queue_families.insert(m_queue_families.transfer_family.value());
    }

    for (uint32_t f = 0; f < QueueFamilyIndices::MAX_TRACKED_FAMILIES; ++f) {
        if (m_queue_families.can_present(f))
            unique_queue_families.insert(f);
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

    auto supported_extensions = m_physical_device.enumerateDeviceExtensionProperties();

    auto is_supported = [&supported_extensions](std::string_view name) {
        return std::ranges::any_of(supported_extensions, [name](const auto& ext) {
            return name == ext.extensionName.data();
        });
    };

#ifdef MAYAFLUX_PLATFORM_MACOS
    if (is_supported("VK_KHR_portability_subset")) {
        device_extensions.push_back("VK_KHR_portability_subset");
    }

    auto feature_chain = vk::StructureChain {
        vk::PhysicalDeviceFeatures2 {},
        vk::PhysicalDeviceVulkan13Features {},
        vk::PhysicalDeviceVulkan12Features {}
    };

#else
    if (m_supports_mesh_shaders) {
        device_extensions.push_back(VK_EXT_MESH_SHADER_EXTENSION_NAME);
    }

    auto feature_chain = vk::StructureChain {
        vk::PhysicalDeviceFeatures2 {},
        vk::PhysicalDeviceVulkan13Features {},
        vk::PhysicalDeviceVulkan12Features {},
        vk::PhysicalDeviceMeshShaderFeaturesEXT {}
    };

    if (!m_supports_mesh_shaders) {
        feature_chain.unlink<vk::PhysicalDeviceMeshShaderFeaturesEXT>();
    } else {
        feature_chain.get<vk::PhysicalDeviceMeshShaderFeaturesEXT>().taskShader = VK_TRUE;
        feature_chain.get<vk::PhysicalDeviceMeshShaderFeaturesEXT>().meshShader = VK_TRUE;
    }
#endif

    feature_chain.get<vk::PhysicalDeviceFeatures2>().features = device_features;
    feature_chain.get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering = VK_TRUE;
    feature_chain.get<vk::PhysicalDeviceVulkan13Features>().synchronization2 = VK_TRUE;
    feature_chain.get<vk::PhysicalDeviceVulkan12Features>().bufferDeviceAddress = VK_TRUE;

    std::vector<std::string> missing_required;

    for (const auto& ext : backend_info.required_extensions) {
        if (is_supported(ext)) {
            device_extensions.push_back(ext.c_str());
        } else {
            missing_required.push_back(ext);
        }
    }

    if (!missing_required.empty()) {
        std::string names;
        for (const auto& ext : missing_required) {
            if (!names.empty())
                names += ", ";
            names += ext;
        }

        error<std::runtime_error>(Journal::Component::Core, Journal::Context::GraphicsBackend,
            std::source_location::current(),
            "Device '{}' does not support required extension(s): {}",
            m_device_name, names);
    }

    for (const auto& ext : backend_info.optional_extensions) {
        if (is_supported(ext)) {
            device_extensions.push_back(ext.c_str());
        } else {
            MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
                "Device '{}' does not support optional extension '{}'; skipping",
                m_device_name, ext);
        }
    }

    vk::DeviceCreateInfo create_info {};
    create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
    create_info.pQueueCreateInfos = queue_create_infos.data();
    create_info.pNext = &feature_chain.get<vk::PhysicalDeviceFeatures2>();
    create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
    create_info.ppEnabledExtensionNames = device_extensions.data();

    try {
        m_logical_device = m_physical_device.createDevice(create_info);
        VULKAN_HPP_DEFAULT_DISPATCHER.init(m_logical_device);

    } catch (const std::exception& e) {
        error_rethrow(Journal::Component::Core, Journal::Context::GraphicsBackend,
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

    for (uint32_t f = 0; f < QueueFamilyIndices::MAX_TRACKED_FAMILIES; ++f) {
        if (m_queue_families.can_present(f))
            m_present_queues[f] = m_logical_device.getQueue(f, 0);
    }

    return true;
}

}
