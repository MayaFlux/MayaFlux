#pragma once

#include "vulkan/vulkan.hpp"

namespace MayaFlux::Registry::Service {
struct BufferService;
}

namespace MayaFlux::Buffers {
class VKBuffer;
}

namespace MayaFlux::Core {

class VKContext;
class VKImage;
class VKCommandManager;

/**
 * @class BackendResourceManager
 * @brief Manages Vulkan resources (buffers, images, samplers) for the graphics backend
 */
class MAYAFLUX_API BackendResourceManager {
public:
    BackendResourceManager(VKContext& context, VKCommandManager& command_manager);
    ~BackendResourceManager() = default;

    void setup_backend_service(const std::shared_ptr<Registry::Service::BufferService>& buffer_service);

    // ========================================================================
    // Buffer management
    // ========================================================================

    /**
     * @brief Initialize a buffer for use with the graphics backend
     * @param buffer Shared pointer to the buffer to initialize
     */
    void initialize_buffer(const std::shared_ptr<Buffers::VKBuffer>& buffer);

    /**
     * @brief Cleanup a buffer and release associated resources
     * @param buffer Shared pointer to the buffer to cleanup
     */
    void cleanup_buffer(const std::shared_ptr<Buffers::VKBuffer>& buffer);

    /**
     * @brief Flush any pending buffer operations (e.g., uploads/downloads)
     */
    void flush_pending_buffer_operations();

    // ========================================================================
    // Buffer management
    // ========================================================================

    /**
     * @brief Initialize a VKImage (allocate VkImage, memory, and create image view)
     * @param image VKImage to initialize
     *
     * Follows the same pattern as initialize_buffer:
     * 1. Create VkImage
     * 2. Allocate VkDeviceMemory
     * 3. Bind memory to image
     * 4. Create VkImageView
     * 5. Store handles in VKImage
     */
    void initialize_image(const std::shared_ptr<VKImage>& image);

    /**
     * @brief Cleanup a VKImage (destroy view, image, and free memory)
     * @param image VKImage to cleanup
     */
    void cleanup_image(const std::shared_ptr<VKImage>& image);

    /**
     * @brief Transition image layout using a pipeline barrier
     * @param image VkImage handle
     * @param old_layout Current layout
     * @param new_layout Target layout
     * @param mip_levels Number of mip levels to transition
     * @param array_layers Number of array layers to transition
     *
     * Executes immediately on graphics queue. Use for initial setup and
     * one-off transitions. For rendering, prefer manual barriers.
     */
    void transition_image_layout(
        vk::Image image,
        vk::ImageLayout old_layout,
        vk::ImageLayout new_layout,
        uint32_t mip_levels = 1,
        uint32_t array_layers = 1,
        vk::ImageAspectFlags aspect_flags = vk::ImageAspectFlagBits::eColor);

    /**
     * @brief Upload data to an image (creates staging buffer internally)
     * @param image Target VKImage
     * @param data Pixel data pointer
     * @param size Data size in bytes
     */
    void upload_image_data(
        std::shared_ptr<VKImage> image,
        const void* data,
        size_t size);

    /**
     * @brief Upload image data using a caller-supplied persistent staging buffer.
     *        Identical to upload_image_data() but skips the per-call VkBuffer
     *        allocation. The staging buffer must be host-visible and at least
     *        @p size bytes. Intended for high-frequency streaming uploads.
     * @param image   Target VKImage.
     * @param data    Source pixel data pointer.
     * @param size    Byte count.
     * @param staging Pre-allocated host-visible staging buffer.
     */
    void upload_image_data_with_staging(
        std::shared_ptr<VKImage> image,
        const void* data,
        size_t size,
        const std::shared_ptr<Buffers::VKBuffer>& staging);

    /**
     * @brief Download data from an image into a caller-supplied buffer.
     *
     * Transitions the image to eTransferSrcOptimal, copies to a staging buffer,
     * then restores it to restore_layout using restore_stage.
     *
     * The defaults cover the common case of a device-local texture in shader
     * read-only layout. Pass ePresentSrcKHR / eBottomOfPipe for swapchain images.
     *
     * @param image          Source image.
     * @param data           Destination host pointer (must be at least size bytes).
     * @param size           Byte count to read.
     * @param restore_layout Layout to transition back to after the copy.
     *                       Defaults to eShaderReadOnlyOptimal.
     * @param restore_stage  Pipeline stage that will consume the image after
     *                       restore. Defaults to eFragmentShader.
     */
    void download_image_data(
        std::shared_ptr<VKImage> image,
        void* data,
        size_t size,
        vk::ImageLayout restore_layout = vk::ImageLayout::eShaderReadOnlyOptimal,
        vk::PipelineStageFlags restore_stage = vk::PipelineStageFlagBits::eFragmentShader);

    // ========================================================================
    // Sampler management
    // ========================================================================

    /**
     * @brief Create sampler
     * @param filter Mag/min filter
     * @param address_mode Texture address mode (wrap, clamp, etc.)
     * @param anisotropy Max anisotropy (0 = disabled)
     * @return Sampler handle
     */
    vk::Sampler create_sampler(
        vk::Filter filter = vk::Filter::eLinear,
        vk::SamplerAddressMode address_mode = vk::SamplerAddressMode::eRepeat,
        float max_anisotropy = 0.0F);

    /**
     * @brief Destroy sampler
     */
    void destroy_sampler(vk::Sampler sampler);

    // ========================================================================
    // Memory management
    // ========================================================================

    /**
     * @brief Find a suitable memory type for Vulkan buffer allocation
     * @param type_filter Memory type bits filter
     * @param properties Desired memory property flags
     * @return Index of the suitable memory type
     */
    uint32_t find_memory_type(uint32_t type_filter, vk::MemoryPropertyFlags properties) const;

    // ========================================================================
    // Command management
    // ========================================================================

    /**
     * @brief Execute immediate command recording for buffer operations
     * @param recorder Command recording function
     */
    void execute_immediate_commands(const std::function<void(vk::CommandBuffer)>& recorder);

    /**
     * @brief Record deferred command recording for buffer operations
     * @param recorder Command recording function
     */
    void record_deferred_commands(const std::function<void(vk::CommandBuffer)>& recorder);

    // ========================================================================
    // Cleanup
    // ========================================================================

    void cleanup();

private:
    VKContext& m_context;
    VKCommandManager& m_command_manager;

    std::vector<std::shared_ptr<Buffers::VKBuffer>> m_managed_buffers;
    std::unordered_map<size_t, vk::Sampler> m_sampler_cache;

    size_t compute_sampler_hash(vk::Filter filter, vk::SamplerAddressMode address_mode, float max_anisotropy) const;
};

} // namespace MayaFlux::Core
