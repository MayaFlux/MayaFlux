#pragma once

namespace MayaFlux::Core {
class VulkanBackend;
}

namespace MayaFlux::Portal::Graphics {

/**
 * @brief Initialize Portal::Graphics Glue Layer
 * @return True if initialization succeeded
 *
 * Initializes all Portal::Graphics managers:
 * - TextureLoom
 * - SamplerFactory
 * - (Future) ShaderCompiler
 * - (Future) PipelineFactory
 *
 * Must be called after backend initialization (VulkanBackend::initialize()).
 *
 * Example:
 *   // In engine initialization
 *   vulkan_backend.initialize();
 *   Portal::Graphics::initialize(vulkan_backend);
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
