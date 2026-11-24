#pragma once

#include "ImageAccess.hpp"
#include "VKCommandManager.hpp"

#include "MayaFlux/Transitive/Memory/FlatMap.hpp"
#include "MayaFlux/Transitive/Memory/SnapshotCell.hpp"
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
     *
     * Waits for tracked submitted use. Deferred users retain the old allocation
     * until their existing submission completes.
     */
    void cleanup_buffer(const std::shared_ptr<Buffers::VKBuffer>& buffer);

    /**
     * @brief Detach a buffer's resources and release them when submitted work completes
     * @param buffer Shared pointer to the buffer to retire
     *
     * The buffer is cleared and unregistered at once, so it can be initialized
     * again. Recorded deferred users retain its old allocation without submitting.
     */
    void retire_buffer(const std::shared_ptr<Buffers::VKBuffer>& buffer);

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
     * @brief Detach a managed image and retire its allocation after recorded use
     * @param image VKImage to cleanup
     *
     * Managed images lose their handles immediately. Recorded library commands
     * retain the old allocation until they complete or are discarded.
     */
    void cleanup_image(const std::shared_ptr<VKImage>& image);

    /** Keep this image's current allocation alive through a recorded command. */
    [[nodiscard]] std::shared_ptr<void> retain_image_allocation(const std::shared_ptr<VKImage>& image) const;

    /**
     * @brief Transition every mip level and layer of an image to a new layout
     * @param image Initialized image; its tracked layout is the old layout
     * @param new_layout Target layout
     * @param mode How the barrier is submitted
     *
     * An image that has never been transitioned changes layout on the host when
     * the device allows it, with no GPU work. Otherwise a barrier is recorded,
     * and later submissions and host image copies are ordered after it. The
     * tracked layout is updated when the barrier is recorded.
     */
    void transition_image_layout(
        const std::shared_ptr<VKImage>& image,
        vk::ImageLayout new_layout,
        SubmitMode mode = SubmitMode::DEFERRED);

    /**
     * @brief Record a barrier between two uses of an image into a command buffer being recorded
     * @param cmd Command buffer in the recording state, outside any dynamic rendering scope
     * @param image Initialized image
     * @param from The use the range leaves
     * @param to The use the range enters
     * @param range Mip levels and layers to move; the whole image by default
     *
     * The caller names the use being left, since the tracked layout is one value for
     * the whole image and cannot describe a range. Moving the whole image sets the
     * tracked layout to the layout of @p to when the barrier is recorded. Moving a range
     * leaves the tracked layout alone, so whoever transitions ranges owns their layouts.
     * A pass recorded elsewhere reports where it left an image with
     * VKImage::set_current_layout.
     */
    void record_image_transition(
        vk::CommandBuffer cmd,
        const std::shared_ptr<VKImage>& image,
        ImageAccess from,
        ImageAccess to,
        const ImageRange& range = {});

    /**
     * @brief A view of part of a managed image, created on first request
     * @param image Image created through this manager
     * @param key What the view covers; a default key covers the whole image
     * @return The view, or a null handle if the image is not managed or the key lies outside it
     *
     * Views are cached on the image's allocation and destroyed with it, so one stays
     * valid as long as a command that retained the allocation through
     * retain_image_allocation. The whole-image view is VKImage::get_image_view().
     */
    [[nodiscard]] vk::ImageView get_image_view(
        const std::shared_ptr<VKImage>& image,
        const ImageViewKey& key = {});

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
     * @brief Whether a format can be drawn into and sampled, and written as a storage image when asked.
     *
     * Checks optimal tiling features for a color attachment and a sampled image,
     * and a storage image when @p storage is set.
     */
    [[nodiscard]] bool supports_render_target(vk::Format format, bool storage = false) const;

    /**
     * @brief Whether a format can be blended as a color attachment.
     */
    [[nodiscard]] bool supports_color_blend(vk::Format format) const;

    /**
     * @brief Whether one draw can give each color attachment its own blend state.
     *
     * True when the device has independentBlend, which the backend enables
     * whenever it is there.
     */
    [[nodiscard]] bool supports_independent_blend() const;

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
     * @param mode     IMMEDIATE waits, DEFERRED records into the batch a flush submits,
     *                 ASYNC submits at once without waiting.
     * @return True if the blit was recorded, executed or submitted.
     */
    bool blit_image(
        const std::shared_ptr<VKImage>& src,
        const std::shared_ptr<VKImage>& dst,
        const vk::ImageBlit& region,
        vk::Filter filter,
        SubmitMode mode = SubmitMode::IMMEDIATE);

    /**
     * @brief Fill every mip level below the first by repeated blits of the level above.
     *
     * Level zero must already hold the contents. The other levels are overwritten.
     * Each level is blitted from the one before it, over every array layer or depth
     * slice, so the barriers move one level at a time. The image returns to the layout it
     * held, or shader read when that was transient.
     *
     * @param image  Initialized color image with more than one mip level.
     * @param filter Nearest or linear.
     * @param mode   IMMEDIATE waits, DEFERRED records into the batch a flush submits,
     *               ASYNC submits at once without waiting.
     * @return True if the chain was recorded, executed or submitted. False for an image
     *         with one level, a depth or stencil format, or a format that cannot blit
     *         with the filter.
     */
    bool generate_mip_chain(
        const std::shared_ptr<VKImage>& image,
        vk::Filter filter = vk::Filter::eLinear,
        SubmitMode mode = SubmitMode::IMMEDIATE);

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
     * @brief Record and submit commands without waiting
     * @param recorder   Command recording function
     * @param keep_alive Objects the commands read or write, released once the submission completes
     * @return Completion point on the graphics queue, or an empty point if the submit failed
     *
     * Ordered after earlier graphics queue work, and waits on the GPU for produced
     * compute work. The point is noted as produced, so a host image copy made later
     * waits for it. Submission is externally synchronized like every other submit.
     */
    TimelinePoint execute_async(
        const std::function<void(vk::CommandBuffer)>& recorder,
        std::vector<std::shared_ptr<void>> keep_alive = {});

    /**
     * @brief Record deferred command recording for buffer operations
     * @param recorder Command recording function
     * @param keep_alive Objects the commands read or write, released once the batch's submission completes
     */
    void record_deferred_commands(
        const std::function<void(vk::CommandBuffer)>& recorder,
        std::vector<std::shared_ptr<void>> keep_alive = {});

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
    using AllocationSizes = Memory::FlatMap<VkDeviceMemory, vk::DeviceSize>;

    struct BufferAllocation {
        vk::Device device;
        std::shared_ptr<Memory::SnapshotCell<AllocationSizes>> allocation_sizes;
        vk::Buffer buffer;
        vk::DeviceMemory memory;
        bool mapped;

        ~BufferAllocation();
    };

    struct ImageAllocation {
        vk::Device device;
        vk::Image image;
        vk::ImageView view;
        vk::DeviceMemory memory;
        std::vector<std::pair<ImageViewKey, vk::ImageView>> sub_views;

        ~ImageAllocation();
    };

    void load_host_copy_layouts();
    [[nodiscard]] bool host_copy_src_layout(vk::ImageLayout layout) const;
    [[nodiscard]] bool host_copy_dst_layout(vk::ImageLayout layout) const;
    void transition_host_image(
        const std::shared_ptr<VKImage>& image,
        vk::ImageLayout old_layout,
        vk::ImageLayout new_layout,
        vk::ImageSubresourceRange range,
        vk::PipelineStageFlags2 restore_stage = vk::PipelineStageFlagBits2::eAllCommands,
        SubmitMode mode = SubmitMode::IMMEDIATE);
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

    /**
     * @brief Wait for the GPU work a host image copy depends on
     *
     * Flushes deferred commands and waits for them and for produced foundry work.
     * Draws and other submissions are not waited for.
     */
    void await_host_image_inputs();

    [[nodiscard]] vk::MappedMemoryRange mapped_memory_range(
        vk::DeviceMemory memory,
        vk::DeviceSize offset,
        vk::DeviceSize size) const;

    VKContext& m_context;
    VKCommandManager& m_command_manager;

    std::vector<std::shared_ptr<Buffers::VKBuffer>> m_managed_buffers;
    std::map<std::shared_ptr<Buffers::VKBuffer>, std::shared_ptr<BufferAllocation>, std::owner_less<>> m_buffer_allocations;
    std::map<std::shared_ptr<VKImage>, std::shared_ptr<ImageAllocation>, std::owner_less<>> m_image_allocations;
    std::shared_ptr<Memory::SnapshotCell<AllocationSizes>> m_buffer_allocation_sizes =
        std::make_shared<Memory::SnapshotCell<AllocationSizes>>();
    std::unordered_map<size_t, vk::Sampler> m_sampler_cache;
    bool m_host_copy_layouts_ready {};
    std::vector<vk::ImageLayout> m_host_copy_src_layouts;
    std::vector<vk::ImageLayout> m_host_copy_dst_layouts;

    size_t compute_sampler_hash(vk::Filter filter, vk::SamplerAddressMode address_mode, float max_anisotropy) const;
};

} // namespace MayaFlux::Core
