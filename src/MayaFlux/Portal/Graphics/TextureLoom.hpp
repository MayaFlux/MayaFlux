#pragma once

#include <vulkan/vulkan.hpp>

#include "GraphicsUtils.hpp"
#include "ImageTarget.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKTimeline.hpp"

#include "MayaFlux/Kakshya/NDData/NDData.hpp"

namespace MayaFlux::Core {
class VulkanBackend;
class BackendResourceManager;
}

namespace MayaFlux::Buffers {
class VKBuffer;
}

namespace MayaFlux::Portal::Graphics {

using SubmitMode = Core::SubmitMode;

/**
 * @brief One layer to layer copy that scales and converts format.
 *
 * Whole images by default, stretched to fit. Rectangles are in pixels and a zero
 * extent runs to the image edge. A smaller @c dst rectangle letterboxes, a smaller
 * @c src rectangle crops, and the flips mirror the destination.
 */
struct LayerBlit {
    uint32_t src_layer {};
    uint32_t dst_layer {};
    vk::Rect2D src {};
    vk::Rect2D dst {};
    FilterMode filter { FilterMode::LINEAR };
    bool flip_x {};
    bool flip_y {};
};

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
     * With data and more than one mip level, the lower levels are generated from the
     * data, as generate_mipmaps does. Without data they hold nothing until the caller
     * fills the first level and calls generate_mipmaps.
     */
    std::shared_ptr<Core::VKImage> create_2d(
        uint32_t width,
        uint32_t height,
        ImageFormat format = ImageFormat::RGBA8,
        const void* data = nullptr,
        uint32_t mip_levels = 1);

    /**
     * @brief Allocate a 2D texture without uploading data or transitioning
     *        its layout - left VK_IMAGE_LAYOUT_UNDEFINED.
     *
     * Every create_2d*() overload above performs (and blocks on) an initial
     * layout transition or upload of its own, even when called with no
     * data - create_2d(w, h, format, nullptr) still does a blocking
     * transition to eShaderReadOnlyOptimal before returning. Use this
     * instead when the caller is about to record its own upload right
     * after (e.g. upload_data(image, data, size, staging, true) with
     * deferred=true), so the whole undefined -> transfer-dst -> copy ->
     * shader-read sequence is recorded once into a deferred command batch instead of
     * paying for a separate blocking queue.waitIdle() first.
     *
     * @param width      Width in pixels.
     * @param height     Height in pixels.
     * @param format     Image format (default RGBA8).
     * @param mip_levels Number of mipmap levels (1 = no mipmaps).
     * @return           VKImage with device memory allocated, still in
     *                   VK_IMAGE_LAYOUT_UNDEFINED, or nullptr on failure.
     *                   The caller owns transitioning it before use.
     */
    [[nodiscard]] std::shared_ptr<Core::VKImage> create_2d_uninitialized(
        uint32_t width,
        uint32_t height,
        ImageFormat format = ImageFormat::RGBA8,
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
     * @brief Create a 2D texture array.
     *
     * All layers share the same width, height, and format. The image view
     * type is VK_IMAGE_VIEW_TYPE_2D_ARRAY, making it bindable as
     * sampler2DArray in GLSL.
     *
     * @param width   Width in pixels per layer.
     * @param height  Height in pixels per layer.
     * @param layers  Number of array layers. Must be > 0.
     * @param format  Image format (default RGBA8).
     * @param data    Optional pixel data for all layers concatenated in order
     *                (layer 0 first). Must be exactly
     *                width * height * bpp(format) * layers bytes when non-null.
     *                Pass nullptr to allocate without uploading; the image is
     *                transitioned to eShaderReadOnlyOptimal.
     * @return Initialised VKImage with array_layers == layers, or nullptr on failure.
     */
    std::shared_ptr<Core::VKImage> create_2d_array(
        uint32_t width,
        uint32_t height,
        uint32_t layers,
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
     *
     * @param storage Also let compute passes write it as a storage image. The format has to allow it.
     * @param layers Array layers. More than one makes a 2D array, and cannot have mip levels.
     * @param cube Six square faces that sample as a cube, in place of layers. Cannot have mip levels.
     * @return The image, or nullptr when it cannot be made as asked.
     */
    std::shared_ptr<Core::VKImage> create_render_target(
        uint32_t width,
        uint32_t height,
        ImageFormat format = ImageFormat::RGBA8,
        uint32_t mip_levels = 1,
        bool storage = false,
        uint32_t layers = 1,
        bool cube = false);

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
     * @brief Create an image that draws land in instead of a window
     * @param config Size, format, clear color, optional depth and redraw policy
     * @return The target, or nullptr if Loom is not initialized or the size is zero
     *
     * The color image rests in shader read layout, so a buffer can bind it as a
     * texture. The target releases its images when its last holder lets go.
     */
    [[nodiscard]] std::shared_ptr<ImageTarget> create_image_target(const ImageTarget::Config& config);

    /**
     * @brief A 2D view of the first mip level of one layer of an image, as a draw attachment takes it
     * @return The view, or a null handle when the image is not managed by this Loom
     */
    [[nodiscard]] vk::ImageView attachment_view(const std::shared_ptr<Core::VKImage>& image, uint32_t layer = 0) const;

    /**
     * @brief Reuse a matching image in an owner-held entry or create it once.
     * @param entry Cache entry retained by the image producer.
     * @param key Immutable image allocation properties.
     * @return Matching image, or nullptr on failure.
     */
    [[nodiscard]] std::shared_ptr<Core::VKImage> acquire_cached_image(
        ImageCacheEntry& entry, const ImageKey& key);

    /**
     * @brief Refresh a sampled image in an owner-held cache entry.
     * @param entry Cache entry retained by the image producer.
     * @param key Sampled image allocation properties.
     * @param pixels Complete pixel bytes for every layer.
     * @return Refreshed image, or nullptr when validation or creation fails.
     */
    [[nodiscard]] std::shared_ptr<Core::VKImage> refresh_cached_image(
        ImageCacheEntry& entry,
        const ImageKey& key,
        std::span<const uint8_t> pixels);

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
     * @brief Upload pixels to an image.
     * Immediate uploads use host image copy. Deferred uploads require staging.
     */
    void upload_data(
        const std::shared_ptr<Core::VKImage>& image,
        const void* data,
        size_t size,
        const std::shared_ptr<Buffers::VKBuffer>& staging = nullptr,
        bool deferred = false);

    /**
     * @brief Download pixels from an image into host memory.
     * @param restore_layout Layout to restore after the copy.
     * @param restore_stage Pipeline stage that consumes the restored image.
     */
    void download_data(
        const std::shared_ptr<Core::VKImage>& image,
        void* data,
        size_t size,
        vk::ImageLayout restore_layout = vk::ImageLayout::eShaderReadOnlyOptimal,
        vk::PipelineStageFlags2 restore_stage = vk::PipelineStageFlagBits2::eFragmentShader);

    /**
     * @brief Download the base level of an image as typed pixels.
     *
     * Blocking. The element type of the returned storage follows the format:
     * one byte per channel gives uint8, two gives uint16, four gives float.
     *
     * @param image          Source image. Must satisfy readable_format().
     * @param restore_layout Layout the image is left in.
     * @param restore_stage  Pipeline stage that consumes the image after restore.
     * @return The pixels with width, height, channels and format set, or
     *         nullopt, with an error logged, if the image is not readable or
     *         the loom is not initialised.
     */
    [[nodiscard]] std::optional<Kakshya::ImageData> download_image(
        const std::shared_ptr<Core::VKImage>& image,
        vk::ImageLayout restore_layout = vk::ImageLayout::eShaderReadOnlyOptimal,
        vk::PipelineStageFlags2 restore_stage = vk::PipelineStageFlagBits2::eFragmentShader);

    /**
     * @brief Transition every mip level and layer of a VKImage to a new layout.
     * @param image      Image to transition. Must be initialised.
     * @param new_layout Target layout.
     * @param mode       DEFERRED records the barrier for the next submission. ASYNC
     *                   submits it at once without waiting. IMMEDIATE waits for it.
     *
     * Later GPU work and host image copies are ordered after the transition in
     * every mode, and the image's tracked layout changes at once.
     */
    void transition_layout(
        const std::shared_ptr<Core::VKImage>& image,
        vk::ImageLayout new_layout,
        SubmitMode mode = SubmitMode::DEFERRED);

    /**
     * @brief Upload pixels into one layer of an array image.
     *
     * The other layers keep their contents. @p size must be one layer's byte count.
     *
     * @param image    Array image.
     * @param layer    Layer index.
     * @param data     Pixels of one layer.
     * @param size     Byte count of one layer.
     * @return True if the upload completed.
     */
    bool upload_layer(
        const std::shared_ptr<Core::VKImage>& image,
        uint32_t layer,
        const void* data,
        size_t size);

    /**
     * @brief Copy a layer of one image into a layer of another, scaling to fit.
     *
     * Works between any two color images, so it writes GPU images into an array
     * layer without a host read, and also converts between supported formats. A
     * cubic filter is not available and falls back to linear.
     *
     * @param src      Source image, distinct from @p dst.
     * @param dst      Destination image.
     * @param blit     Layers, rectangles, filter and flips.
     * @param mode     IMMEDIATE waits for the copy. DEFERRED records it for the next flush
     *                 or present submit. ASYNC submits it at once without waiting; later
     *                 graphics work, including a read of the layer, is ordered after it.
     * @return True if the blit was recorded, executed or submitted.
     */
    bool blit_layer(
        const std::shared_ptr<Core::VKImage>& src,
        const std::shared_ptr<Core::VKImage>& dst,
        const LayerBlit& blit = {},
        SubmitMode mode = SubmitMode::IMMEDIATE);

    /**
     * @brief Whether the device can blit between two images with a filter.
     */
    [[nodiscard]] bool can_blit(
        const std::shared_ptr<Core::VKImage>& src,
        const std::shared_ptr<Core::VKImage>& dst,
        FilterMode filter = FilterMode::LINEAR) const;

    /**
     * @brief Fill every mip level below the first from the level above it.
     *
     * The first level must already hold the picture, whether uploaded or drawn
     * into. The other levels are overwritten. Use it after rendering into an image
     * created with more than one mip level, since nothing generates them on its own.
     *
     * @param image  Color image created with more than one mip level.
     * @param filter Nearest or linear; a cubic filter is not available and falls back to linear.
     * @param mode   IMMEDIATE waits for the chain. DEFERRED records it for the next flush
     *               or present submit. ASYNC submits it at once without waiting.
     * @return True if the chain was recorded, executed or submitted. False for an image
     *         with one level, a depth format, or a format that cannot blit with the filter.
     */
    bool generate_mipmaps(
        const std::shared_ptr<Core::VKImage>& image,
        FilterMode filter = FilterMode::LINEAR,
        SubmitMode mode = SubmitMode::IMMEDIATE);

    //==========================================================================
    // Cleanup
    //==========================================================================

    /**
     * @brief Release an image's GPU resources and stop retaining it in the Loom.
     *
     * Recorded library uses retain managed allocations until completion. The
     * caller must ensure that no consumer intends to record another use.
     * Raw handle users must synchronize independently. Images not retained by
     * the Loom are also cleaned up.
     */
    void clear_image_residency(const std::shared_ptr<Core::VKImage>& image);

    /**
     * @brief Add a holder to an image created by the Loom.
     *
     * A created image starts with one holder, the creator. Each receiver that
     * keeps the image retains it and releases it when it replaces or drops it.
     * Images the Loom did not create are ignored.
     */
    void retain_image(const std::shared_ptr<Core::VKImage>& image);

    /**
     * @brief Drop one holder; the last release clears the image's residency.
     *
     * Null and untracked images are ignored.
     */
    void release_image(const std::shared_ptr<Core::VKImage>& image);

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
     * @brief ImageFormat of an image that can be read back as one flat block.
     *
     * Requires an initialised 2D image of one layer and depth 1 with a
     * transfer-source usage, a format with an ImageFormat mapping that is not
     * combined depth-stencil, and a nonzero base level that fits the image's
     * allocation. The base level's byte count is
     * calculate_image_size(width, height, 1, format).
     *
     * Checks structure only: it says nothing about the image's current layout
     * or whether anything has been written to it.
     *
     * @return The format, or nullopt when any requirement is not met.
     */
    [[nodiscard]] static std::optional<ImageFormat> readable_format(
        const std::shared_ptr<Core::VKImage>& image);

    /**
     * @brief ImageFormat of an image that can be written as one flat block.
     *
     * The mirror of readable_format() with transfer-destination usage in
     * place of transfer-source: an initialised single-layer 2D image of depth 1,
     * a mapped format that is not combined depth-stencil, and a nonzero base
     * level that fits the image's allocation.
     *
     * @return The format, or nullopt when any requirement is not met.
     */
    [[nodiscard]] static std::optional<ImageFormat> writable_format(
        const std::shared_ptr<Core::VKImage>& image);

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
    static std::optional<ImageFormat> transfer_format(
        const std::shared_ptr<Core::VKImage>& image,
        vk::ImageUsageFlagBits required_usage);

    TextureLoom() = default;
    ~TextureLoom() { shutdown(); }

    std::shared_ptr<Core::VulkanBackend> m_backend;
    Core::BackendResourceManager* m_resource_manager = nullptr;

    void track(const std::shared_ptr<Core::VKImage>& image);

    // Managed textures (for cleanup)
    std::vector<std::shared_ptr<Core::VKImage>> m_textures;
    std::unordered_map<const Core::VKImage*, uint32_t> m_holders;

    static bool s_initialized;
};

/**
 * @brief Cache images keyed by immutable allocation properties.
 *
 * The byte budget is a warning threshold. Entries remain cached after the
 * threshold is exceeded because TextureLoom retains created images until
 * shutdown and eviction would recreate them on a later request.
 */
class MAYAFLUX_API ImageCacheSet {
public:
    explicit ImageCacheSet(size_t byte_budget)
        : m_byte_budget(byte_budget)
    {
    }

    ImageCacheSet(const ImageCacheSet&) = delete;
    ImageCacheSet& operator=(const ImageCacheSet&) = delete;
    ImageCacheSet(ImageCacheSet&&) noexcept = default;
    ImageCacheSet& operator=(ImageCacheSet&&) noexcept = default;
    ~ImageCacheSet() = default;

    [[nodiscard]] std::shared_ptr<Core::VKImage> acquire(
        TextureLoom& loom, const ImageKey& key);

private:
    struct Entry {
        ImageCacheEntry cache;
    };

    std::unordered_map<ImageKey, Entry, ImageKeyHash> m_entries;
    size_t m_byte_budget;
    size_t m_cached_bytes {};
    bool m_budget_warned {};
    std::optional<ImageKey> m_last_key;
    Entry* m_last_entry {};
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
