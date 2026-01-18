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
 * @brief Stop all Portal::Graphics operations
 *
 * Frees command buffers and stops active rendering.
 * Call this AFTER graphics thread stops, BEFORE destroying resources.
 */
MAYAFLUX_API void stop();

/**
 * @brief Shutdown Portal::Graphics subsystem
 *
 * Destroys all pipelines, shaders, and resources.
 * Must be called AFTER stop().
 */
MAYAFLUX_API void shutdown();

/**
 * @brief Check if Portal::Graphics is initialized
 */
MAYAFLUX_API bool is_initialized();

} // namespace MayaFlux::Portal::Graphics
