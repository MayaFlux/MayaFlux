#pragma once

namespace MayaFlux::Core {
class VulkanBackend;
}

namespace MayaFlux::Portal::Graphics {

/**
 * @brief Initialize Portal::Graphics Glue Layer
 * @param backend Initialized Vulkan backend
 * @return True if initialization succeeded
 *
 * Initializes all Portal::Graphics managers:
 * - TextureLoom (texture management)
 * - SamplerForge (sampler creation)
 * - ShaderFoundry (shader compilation + resources)
 * - ComputePress (compute pipeline + dispatch)
 * - RenderFlow (graphics pipeline + rendering)
 *
 * Must be called after backend initialization (VulkanBackend::initialize()).
 */
MAYAFLUX_API bool initialize(const std::shared_ptr<Core::VulkanBackend>& backend);

/**
 * @brief Shutdown Portal::Graphics subsystem
 *
 * Cleans up all managers in reverse order.
 * Safe to call multiple times.
 */
MAYAFLUX_API void shutdown();

/**
 * @brief Check if Portal::Graphics is initialized
 */
MAYAFLUX_API bool is_initialized();

} // namespace MayaFlux::Portal::Graphics
