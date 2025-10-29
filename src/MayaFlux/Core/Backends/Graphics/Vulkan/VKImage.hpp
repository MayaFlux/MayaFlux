#pragma once

#include "MayaFlux/Kakshya/NDData/NDData.hpp"
#include <vulkan/vulkan.hpp>

namespace MayaFlux::Core {

/**
 * @struct VKImageResources
 * @brief Vulkan image resource handles
 *
 * Stores all Vulkan handles associated with an image.
 * Set by backend after allocation.
 */
struct VKImageResources {
    vk::Image image;
    vk::ImageView image_view;
    vk::DeviceMemory memory;
    vk::Sampler sampler; // Optional, can be null
};

/**
 * @class VKImage
 * @brief Lightweight Vulkan image wrapper for MayaFlux processing pipeline
 *
 * VKImage is a backend-level representation of a GPU image/texture. Like VKBuffer,
 * it is a semantic container that stores metadata and Vulkan handles but does not
 * perform allocation itself. The backend (VulkanBackend) handles actual resource
 * creation via BufferService-style patterns.
 *
 * Responsibilities:
 * - Store image dimensions, format, usage intent, and semantic modality
 * - Provide inferred data dimensions for processors and pipeline inspection
 * - Hold Vulkan handles (VkImage, VkImageView, VkDeviceMemory) assigned by backend
 * - Provide convenience helpers for Vulkan creation flags and memory properties
 * - Track current layout for automatic layout transitions
 *
 * Does NOT handle:
 * - Actual Vulkan allocation (that's VulkanBackend)
 * - Layout transitions (that's command buffer operations)
 * - Descriptor set binding (that's VKDescriptorManager)
 *
 * Integration points:
 * - Created by Portal::Graphics::TextureManager
 * - Allocated by VulkanBackend::initialize_image()
 * - Wrapped by Kakshya::Visual::TextureStream (future)
 * - Used by Yantra::Graphics::RenderPipeline (future)
 */
class MAYAFLUX_API VKImage {
public:
    enum class Usage : uint8_t {
        TEXTURE_2D, ///< Sampled texture (shader read)
        RENDER_TARGET, ///< Color attachment for rendering
        DEPTH_STENCIL, ///< Depth/stencil attachment
        STORAGE, ///< Storage image (compute shader read/write)
        TRANSFER_SRC, ///< Transfer source
        TRANSFER_DST, ///< Transfer destination
        STAGING ///< Host-visible staging image (rare)
    };

    enum class Type : uint8_t {
        TYPE_1D,
        TYPE_2D,
        TYPE_3D,
        TYPE_CUBE
    };

    /**
     * @brief Construct an uninitialized VKImage
     *
     * Creates a VKImage object with requested parameters. No Vulkan resources
     * are created by this constructor - registration with the backend is required.
     *
     * @param width Image width in pixels
     * @param height Image height in pixels
     * @param depth Image depth (for 3D textures, 1 for 2D)
     * @param format Vulkan image format
     * @param usage Intended usage pattern
     * @param type Image dimensionality
     * @param mip_levels Number of mipmap levels (1 for no mipmaps)
     * @param array_layers Number of array layers (1 for single image, 6 for cubemap)
     * @param modality Semantic interpretation of image contents
     */
    VKImage(
        uint32_t width,
        uint32_t height,
        uint32_t depth,
        vk::Format format,
        Usage usage,
        Type type = Type::TYPE_2D,
        uint32_t mip_levels = 1,
        uint32_t array_layers = 1,
        Kakshya::DataModality modality = Kakshya::DataModality::IMAGE_COLOR);

    VKImage() = default;
    ~VKImage() = default;

    VKImage(const VKImage&) = delete;
    VKImage& operator=(const VKImage&) = delete;
    VKImage(VKImage&&) noexcept = default;
    VKImage& operator=(VKImage&&) noexcept = default;

    //==========================================================================
    // Vulkan Handle Access
    //==========================================================================

    /** Get VkImage handle (VK_NULL_HANDLE if not initialized) */
    [[nodiscard]] vk::Image get_image() const { return m_resources.image; }

    /** Get VkImageView handle (VK_NULL_HANDLE if not initialized) */
    [[nodiscard]] vk::ImageView get_image_view() const { return m_resources.image_view; }

    /** Get VkDeviceMemory handle (VK_NULL_HANDLE if not initialized) */
    [[nodiscard]] vk::DeviceMemory get_memory() const { return m_resources.memory; }

    /** Get VkSampler handle (VK_NULL_HANDLE if none assigned) */
    [[nodiscard]] vk::Sampler get_sampler() const { return m_resources.sampler; }

    /** Get all image resources at once */
    [[nodiscard]] const VKImageResources& get_image_resources() const { return m_resources; }

    /** Set VkImage handle after backend allocation */
    void set_image(vk::Image image) { m_resources.image = image; }

    /** Set VkImageView handle after backend allocation */
    void set_image_view(vk::ImageView view) { m_resources.image_view = view; }

    /** Set VkDeviceMemory handle after backend allocation */
    void set_memory(vk::DeviceMemory memory) { m_resources.memory = memory; }

    /** Set VkSampler handle (optional) */
    void set_sampler(vk::Sampler sampler) { m_resources.sampler = sampler; }

    /** Set all image resources at once */
    void set_image_resources(const VKImageResources& resources) { m_resources = resources; }

    /** Check whether Vulkan handles are present (image registered) */
    [[nodiscard]] bool is_initialized() const { return m_resources.image != VK_NULL_HANDLE; }

    //==========================================================================
    // Image Properties
    //==========================================================================

    [[nodiscard]] uint32_t get_width() const { return m_width; }
    [[nodiscard]] uint32_t get_height() const { return m_height; }
    [[nodiscard]] uint32_t get_depth() const { return m_depth; }
    [[nodiscard]] vk::Format get_format() const { return m_format; }
    [[nodiscard]] Usage get_usage() const { return m_usage; }
    [[nodiscard]] Type get_type() const { return m_type; }
    [[nodiscard]] uint32_t get_mip_levels() const { return m_mip_levels; }
    [[nodiscard]] uint32_t get_array_layers() const { return m_array_layers; }
    [[nodiscard]] Kakshya::DataModality get_modality() const { return m_modality; }

    /** Get the inferred data dimensions for the image contents */
    [[nodiscard]] const std::vector<Kakshya::DataDimension>& get_dimensions() const { return m_dimensions; }

    /** Update the semantic modality and re-infer dimensions */
    void set_modality(Kakshya::DataModality modality);

    //==========================================================================
    // Layout Tracking
    //==========================================================================

    /** Get current image layout (for synchronization) */
    [[nodiscard]] vk::ImageLayout get_current_layout() const { return m_current_layout; }

    /** Set current layout (called after transitions) */
    void set_current_layout(vk::ImageLayout layout) { m_current_layout = layout; }

    //==========================================================================
    // Vulkan Creation Helpers (for backend use)
    //==========================================================================

    /**
     * @brief Get appropriate VkImageUsageFlags based on Usage
     * @return VkImageUsageFlags to use when creating VkImage
     */
    [[nodiscard]] vk::ImageUsageFlags get_usage_flags() const;

    /**
     * @brief Get appropriate VkMemoryPropertyFlags based on Usage
     * @return VkMemoryPropertyFlags for memory allocation
     */
    [[nodiscard]] vk::MemoryPropertyFlags get_memory_properties() const;

    /**
     * @brief Get appropriate VkImageAspectFlags based on format
     * @return VkImageAspectFlags for image view creation
     */
    [[nodiscard]] vk::ImageAspectFlags get_aspect_flags() const;

    /**
     * @brief Whether this image should be host-visible (staging images)
     * @return True for staging images, false for device-local
     */
    [[nodiscard]] bool is_host_visible() const { return m_usage == Usage::STAGING; }

    /**
     * @brief Get total size in bytes (for memory allocation)
     * @return Estimated size in bytes
     */
    [[nodiscard]] size_t get_size_bytes() const;

private:
    VKImageResources m_resources;

    uint32_t m_width { 0 };
    uint32_t m_height { 0 };
    uint32_t m_depth { 1 };
    vk::Format m_format { vk::Format::eUndefined };
    Usage m_usage { Usage::TEXTURE_2D };
    Type m_type { Type::TYPE_2D };
    uint32_t m_mip_levels { 1 };
    uint32_t m_array_layers { 1 };

    vk::ImageLayout m_current_layout { vk::ImageLayout::eUndefined };

    Kakshya::DataModality m_modality;
    std::vector<Kakshya::DataDimension> m_dimensions;

    /**
     * @brief Infer Kakshya::DataDimension entries from image parameters
     *
     * Uses current modality, dimensions, and format to populate m_dimensions
     * so processors and UI code can reason about the image layout.
     */
    void infer_dimensions_from_parameters();
};

} // namespace MayaFlux::Core
