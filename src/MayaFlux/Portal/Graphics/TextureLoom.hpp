#pragma once

#include <vulkan/vulkan.hpp>

#include "GraphicsUtils.hpp"

#include "MayaFlux/Kakshya/NDData/NDData.hpp"

namespace MayaFlux::Core {
class VulkanBackend;
class BackendResourceManager;
}

namespace MayaFlux::Buffers {
class VKBuffer;
}

namespace MayaFlux::Portal::Graphics {

/**
 * @class TextureLoom
 * @brief Portal-level texture creation and management
 *
 * TextureLoom is the primary Portal::Graphics class for creating and
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
 *   auto& mgr = Portal::Graphics::TextureLoom::instance();
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
class MAYAFLUX_API TextureLoom {
public:
    static TextureLoom& instance()
    {
        static TextureLoom manager;
        return manager;
    }

    // Non-copyable, movable
    TextureLoom(const TextureLoom&) = delete;
    TextureLoom& operator=(const TextureLoom&) = delete;
    TextureLoom(TextureLoom&&) noexcept = default;
    TextureLoom& operator=(TextureLoom&&) noexcept = default;

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

    /**
     * @brief Create a 2D texture from a DataVariant in one shot.
     * @param variant  Source data. Conversion and validation delegated to
     *                 Kakshya::as_texture_access(): vec3 is promoted to vec4
     *                 with W=0 (warned); complex<double> and mat4 are rejected.
     * @param width    Texture width in texels.
     * @param height   Texture height in texels.
     * @param format   Target image format (default: RGBA32F).
     * @return         Initialised VKImage in shader-read layout, or nullptr on failure.
     *
     * Validates byte count against width * height * bpp(format) before upload.
     * Throws std::invalid_argument on mismatch.
     */
    [[nodiscard]] std::shared_ptr<Core::VKImage> create_2d(
        const Kakshya::DataVariant& variant,
        uint32_t width,
        uint32_t height,
        ImageFormat format = ImageFormat::RGBA32F);

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
     * @brief Upload pixel data reusing a caller-supplied persistent staging buffer.
     *        Identical to upload_data() but skips the per-call VkBuffer allocation,
     *        eliminating the Vulkan object churn that causes VK_ERROR_DEVICE_LOST
     *        under sustained per-frame texture updates (e.g. video playback).
     * @param image   Target VKImage (must already be initialised).
     * @param data    Pixel data pointer (at least @p size bytes).
     * @param size    Byte count — must match the image footprint.
     * @param staging Host-visible staging VKBuffer from create_streaming_staging().
     */
    void upload_data(
        const std::shared_ptr<Core::VKImage>& image,
        const void* data,
        size_t size,
        const std::shared_ptr<Buffers::VKBuffer>& staging);

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

    /**
     * @brief Download pixel data from a texture without blocking the
     *        graphics queue.
     *
     * Allocates a per-call staging buffer, command buffer, and fence.
     * Records a copy-image-to-buffer op, submits with the fence, and
     * blocks the calling thread on vkWaitForFences until the copy
     * completes. Unlike download_data, this does not call queue.waitIdle,
     * so other graphics work proceeds concurrently.
     *
     * Intended to be called from a worker thread (e.g. via std::async).
     * Calling from the graphics thread or any thread that must not block
     * will stall that thread for the duration of the copy.
     *
     * @param image Source image.
     * @param data  Destination host pointer, at least @p size bytes.
     * @param size  Byte count to read.
     */
    void download_data_async(
        const std::shared_ptr<Core::VKImage>& image,
        void* data,
        size_t size);

    /**
     * @brief Transition a VKImage to a new Vulkan layout via an immediate submission.
     * @param image       Image to transition. Must be initialised.
     * @param old_layout  Current layout of the image.
     * @param new_layout  Target layout.
     * @param mip_levels  Number of mip levels covered by the transition.
     * @param array_layers Number of array layers covered by the transition.
     * @param aspect_mask Image aspect flags (colour, depth, stencil).
     *
     * Delegates to BackendResourceManager and updates the image's tracked layout.
     * Use before binding an image to a compute shader descriptor or before
     * upload/download operations that require a specific layout.
     */
    void transition_layout(
        const std::shared_ptr<Core::VKImage>& image,
        vk::ImageLayout old_layout,
        vk::ImageLayout new_layout,
        uint32_t mip_levels = 1,
        uint32_t array_layers = 1,
        vk::ImageAspectFlags aspect_mask = vk::ImageAspectFlagBits::eColor);

    //==========================================================================
    // Sampler Management
    //==========================================================================

    /**
     * @brief Get or create a sampler with the given configuration
     * @param config Sampler configuration
     * @return Vulkan sampler handle (cached)
     *
     * Samplers are cached - identical configs return same sampler.
     * Managed by TextureLoom, destroyed on shutdown.
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
     * @brief Convert Vulkan format to Portal ImageFormat.
     *
     * Reverse twin of to_vulkan_format. Returns std::nullopt for Vulkan
     * formats with no ImageFormat equivalent rather than guessing.
     */
    static std::optional<ImageFormat> from_vulkan_format(vk::Format vk_format);

    /**
     * @brief Calculate image data size
     */
    static size_t calculate_image_size(
        uint32_t width,
        uint32_t height,
        uint32_t depth,
        ImageFormat format);

    /**
     * @brief Get the number of color channels for a given format
     */
    static uint32_t get_channel_count(ImageFormat format);

private:
    TextureLoom() = default;
    ~TextureLoom() { shutdown(); }

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

    static bool s_initialized;
};

/**
 * @brief Get the global texture manager instance
 * @return Reference to singleton texture manager
 *
 * Must call initialize() before first use.
 * Thread-safe after initialization.
 */
inline MAYAFLUX_API TextureLoom& get_texture_manager()
{
    return TextureLoom::instance();
}

} // namespace MayaFlux::Portal::Graphics
