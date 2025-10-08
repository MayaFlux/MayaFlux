#pragma once

#include <vulkan/vulkan.hpp>

namespace MayaFlux::Core {

/**
 * @class VKInstance
 * @brief Manages Vulkan instance creation and validation layers
 *
 * The instance is the connection between your application and Vulkan.
 * It handles global Vulkan state and validation layers for debugging.
 */
class VKInstance {
public:
    VKInstance() = default;

    /**
     * @brief Destructor to clean up Vulkan instance and debug messenger
     */
    ~VKInstance();

    VKInstance(const VKInstance&) = delete;
    VKInstance& operator=(const VKInstance&) = delete;
    VKInstance(VKInstance&&) noexcept;
    VKInstance& operator=(VKInstance&&) noexcept;

    /**
     * @brief Initialize Vulkan instance
     * @param enable_validation Enable validation layers (recommended for development)
     * @param required_extensions Extensions required (e.g., for GLFW surface)
     * @return true if initialization succeeded
     */
    bool initialize(bool enable_validation = true,
        const std::vector<const char*>& required_extensions = {});

    /**
     * @brief Cleanup Vulkan instance
     */
    void cleanup();

    /**
     * @brief Get the Vulkan instance handle
     * @return Vulkan instance handle
     */
    [[nodiscard]] vk::Instance get_instance() const { return m_instance; }

private:
    vk::Instance m_instance; ///< Vulkan instance handle
    vk::DebugUtilsMessengerEXT m_debug_messenger; ///< Debug messenger for validation layers
    bool m_validation_enabled = false; ///< Flag to indicate if validation layers are enabled

    vk::detail::DispatchLoaderDynamic m_dynamic_dispatcher; ///< Dynamic dispatcher for extension functions

    /**
     * @brief Check if requested validation layers are available
     */
    bool check_validation_layer_support(const std::vector<const char*>& layers);

    /**
     * @brief Setup debug messenger for validation layer output
     */
    bool setup_debug_messenger();

    /**
     * @brief Validation layer callback
     */

    static vk::Bool32 debug_callback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT message_severity,
        vk::DebugUtilsMessageTypeFlagsEXT message_types,
        const vk::DebugUtilsMessengerCallbackDataEXT* p_callback_data,
        void* p_user_data);
};

}
