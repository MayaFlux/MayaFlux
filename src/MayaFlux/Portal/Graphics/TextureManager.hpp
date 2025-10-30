#pragma once

#include <vulkan/vulkan.hpp>

namespace MayaFlux::Core {
class VulkanBackend;
class VKImage;
class BackendResourceManager;
}

namespace MayaFlux::Portal::Graphics {

/**
 * @enum ImageFormat
 * @brief User-friendly image format enum
 *
 * Abstracts Vulkan formats for Portal API convenience.
 * Maps to vk::Format internally.
 */
enum class ImageFormat : uint8_t {
    // Normalized formats
    R8, ///< Single channel 8-bit
    RG8, ///< Two channel 8-bit
    RGB8, ///< Three channel 8-bit
    RGBA8, ///< Four channel 8-bit
    RGBA8_SRGB, ///< Four channel 8-bit sRGB

    // Floating point formats
    R16F, ///< Single channel 16-bit float
    RG16F, ///< Two channel 16-bit float
    RGBA16F, ///< Four channel 16-bit float
    R32F, ///< Single channel 32-bit float
    RG32F, ///< Two channel 32-bit float
    RGBA32F, ///< Four channel 32-bit float

    // Depth/stencil formats
    DEPTH16, ///< 16-bit depth
    DEPTH24, ///< 24-bit depth
    DEPTH32F, ///< 32-bit float depth
    DEPTH24_STENCIL8 ///< 24-bit depth + 8-bit stencil
};

/**
 * @enum FilterMode
 * @brief Texture filtering mode
 */
enum class FilterMode : uint8_t {
    NEAREST, ///< Nearest neighbor (pixelated)
    LINEAR, ///< Bilinear filtering (smooth)
    CUBIC ///< Bicubic filtering (high quality, slower)
};

/**
 * @enum AddressMode
 * @brief Texture addressing mode (wrapping)
 */
enum class AddressMode : uint8_t {
    REPEAT, ///< Repeat texture
    MIRRORED_REPEAT, ///< Mirror and repeat
    CLAMP_TO_EDGE, ///< Clamp to edge color
    CLAMP_TO_BORDER ///< Clamp to border color
};

/**
 * @struct SamplerConfig
 * @brief Sampler configuration
 */
struct SamplerConfig {
    FilterMode mag_filter = FilterMode::LINEAR;
    FilterMode min_filter = FilterMode::LINEAR;
    AddressMode address_mode_u = AddressMode::REPEAT;
    AddressMode address_mode_v = AddressMode::REPEAT;
    AddressMode address_mode_w = AddressMode::REPEAT;
    float max_anisotropy = 1.0F; // 1.0 = disabled, 16.0 = max quality
    bool enable_mipmaps = false;
};

/**
 * @class TextureManager
 * @brief Portal-level texture creation and management
 *
 * TextureManager is the primary Portal::Graphics class for creating and
 * managing GPU textures. It bridges between user-friendly Portal API and
 * backend VKImage resources via BufferService registry.
 *
 * Key Responsibilities:
 * - Create textures (2D, 3D, cubemaps, render targets)
 * - Load textures from files (delegates to IO namespace)
 * - Manage sampler objects (filtering, addressing)
 * - Track texture lifecycle for cleanup
 * - Provide convenient format conversions
 *
 * Design Philosophy:
 * - Manages creation, NOT rendering (that's Pipeline/RenderPass)
 * - Returns VKImage directly (no wrapping yet)
 * - Simple, focused API (file loading deferred to IO)
 * - Integrates with BufferService for backend independence
 *
 * Usage:
 *   auto& mgr = Portal::Graphics::TextureManager::instance();
 *
 *   // Create basic texture
 *   auto texture = mgr.create_2d(512, 512, ImageFormat::RGBA8);
 *
 *   // Create with data
 *   std::vector<uint8_t> pixels = {...};
 *   auto texture = mgr.create_2d(512, 512, ImageFormat::RGBA8, pixels.data());
 *
 *   // Create render target
 *   auto target = mgr.create_render_target(1920, 1080);
 *
 *   // Get sampler
 *   SamplerConfig config;
 *   config.mag_filter = FilterMode::LINEAR;
 *   auto sampler = mgr.get_or_create_sampler(config);
 */
class MAYAFLUX_API TextureManager {
public:
    static TextureManager& instance()
    {
        static TextureManager manager;
        return manager;
    }

    // Non-copyable, movable
    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;
    TextureManager(TextureManager&&) noexcept = default;
    TextureManager& operator=(TextureManager&&) noexcept = default;

    /**
     * @brief Initialize texture manager
     * @param backend Shared pointer to VulkanBackend
     * @return True if initialization succeeded
     *
     * Queries BufferService from BackendRegistry.
     * Must be called before creating any textures.
     */
    bool initialize(const std::shared_ptr<Core::VulkanBackend>& backend);

    /**
     * @brief Shutdown and cleanup all textures
     *
     * Destroys all managed textures and samplers.
     * Safe to call multiple times.
     */
    void shutdown();

    /**
     * @brief Check if manager is initialized
     */
    [[nodiscard]] bool is_initialized() const { return m_backend != nullptr; }

    //==========================================================================
    // Texture Creation
    //==========================================================================

    /**
     * @brief Create a 2D texture
     * @param width Width in pixels
     * @param height Height in pixels
     * @param format Image format
     * @param data Optional initial pixel data (nullptr = empty)
     * @param mip_levels Number of mipmap levels (1 = no mipmaps)
     * @return Initialized VKImage ready for use
     *
     * Creates device-local texture optimized for shader sampling.
     * If data provided, uploads immediately and transitions to shader read layout.
     */
    std::shared_ptr<Core::VKImage> create_2d(
        uint32_t width,
        uint32_t height,
        ImageFormat format = ImageFormat::RGBA8,
        const void* data = nullptr,
        uint32_t mip_levels = 1);

    /**
     * @brief Create a 3D texture (volumetric)
     * @param width Width in pixels
     * @param height Height in pixels
     * @param depth Depth in pixels
     * @param format Image format
     * @param data Optional initial pixel data
     * @return Initialized VKImage
     */
    std::shared_ptr<Core::VKImage> create_3d(
        uint32_t width,
        uint32_t height,
        uint32_t depth,
        ImageFormat format = ImageFormat::RGBA8,
        const void* data = nullptr);

    /**
     * @brief Create a cubemap texture
     * @param size Cubemap face size in pixels (square)
     * @param format Image format
     * @param data Optional face data (6 faces in order: +X,-X,+Y,-Y,+Z,-Z)
     * @return Initialized VKImage configured as cubemap
     */
    std::shared_ptr<Core::VKImage> create_cubemap(
        uint32_t size,
        ImageFormat format = ImageFormat::RGBA8,
        const void* data = nullptr);

    /**
     * @brief Create a render target (color attachment)
     * @param width Width in pixels
     * @param height Height in pixels
     * @param format Image format (default RGBA8)
     * @return Initialized VKImage configured for rendering
     *
     * Creates image suitable for use as framebuffer color attachment.
     * Can also be sampled in shaders after rendering.
     */
    std::shared_ptr<Core::VKImage> create_render_target(
        uint32_t width,
        uint32_t height,
        ImageFormat format = ImageFormat::RGBA8);

    /**
     * @brief Create a depth buffer
     * @param width Width in pixels
     * @param height Height in pixels
     * @param with_stencil Whether to include stencil component
     * @return Initialized VKImage configured as depth/stencil attachment
     */
    std::shared_ptr<Core::VKImage> create_depth_buffer(
        uint32_t width,
        uint32_t height,
        bool with_stencil = false);

    /**
     * @brief Create a storage image (compute shader read/write)
     * @param width Width in pixels
     * @param height Height in pixels
     * @param format Image format
     * @return Initialized VKImage configured for compute storage
     */
    std::shared_ptr<Core::VKImage> create_storage_image(
        uint32_t width,
        uint32_t height,
        ImageFormat format = ImageFormat::RGBA8);

    //==========================================================================
    // Data Upload/Download
    //==========================================================================

    /**
     * @brief Upload pixel data to an existing texture
     * @param image Target image
     * @param data Pixel data pointer
     * @param size Data size in bytes
     *
     * Handles staging buffer, layout transitions, and cleanup.
     * Blocks until upload completes.
     */
    void upload_data(
        const std::shared_ptr<Core::VKImage>& image,
        const void* data,
        size_t size);

    /**
     * @brief Download pixel data from a texture
     * @param image Source image
     * @param data Destination buffer
     * @param size Buffer size in bytes
     *
     * Handles staging buffer, layout transitions, and cleanup.
     * Blocks until download completes.
     */
    void download_data(
        const std::shared_ptr<Core::VKImage>& image,
        void* data,
        size_t size);

    //==========================================================================
    // Sampler Management
    //==========================================================================

    /**
     * @brief Get or create a sampler with the given configuration
     * @param config Sampler configuration
     * @return Vulkan sampler handle (cached)
     *
     * Samplers are cached - identical configs return same sampler.
     * Managed by TextureManager, destroyed on shutdown.
     */
    vk::Sampler get_or_create_sampler(const SamplerConfig& config);

    /**
     * @brief Get a default linear sampler (for convenience)
     */
    vk::Sampler get_default_sampler();

    /**
     * @brief Get a default nearest sampler (for pixel-perfect sampling)
     */
    vk::Sampler get_nearest_sampler();

    //==========================================================================
    // Utilities
    //==========================================================================

    /**
     * @brief Convert Portal ImageFormat to Vulkan format
     */
    static vk::Format to_vulkan_format(ImageFormat format);

    /**
     * @brief Get bytes per pixel for a format
     */
    static size_t get_bytes_per_pixel(ImageFormat format);

    /**
     * @brief Calculate image data size
     */
    static size_t calculate_image_size(
        uint32_t width,
        uint32_t height,
        uint32_t depth,
        ImageFormat format);

private:
    TextureManager() = default;
    ~TextureManager() { shutdown(); }

    std::shared_ptr<Core::VulkanBackend> m_backend;
    Core::BackendResourceManager* m_resource_manager = nullptr;

    // Managed textures (for cleanup)
    std::vector<std::shared_ptr<Core::VKImage>> m_textures;

    // Sampler cache (config hash -> sampler)
    std::unordered_map<size_t, vk::Sampler> m_sampler_cache;

    // Helper: create sampler from config
    vk::Sampler create_sampler(const SamplerConfig& config);

    // Helper: hash sampler config for caching
    static size_t hash_sampler_config(const SamplerConfig& config);
};

/**
 * @brief Get the global texture manager instance
 * @return Reference to singleton texture manager
 *
 * Must call initialize() before first use.
 * Thread-safe after initialization.
 */
inline MAYAFLUX_API TextureManager& get_texture_manager()
{
    return TextureManager::instance();
}

} // namespace MayaFlux::Portal::Graphics
