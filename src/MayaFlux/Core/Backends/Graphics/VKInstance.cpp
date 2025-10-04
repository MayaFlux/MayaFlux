#include "VKInstance.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Core {

static const std::vector<const char*> VALIDATION_LAYERS = {
    "VK_LAYER_KHRONOS_validation"
};

VKInstance::~VKInstance()
{
    cleanup();
}

VKInstance::VKInstance(VKInstance&& other) noexcept
    : m_instance(other.m_instance)
    , m_debug_messenger(other.m_debug_messenger)
    , m_validation_enabled(other.m_validation_enabled)
{
    other.m_instance = nullptr;
    other.m_debug_messenger = nullptr;
}

VKInstance& VKInstance::operator=(VKInstance&& other) noexcept
{
    if (this != &other) {
        cleanup();
        m_instance = other.m_instance;
        m_debug_messenger = other.m_debug_messenger;
        m_validation_enabled = other.m_validation_enabled;

        other.m_instance = nullptr;
        other.m_debug_messenger = nullptr;
    }
    return *this;
}

bool VKInstance::initialize(bool enable_validation,
    const std::vector<const char*>& required_extensions)
{
    m_validation_enabled = enable_validation;

    if (m_validation_enabled && !check_validation_layer_support(VALIDATION_LAYERS)) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend, "Vulkan validation layers requested, but not available!");
        return false;
    }

    vk::ApplicationInfo app_info {};
    app_info.pApplicationName = "MayaFlux";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "MayaFlux Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_2;

    vk::InstanceCreateInfo create_info {};
    create_info.pApplicationInfo = &app_info;

    std::vector<const char*> extensions = required_extensions;
    if (m_validation_enabled) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    create_info.enabledExtensionCount = static_cast<u_int32_t>(extensions.size());
    create_info.ppEnabledExtensionNames = extensions.data();

    vk::DebugUtilsMessengerCreateInfoEXT debug_create_info {};

    if (m_validation_enabled) {
        create_info.enabledLayerCount = static_cast<u_int32_t>(VALIDATION_LAYERS.size());
        create_info.ppEnabledLayerNames = VALIDATION_LAYERS.data();

        debug_create_info.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose
            | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;

        debug_create_info.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
            | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
            | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;

        debug_create_info.pfnUserCallback = debug_callback;
        create_info.pNext = &debug_create_info;

    } else {
        create_info.enabledLayerCount = 0;
        create_info.pNext = nullptr;
    }

    try {
        m_instance = vk::createInstance(create_info);
        m_dynamic_dispatcher = vk::detail::DispatchLoaderDynamic(m_instance, vkGetInstanceProcAddr);

    } catch (const std::exception& e) {
        error<std::runtime_error>(Journal::Component::Core, Journal::Context::GraphicsBackend,
            std::source_location::current(),
            "Failed to create Vulkan instance: {}", e.what());
    }

    MF_PRINT(Journal::Component::Core, Journal::Context::GraphicsBackend, "Vulkan instance created.");

    if (m_validation_enabled && !setup_debug_messenger()) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend, "Failed to set up debug messenger!");
        return false;
    }

    return true;
}

void VKInstance::cleanup()
{
    if (m_debug_messenger) {
        m_instance.destroyDebugUtilsMessengerEXT(m_debug_messenger, nullptr, m_dynamic_dispatcher);
        m_debug_messenger = nullptr;
    }

    if (m_instance) {
        m_instance.destroy();
        m_instance = nullptr;
        MF_INFO(Journal::Component::Core, Journal::Context::GraphicsBackend, "Vulkan instance destroyed.");
    }
}

bool VKInstance::check_validation_layer_support(const std::vector<const char*>& layers)
{
    auto available_layers = vk::enumerateInstanceLayerProperties();

    for (const char* layer_name : layers) {
        bool layer_found = false;

        for (const auto& layer_properties : available_layers) {
            if (strcmp(layer_name, layer_properties.layerName) == 0) {
                layer_found = true;
                break;
            }
        }

        if (!layer_found) {
            return false;
        }
    }

    return true;
}

bool VKInstance::setup_debug_messenger()
{
    vk::DebugUtilsMessengerCreateInfoEXT create_info {};

    create_info.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose
        | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;

    create_info.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
        | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
        | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;

    create_info.pfnUserCallback = debug_callback;
    create_info.pUserData = nullptr;

    try {
        m_debug_messenger = m_instance.createDebugUtilsMessengerEXT(create_info, nullptr, m_dynamic_dispatcher);
    } catch (const std::exception& e) {
        error<std::runtime_error>(Journal::Component::Core, Journal::Context::GraphicsBackend,
            std::source_location::current(),
            "Failed to set up debug messenger: {}", e.what());
        return false;
    }

    return true;
}

vk::Bool32 VKInstance::debug_callback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT /*messageSeverity*/,
    vk::DebugUtilsMessageTypeFlagsEXT /*messageTypes*/,
    const vk::DebugUtilsMessengerCallbackDataEXT* p_callback_data,
    void* /*pUserData*/)
{
    MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend, "Validation layer: {}", p_callback_data->pMessage);
    return VK_FALSE;
}

}
