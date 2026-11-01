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

    struct ConfigHash {
        size_t operator()(const SamplerConfig& config) const;
    };

    struct ConfigEqual {
        bool operator()(const SamplerConfig& a, const SamplerConfig& b) const;
    };

    std::unordered_map<SamplerConfig, vk::Sampler, ConfigHash, ConfigEqual> m_sampler_cache;

    vk::Sampler create_sampler(const SamplerConfig& config);

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

/**
 * @brief SamplerConfig values named by what they do at a texture's limits.
 *
 * Each returns a plain config to pass to SamplerForge::get_or_create or to a
 * SampledTexture, and to adjust further. The filter chooses how a texel is read, the
 * addressing what a coordinate outside the image reads, and the border its colour.
 * Mipmaps stay off: no mip chain is generated.
 */
namespace SamplerPresets {

    /** @brief Exact texels, no blending; outside the image the edge texel continues. For maps, masks and indices. */
    [[nodiscard]] inline SamplerConfig nearest_clamped()
    {
        return { .mag_filter = FilterMode::NEAREST, .min_filter = FilterMode::NEAREST,
            .address_mode_u = AddressMode::CLAMP_TO_EDGE, .address_mode_v = AddressMode::CLAMP_TO_EDGE,
            .address_mode_w = AddressMode::CLAMP_TO_EDGE };
    }

    /** @brief Linear blending; outside the image the edge texel continues. For gradient lookups. */
    [[nodiscard]] inline SamplerConfig linear_clamped()
    {
        return { .address_mode_u = AddressMode::CLAMP_TO_EDGE, .address_mode_v = AddressMode::CLAMP_TO_EDGE,
            .address_mode_w = AddressMode::CLAMP_TO_EDGE };
    }

    /** @brief The image repeats in every direction. For tiling and wrapping lookups. */
    [[nodiscard]] inline SamplerConfig repeating(FilterMode filter = FilterMode::LINEAR)
    {
        return { .mag_filter = filter, .min_filter = filter };
    }

    /** @brief The image repeats, every other copy mirrored. Continuous across the seams. */
    [[nodiscard]] inline SamplerConfig mirrored(FilterMode filter = FilterMode::LINEAR)
    {
        return { .mag_filter = filter, .min_filter = filter,
            .address_mode_u = AddressMode::MIRRORED_REPEAT, .address_mode_v = AddressMode::MIRRORED_REPEAT,
            .address_mode_w = AddressMode::MIRRORED_REPEAT };
    }

    /** @brief Outside the image reads a fixed colour. */
    [[nodiscard]] inline SamplerConfig bordered(BorderColor color, FilterMode filter = FilterMode::LINEAR)
    {
        return { .mag_filter = filter, .min_filter = filter,
            .address_mode_u = AddressMode::CLAMP_TO_BORDER, .address_mode_v = AddressMode::CLAMP_TO_BORDER,
            .address_mode_w = AddressMode::CLAMP_TO_BORDER, .border_color = color };
    }

    /** @brief Each axis addressed on its own, for surfaces that wrap one way and stop the other. */
    [[nodiscard]] inline SamplerConfig axes(
        AddressMode u,
        AddressMode v,
        FilterMode filter = FilterMode::LINEAR,
        AddressMode w = AddressMode::CLAMP_TO_EDGE)
    {
        return { .mag_filter = filter, .min_filter = filter,
            .address_mode_u = u, .address_mode_v = v, .address_mode_w = w };
    }

    /** @brief Magnified texels stay hard, minified ones are blended. */
    [[nodiscard]] inline SamplerConfig magnify_nearest()
    {
        return { .mag_filter = FilterMode::NEAREST, .min_filter = FilterMode::LINEAR,
            .address_mode_u = AddressMode::CLAMP_TO_EDGE, .address_mode_v = AddressMode::CLAMP_TO_EDGE,
            .address_mode_w = AddressMode::CLAMP_TO_EDGE };
    }

    /** @brief Magnified texels are blended, minified ones are not, so shrinking aliases. */
    [[nodiscard]] inline SamplerConfig minify_nearest()
    {
        return { .mag_filter = FilterMode::LINEAR, .min_filter = FilterMode::NEAREST,
            .address_mode_u = AddressMode::CLAMP_TO_EDGE, .address_mode_v = AddressMode::CLAMP_TO_EDGE,
            .address_mode_w = AddressMode::CLAMP_TO_EDGE };
    }

    /** @brief The depth axis of a 3D image repeats; width and height clamp. Ignored by array layers. */
    [[nodiscard]] inline SamplerConfig repeating_depth(FilterMode filter = FilterMode::LINEAR)
    {
        return { .mag_filter = filter, .min_filter = filter,
            .address_mode_u = AddressMode::CLAMP_TO_EDGE, .address_mode_v = AddressMode::CLAMP_TO_EDGE,
            .address_mode_w = AddressMode::REPEAT };
    }

    /** @brief Linear, repeating, with anisotropic filtering clamped to 1 to 16, for surfaces seen at a slant. */
    [[nodiscard]] inline SamplerConfig oblique(float anisotropy = 16.0F)
    {
        return { .max_anisotropy = std::clamp(anisotropy, 1.0F, 16.0F) };
    }

} // namespace SamplerPresets

} // namespace MayaFlux::Portal::Graphics
