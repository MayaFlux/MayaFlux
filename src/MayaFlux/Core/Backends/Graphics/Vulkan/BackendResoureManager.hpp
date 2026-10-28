#pragma once

#include "VKCommandManager.hpp"
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
     * @brief Allocate a raw VkBuffer/VkDeviceMemory pair without an owning
     *        VKBuffer object.
     * @param size_bytes Buffer capacity in bytes.
     * @param usage Vulkan buffer usage flags.
     * @param memory_properties Vulkan memory property flags.
     * @param host_visible When true, the allocation is mapped and out_mapped_ptr
     *        is populated; when false, out_mapped_ptr is left null.
     * @param out_buffer Receives the created vk::Buffer.
     * @param out_memory Receives the bound vk::DeviceMemory.
     * @param out_mapped_ptr Receives the mapped host pointer, or nullptr if
     *        @p host_visible is false.
     *
     * Same allocation sequence as initialize_buffer() (createBuffer,
     * getBufferMemoryRequirements, allocateMemory, bindBufferMemory, optional
     * mapMemory), without requiring or populating a VKBuffer. Intended for
     * VKBuffer subclasses that own secondary raw handle pairs directly in
     * VKBufferResources::back_buffers rather than as separate VKBuffer objects.
     */
    void allocate_raw_buffer(
        size_t size_bytes,
        vk::BufferUsageFlags usage,
        vk::MemoryPropertyFlags memory_properties,
        bool host_visible,
        vk::Buffer& out_buffer,
        vk::DeviceMemory& out_memory,
        void*& out_mapped_ptr);

    /**
     * @brief Cleanup a buffer and release associated resources
     * @param buffer Shared pointer to the buffer to cleanup
     */
    void cleanup_buffer(const std::shared_ptr<Buffers::VKBuffer>& buffer);

    /**
     * @brief Query the Vulkan device address of an initialized BDA-capable buffer
     * @param buffer Buffer with Usage::UNIFORM_BDA or Usage::STORAGE_BDA
     * @return vk::DeviceAddress, or 0 if the buffer was not created with eShaderDeviceAddress
     */
    [[nodiscard]] vk::DeviceAddress get_buffer_device_address(
        const std::shared_ptr<Buffers::VKBuffer>& buffer) const;

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
     * @brief Upload a whole color image from host memory.
     * Immediate calls wait for prior GPU use. Deferred calls record a GPU copy
     * and require a caller-owned staging buffer.
     */
    void upload_image_data(
        const std::shared_ptr<VKImage>& image,
        const void* data,
        size_t size,
        const std::shared_ptr<Buffers::VKBuffer>& staging = nullptr,
        bool deferred = false);

    /**
     * @brief Read an image into host memory.
     * Managed images use host image copy. Swapchain images use GPU readback.
     * @param restore_layout Layout to restore after the copy.
     * @param restore_stage Pipeline stage that consumes the restored image.
     */
    void download_image_data(
        const std::shared_ptr<VKImage>& image,
        void* data,
        size_t size,
        vk::ImageLayout restore_layout = vk::ImageLayout::eShaderReadOnlyOptimal,
        vk::PipelineStageFlags2 restore_stage = vk::PipelineStageFlagBits2::eFragmentShader);

    /**
     * @brief Upload one array layer while preserving the other layers.
     * @return True if the upload completed.
     */
    bool upload_image_layer(
        const std::shared_ptr<VKImage>& image,
        uint32_t layer,
        const void* data,
        size_t size);

    /**
     * @brief Whether a blit between two formats is supported with a filter.
     *
     * Checks optimal tiling blit source and destination features, and linear
     * sampling on the source for a linear filter.
     */
    [[nodiscard]] bool supports_blit(
        vk::Format src_format,
        vk::Format dst_format,
        vk::Filter filter = vk::Filter::eNearest) const;

    /**
     * @brief Blit a region of one image into a region of another.
     *
     * Scales with @p filter and converts between supported formats. The
     * subresources in @p region choose the array layers. Both images are
     * returned to the layout they held, or shader read when it was transient.
     *
     * @param src      Source image.
     * @param dst      Destination image, distinct from @p src.
     * @param region   Subresources and offsets.
     * @param filter   Nearest or linear.
     * @param deferred Record for deferred submission.
     * @return True if the blit was recorded or executed.
     */
    bool blit_image(
        const std::shared_ptr<VKImage>& src,
        const std::shared_ptr<VKImage>& dst,
        const vk::ImageBlit& region,
        vk::Filter filter,
        bool deferred = false);

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

    /**
     * @brief Submit the pending deferred commands
     * @return Point on the graphics queue timeline, or an empty point when nothing was pending
     */
    TimelinePoint flush_deferred_commands();

    // ========================================================================
    // Cleanup
    // ========================================================================

    void cleanup();

private:
    void load_host_copy_layouts();
    [[nodiscard]] bool host_copy_src_layout(vk::ImageLayout layout) const;
    [[nodiscard]] bool host_copy_dst_layout(vk::ImageLayout layout) const;
    void transition_host_image(
        vk::Image image,
        vk::ImageLayout old_layout,
        vk::ImageLayout new_layout,
        vk::ImageSubresourceRange range,
        vk::PipelineStageFlags2 restore_stage = vk::PipelineStageFlagBits2::eAllCommands);
    void upload_image_host(
        const std::shared_ptr<VKImage>& image,
        uint32_t base_layer,
        uint32_t layer_count,
        const void* data,
        size_t size);
    void download_image_host(
        const std::shared_ptr<VKImage>& image,
        void* data,
        size_t size,
        vk::ImageLayout restore_layout,
        vk::PipelineStageFlags2 restore_stage);

    [[nodiscard]] vk::MappedMemoryRange mapped_memory_range(
        vk::DeviceMemory memory,
        vk::DeviceSize offset,
        vk::DeviceSize size) const;

    VKContext& m_context;
    VKCommandManager& m_command_manager;

    std::vector<std::shared_ptr<Buffers::VKBuffer>> m_managed_buffers;
    std::unordered_map<VkDeviceMemory, vk::DeviceSize> m_buffer_allocation_sizes;
    std::unordered_map<size_t, vk::Sampler> m_sampler_cache;
    bool m_host_copy_layouts_ready {};
    std::vector<vk::ImageLayout> m_host_copy_src_layouts;
    std::vector<vk::ImageLayout> m_host_copy_dst_layouts;

    size_t compute_sampler_hash(vk::Filter filter, vk::SamplerAddressMode address_mode, float max_anisotropy) const;
};

} // namespace MayaFlux::Core
