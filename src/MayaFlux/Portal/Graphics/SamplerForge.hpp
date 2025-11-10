#pragma once

#include <vulkan/vulkan.hpp>

namespace MayaFlux::Core {
class VulkanBackend;
}

namespace MayaFlux::Portal::Graphics {

enum class FilterMode : uint8_t;
enum class AddressMode : uint8_t;
struct SamplerConfig;

/**
 * @class SamplerForge
 * @brief Creates and caches Vulkan samplers (Singleton)
 *
 * Samplers control how textures are filtered and addressed when sampled in shaders.
 * This factory caches samplers based on configuration to avoid creating duplicates.
 *
 * Lifecycle:
 * - Initialize with backend reference
 * - Create samplers via get_or_create()
 * - Samplers are cached and reused
 * - Cleanup destroys all samplers
 *
 * Thread-safe after initialization.
 */
class MAYAFLUX_API SamplerForge {
public:
    static SamplerForge& instance()
    {
        static SamplerForge factory;
        return factory;
    }

    SamplerForge(const SamplerForge&) = delete;
    SamplerForge& operator=(const SamplerForge&) = delete;
    SamplerForge(SamplerForge&&) = delete;
    SamplerForge& operator=(SamplerForge&&) = delete;

    /**
     * @brief Initialize with backend reference
     * @param backend Vulkan backend instance
     * @return True if initialization succeeded
     */
    bool initialize(const std::shared_ptr<Core::VulkanBackend>& backend);

    /**
     * @brief Shutdown and cleanup all samplers
     */
    void shutdown();

    /**
     * @brief Check if factory is initialized
     */
    [[nodiscard]] bool is_initialized() const { return m_backend != nullptr; }

    /**
     * @brief Get or create a sampler with the given configuration
     * @param config Sampler configuration
     * @return Vulkan sampler handle (cached)
     *
     * Samplers are cached - identical configs return the same sampler.
     * All samplers are destroyed on shutdown.
     */
    vk::Sampler get_or_create(const SamplerConfig& config);

    /**
     * @brief Get a default linear sampler
     * @return Sampler with linear filtering and repeat addressing
     */
    vk::Sampler get_default_linear();

    /**
     * @brief Get a default nearest sampler
     * @return Sampler with nearest filtering and clamp-to-edge addressing
     */
    vk::Sampler get_default_nearest();

    /**
     * @brief Get an anisotropic sampler (high quality)
     * @param max_anisotropy Maximum anisotropy level (1.0-16.0)
     * @return Sampler with anisotropic filtering
     */
    vk::Sampler get_anisotropic(float max_anisotropy = 16.0F);

    /**
     * @brief Destroy a specific sampler
     * @param sampler Sampler to destroy
     *
     * Removes from cache and destroys. Useful for hot-reloading.
     */
    void destroy_sampler(vk::Sampler sampler);

    /**
     * @brief Get number of cached samplers
     */
    [[nodiscard]] size_t get_sampler_count() const { return m_sampler_cache.size(); }

private:
    SamplerForge() = default;
    ~SamplerForge() { shutdown(); }

    std::shared_ptr<Core::VulkanBackend> m_backend;

    // Sampler cache (config hash -> sampler)
    std::unordered_map<size_t, vk::Sampler> m_sampler_cache;

    // Helper: Create sampler from config
    vk::Sampler create_sampler(const SamplerConfig& config);

    // Helper: Hash sampler config for caching
    static size_t hash_config(const SamplerConfig& config);

    // Helper: Convert FilterMode to Vulkan filter
    static vk::Filter to_vk_filter(FilterMode mode);

    // Helper: Convert AddressMode to Vulkan address mode
    static vk::SamplerAddressMode to_vk_address_mode(AddressMode mode);

    static bool s_initialized;
};

/**
 * @brief Convenience wrapper around SamplerForge::instance()
 */
inline SamplerForge& get_sampler_factory()
{
    return SamplerForge::instance();
}

} // namespace MayaFlux::Portal::Graphics
