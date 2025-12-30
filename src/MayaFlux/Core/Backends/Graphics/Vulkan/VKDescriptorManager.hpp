#pragma once

#include <vulkan/vulkan.hpp>

namespace MayaFlux::Core {

/**
 * @struct DescriptorBinding
 * @brief Describes a single descriptor binding in a set
 *
 * Matches the binding declaration in shaders:
 *   layout(set = X, binding = Y) buffer Data { ... };
 */
struct DescriptorBinding {
    uint32_t binding; ///< Binding index within set
    vk::DescriptorType type; ///< Type (storage buffer, uniform, etc.)
    uint32_t count; ///< Array size (1 for non-arrays)
    vk::ShaderStageFlags stage_flags; ///< Which shader stages access this

    DescriptorBinding(
        uint32_t binding_,
        vk::DescriptorType type_,
        vk::ShaderStageFlags stages_,
        uint32_t count_ = 1)
        : binding(binding_)
        , type(type_)
        , count(count_)
        , stage_flags(stages_)
    {
    }
};

/**
 * @struct DescriptorSetLayoutConfig
 * @brief Configuration for creating a descriptor set layout
 *
 * Defines all bindings in a descriptor set. Multiple sets can exist
 * per pipeline (set=0, set=1, etc.), each with its own layout.
 */
struct DescriptorSetLayoutConfig {
    std::vector<DescriptorBinding> bindings;

    void add_binding(uint32_t binding, vk::DescriptorType type,
        vk::ShaderStageFlags stages, uint32_t count = 1)
    {
        bindings.emplace_back(binding, type, stages, count);
    }

    void add_storage_buffer(uint32_t binding, vk::ShaderStageFlags stages = vk::ShaderStageFlagBits::eCompute)
    {
        add_binding(binding, vk::DescriptorType::eStorageBuffer, stages);
    }

    void add_uniform_buffer(uint32_t binding, vk::ShaderStageFlags stages = vk::ShaderStageFlagBits::eCompute)
    {
        add_binding(binding, vk::DescriptorType::eUniformBuffer, stages);
    }

    void add_storage_image(uint32_t binding, vk::ShaderStageFlags stages = vk::ShaderStageFlagBits::eCompute)
    {
        add_binding(binding, vk::DescriptorType::eStorageImage, stages);
    }

    void add_sampled_image(uint32_t binding, vk::ShaderStageFlags stages = vk::ShaderStageFlagBits::eCompute)
    {
        add_binding(binding, vk::DescriptorType::eCombinedImageSampler, stages);
    }
};

/**
 * @class DescriptorUpdateBatch
 * @brief Fluent interface for batching descriptor updates
 *
 * Usage:
 *   manager.begin_batch(device, descriptor_set)
 *       .buffer(0, vk_buffer)
 *       .image(1, image_view, sampler)
 *       .buffer(2, another_buffer)
 *       .submit();
 */
class DescriptorUpdateBatch {
public:
    DescriptorUpdateBatch(vk::Device device, vk::DescriptorSet set);

    DescriptorUpdateBatch& buffer(uint32_t binding, vk::Buffer buffer,
        vk::DeviceSize offset = 0,
        vk::DeviceSize range = VK_WHOLE_SIZE);

    DescriptorUpdateBatch& storage_image(uint32_t binding,
        vk::ImageView image_view,
        vk::ImageLayout layout = vk::ImageLayout::eGeneral);

    DescriptorUpdateBatch& combined_image_sampler(uint32_t binding,
        vk::ImageView image_view,
        vk::Sampler sampler,
        vk::ImageLayout layout = vk::ImageLayout::eShaderReadOnlyOptimal);

    DescriptorUpdateBatch& sampler(uint32_t binding, vk::Sampler sampler);

    void submit(); // Actually performs the update

private:
    vk::Device m_device;
    vk::DescriptorSet m_set;
    std::vector<vk::WriteDescriptorSet> m_writes;
    std::vector<vk::DescriptorBufferInfo> m_buffer_infos;
    std::vector<vk::DescriptorImageInfo> m_image_infos;
};

/**
 * @class VKDescriptorManager
 * @brief Manages descriptor pools, layouts, and set allocation
 *
 * Responsibilities:
 * - Create descriptor set layouts from bindings
 * - Allocate descriptor pools
 * - Allocate descriptor sets from pools
 * - Update descriptor sets (bind buffers/images)
 * - Handle pool exhaustion and growth
 *
 * Does NOT handle:
 * - Pipeline creation (that's VKComputePipeline)
 * - Command buffer recording (that's VKCommandManager)
 * - Buffer/image creation (that's VKBuffer/VKImage)
 *
 * Design:
 * - One manager per logical context (e.g., per VulkanBackend)
 * - Pools grow automatically when exhausted
 * - Layouts are cached by configuration
 */
class VKDescriptorManager {
public:
    VKDescriptorManager() = default;
    ~VKDescriptorManager();

    VKDescriptorManager(const VKDescriptorManager&) = delete;
    VKDescriptorManager& operator=(const VKDescriptorManager&) = delete;
    VKDescriptorManager(VKDescriptorManager&&) noexcept;
    VKDescriptorManager& operator=(VKDescriptorManager&&) noexcept;

    /**
     * @brief Initialize descriptor manager
     * @param device Logical device
     * @param initial_pool_size Number of descriptor sets per pool
     * @return true if initialization succeeded
     *
     * Creates initial descriptor pool. More pools are allocated on-demand
     * when the current pool is exhausted.
     */
    bool initialize(vk::Device device, uint32_t initial_pool_size = 1024);

    /**
     * @brief Cleanup all descriptor resources
     * @param device Logical device (must match initialization)
     *
     * Destroys all pools and layouts. Safe to call multiple times.
     */
    void cleanup(vk::Device device);

    /**
     * @brief Create descriptor set layout from configuration
     * @param device Logical device
     * @param config Layout configuration (bindings)
     * @return Descriptor set layout handle, or null on failure
     *
     * Layouts are cached - subsequent calls with identical configs
     * return the same layout without recreation.
     *
     * Example:
     *   DescriptorSetLayoutConfig config;
     *   config.add_storage_buffer(0);  // layout(binding=0) buffer
     *   config.add_storage_buffer(1);  // layout(binding=1) buffer
     *   auto layout = manager.create_layout(device, config);
     */
    vk::DescriptorSetLayout create_layout(
        vk::Device device,
        const DescriptorSetLayoutConfig& config);

    /**
     * @brief Allocate a descriptor set from the pool
     * @param device Logical device
     * @param layout Descriptor set layout
     * @return Allocated descriptor set, or null on failure
     *
     * Allocates from current pool. If pool is exhausted, creates a new
     * pool automatically and retries allocation.
     *
     * Sets are freed when their pool is destroyed (on cleanup()).
     */
    vk::DescriptorSet allocate_set(
        vk::Device device,
        vk::DescriptorSetLayout layout);

    /**
     * @brief Update descriptor set with buffer binding
     * @param device Logical device
     * @param set Descriptor set to update
     * @param binding Binding index
     * @param buffer Buffer to bind
     * @param offset Offset in buffer (bytes)
     * @param range Size to bind (VK_WHOLE_SIZE for entire buffer)
     *
     * Binds a buffer to the specified binding point in the descriptor set.
     * Must match the type declared in the layout (storage/uniform buffer).
     *
     * Example:
     *   manager.update_buffer(device, set, 0, vk_buffer, 0, VK_WHOLE_SIZE);
     *   // Now shader can access: layout(binding=0) buffer Data { ... };
     */
    void update_buffer(
        vk::Device device,
        vk::DescriptorSet set,
        uint32_t binding,
        vk::Buffer buffer,
        vk::DeviceSize offset = 0,
        vk::DeviceSize range = VK_WHOLE_SIZE);

    /**
     * @brief Update descriptor set with image binding
     * @param device Logical device
     * @param set Descriptor set to update
     * @param binding Binding index
     * @param image_view Image view to bind
     * @param sampler Sampler (for combined image samplers, null for storage images)
     * @param layout Image layout (typically eGeneral or eShaderReadOnlyOptimal)
     *
     * Binds an image to the specified binding point in the descriptor set.
     * For storage images: sampler = null, layout = eGeneral
     * For sampled images: provide sampler, layout = eShaderReadOnlyOptimal
     */
    void update_image(
        vk::Device device,
        vk::DescriptorSet set,
        uint32_t binding,
        vk::ImageView image_view,
        vk::Sampler sampler = nullptr,
        vk::ImageLayout layout = vk::ImageLayout::eGeneral);

    /**
     * @brief Update descriptor set with sampler binding
     * @param device Logical device
     * @param set Descriptor set to update
     * @param binding Binding index
     * @param sampler Sampler to bind
     *
     * For samplers used independently of images (immutable samplers in layout).
     */
    void update_sampler(
        vk::Device device,
        vk::DescriptorSet set,
        uint32_t binding,
        vk::Sampler sampler);

    /**
     * @brief Update descriptor set with combined image+sampler
     * @param device Logical device
     * @param set Descriptor set to update
     * @param binding Binding index
     * @param image_view Image view to bind
     * @param sampler Sampler to bind
     * @param layout Image layout (typically eShaderReadOnlyOptimal)
     *
     * This is the standard way to bind textures in graphics shaders.
     */
    void update_combined_image_sampler(
        vk::Device device,
        vk::DescriptorSet set,
        uint32_t binding,
        vk::ImageView image_view,
        vk::Sampler sampler,
        vk::ImageLayout layout = vk::ImageLayout::eShaderReadOnlyOptimal);

    /**
     * @brief Update descriptor set with input attachment
     * @param device Logical device
     * @param set Descriptor set to update
     * @param binding Binding index
     * @param image_view Image view to bind
     * @param layout Image layout (must be eShaderReadOnlyOptimal or eGeneral)
     *
     * TODO: Future - deferred rendering support
     */
    void update_input_attachment(
        vk::Device device,
        vk::DescriptorSet set,
        uint32_t binding,
        vk::ImageView image_view,
        vk::ImageLayout layout = vk::ImageLayout::eShaderReadOnlyOptimal);

    /**
     * @brief Begin a batch descriptor update
     * @return Batch builder for fluent interface
     */
    DescriptorUpdateBatch begin_batch(vk::Device device, vk::DescriptorSet set)
    {
        return { device, set };
    }

    /**
     * @brief Copy descriptor set contents
     * @param device Logical device
     * @param src Source descriptor set
     * @param dst Destination descriptor set
     * @param copy_count Number of bindings to copy (0 = all)
     */
    void copy_descriptor_set(
        vk::Device device,
        vk::DescriptorSet src,
        vk::DescriptorSet dst,
        uint32_t copy_count = 0);

    /**
     * @brief Allocate multiple descriptor sets at once
     * @param device Logical device
     * @param layouts Vector of layouts (one per set)
     * @return Vector of allocated descriptor sets
     *
     * More efficient than multiple allocate_set() calls.
     */
    std::vector<vk::DescriptorSet> allocate_sets(
        vk::Device device,
        const std::vector<vk::DescriptorSetLayout>& layouts);

    /**
     * @brief Batch update multiple bindings at once
     * @param device Logical device
     * @param writes Vector of descriptor write operations
     *
     * More efficient than multiple individual updates when updating
     * many bindings in the same set.
     */
    void batch_update(
        vk::Device device,
        const std::vector<vk::WriteDescriptorSet>& writes);

    /**
     * @brief Reset all descriptor pools
     * @param device Logical device
     *
     * Frees all allocated descriptor sets. Useful for clearing temporary
     * descriptors between frames or processing cycles.
     * Does NOT destroy pools or layouts.
     */
    void reset_pools(vk::Device device);

    /**
     * @brief Get current pool utilization
     * @return Pair of (allocated_sets, total_capacity)
     */
    std::pair<uint32_t, uint32_t> get_pool_stats() const
    {
        return { m_allocated_count, m_pool_capacity };
    }

private:
    vk::Device m_device = nullptr;

    std::vector<vk::DescriptorPool> m_pools;
    size_t m_current_pool_index = 0;
    uint32_t m_pool_size = 1024; ///< Sets per pool
    uint32_t m_allocated_count = 0; ///< Total allocated sets
    uint32_t m_pool_capacity = 0; ///< Total capacity across all pools

    std::vector<vk::DescriptorSetLayout> m_layouts;

    std::unordered_map<size_t, size_t> m_layout_cache;

    /**
     * @brief Create a new descriptor pool
     * @param device Logical device
     * @param max_sets Maximum descriptor sets this pool can allocate
     * @return Pool handle, or null on failure
     *
     * Pool sizes are calculated to handle common descriptor types:
     * - Storage buffers (common in compute)
     * - Uniform buffers (common in graphics)
     * - Storage images (less common)
     * - Combined image samplers (less common)
     */
    vk::DescriptorPool create_pool(vk::Device device, uint32_t max_sets);

    /**
     * @brief Grow pool capacity by allocating new pool
     * @param device Logical device
     * @return true if new pool created successfully
     */
    bool grow_pools(vk::Device device);

    /**
     * @brief Compute hash of descriptor set layout config
     * @param config Layout configuration
     * @return Hash value for cache lookup
     */
    size_t hash_layout_config(const DescriptorSetLayoutConfig& config) const;
};

} // namespace MayaFlux::Core
