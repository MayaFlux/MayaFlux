#pragma once

#include <vulkan/vulkan.hpp>

#include "TextureLoom.hpp"

namespace MayaFlux::Core {
class VulkanBackend;
}

namespace MayaFlux::Portal::Graphics {

enum class FilterMode : uint8_t;
enum class AddressMode : uint8_t;
enum class BorderColor : uint8_t;
struct SamplerConfig;

/**
 * @brief One fit expressed for both ways of applying it.
 *
 * @c blits copies the source into the destination when the fit is written into an
 * image, with layers left at zero for the caller to set. @c sampler is the
 * addressing that gives the same result when a shader samples the source directly;
 * the scale and offset of that sampling follow from the rectangles in @c blits.
 */
struct FitPlan {
    std::vector<LayerBlit> blits;
    SamplerConfig sampler;
};

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

    /**
     * @brief Work out how a source extent maps onto a destination extent.
     *
     * Pure arithmetic, no device needed. Zero in any extent yields an empty plan.
     * Tiling produces one blit per tile, so a source much smaller than the
     * destination produces many.
     *
     * @param src_width  Source width in pixels.
     * @param src_height Source height in pixels.
     * @param dst_width  Destination width in pixels.
     * @param dst_height Destination height in pixels.
     * @param mode       How the source is fitted.
     * @param filter     Filter for the blits and the sampler.
     * @return Rectangles for an image write and the matching sampler.
     */
    [[nodiscard]] static FitPlan plan_fit(
        uint32_t src_width,
        uint32_t src_height,
        uint32_t dst_width,
        uint32_t dst_height,
        FitMode mode = FitMode::STRETCH,
        FilterMode filter = FilterMode::LINEAR);

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

    // Helper: Convert BorderColor to Vulkan border color
    static vk::BorderColor to_vk_border_color(BorderColor color);

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
