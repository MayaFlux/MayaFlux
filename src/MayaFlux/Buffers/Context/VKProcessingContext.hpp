#pragma once

#include <vulkan/vulkan.hpp>

namespace MayaFlux::Buffers {
class Buffer;
class VKBuffer;

using BufferRegistrationCallback = std::function<void(std::shared_ptr<VKBuffer>)>;

/**
 * @class VKProcessingContext
 * @brief Pure facade for GPU operations - no direct Vulkan handle exposure
 *
 * Provides high-level operations for processors without exposing
 * backend implementation details. All operations delegate to backend
 * via callbacks.
 *
 * Design Philosophy:
 * - Processors never see vk::Device or backend objects
 * - All resource creation goes through callbacks
 * - Backend retains full control of resource lifecycle
 * - Thread-safe via backend synchronization
 */
class MAYAFLUX_API VKProcessingContext {
public:
    using CommandRecorder = std::function<void(vk::CommandBuffer)>;
    using ResourceHandle = void*;

    using ShaderModuleCreator = std::function<ResourceHandle(
        const std::string& spirv_path, vk::ShaderStageFlagBits stage)>;

    using DescriptorManagerCreator = std::function<ResourceHandle(uint32_t pool_size)>;

    using DescriptorLayoutCreator = std::function<vk::DescriptorSetLayout(ResourceHandle manager,
        const std::vector<std::pair<uint32_t, vk::DescriptorType>>& bindings)>;

    using ComputePipelineCreator = std::function<ResourceHandle(ResourceHandle shader,
        const std::vector<vk::DescriptorSetLayout>& layouts, uint32_t push_constant_size)>;

    using ResourceCleaner = std::function<void(ResourceHandle)>;

    VKProcessingContext() = default;
    ~VKProcessingContext() = default;

    /**
     * @brief Execute GPU commands immediately
     * @param recorder Lambda that records commands into command buffer
     *
     * Usage:
     *   context.execute_immediate([&](vk::CommandBuffer cmd) {
     *       cmd.copyBuffer(src, dst, copy_region);
     *   });
     *
     * Handles:
     * - Command buffer allocation
     * - Begin/end recording
     * - Queue submission
     * - Fence wait
     */
    void execute_immediate(CommandRecorder recorder);

    /**
     * @brief Record commands for deferred execution
     * @param recorder Lambda that records commands
     *
     * Commands are batched and submitted later by backend.
     * Use for optimal performance when order doesn't matter.
     */
    void record_deferred(CommandRecorder recorder);

    /**
     * @brief Flush host-visible buffer memory
     */
    void flush_buffer(vk::DeviceMemory memory, size_t offset, size_t size);

    /**
     * @brief Invalidate host-visible buffer memory
     */
    void invalidate_buffer(vk::DeviceMemory memory, size_t offset, size_t size);

    /**
     * @brief Set callback for immediate command execution
     */
    void set_execute_immediate_callback(std::function<void(CommandRecorder)> callback);

    /**
     * @brief Set callback for deferred command recording
     */
    void set_record_deferred_callback(std::function<void(CommandRecorder)> callback);

    /**
     * @brief Set callback for flushing buffer memory
     */
    void set_flush_callback(std::function<void(vk::DeviceMemory, size_t, size_t)> callback);

    /**
     * @brief Set callback for invalidating buffer memory
     */
    void set_invalidate_callback(std::function<void(vk::DeviceMemory, size_t, size_t)> callback);

    /**
     * @brief Create a shader module from SPIR-V file
     */
    ResourceHandle create_shader_module(const std::string& spirv_path, vk::ShaderStageFlagBits stage);

    /**
     * @brief Create a descriptor manager with specified pool size
     */
    ResourceHandle create_descriptor_manager(uint32_t pool_size = 1024);

    /**
     * @brief Create a descriptor set layout from bindings
     */
    vk::DescriptorSetLayout create_descriptor_layout(ResourceHandle manager,
        const std::vector<std::pair<uint32_t, vk::DescriptorType>>& bindings);

    /**
     * @brief Create a compute pipeline from shader and layouts
     */
    ResourceHandle create_compute_pipeline(ResourceHandle shader,
        const std::vector<vk::DescriptorSetLayout>& layouts,
        uint32_t push_constant_size);

    /**
     * @brief Cleanup a resource using the registered cleaner callback
     */
    void cleanup_resource(ResourceHandle resource);

    /**
     * @brief Set resource creation callbacks
     */
    void set_shader_module_creator(ShaderModuleCreator creator);

    /**
     * @brief Set resource creation callbacks
     */
    void set_descriptor_manager_creator(DescriptorManagerCreator creator);

    /**
     * @brief Set resource creation callbacks
     */
    void set_descriptor_layout_creator(DescriptorLayoutCreator creator);

    /**
     * @brief Set resource creation callbacks
     */
    void set_compute_pipeline_creator(ComputePipelineCreator creator);

    /**
     * @brief Set resource cleanup callback
     */
    void set_resource_cleaner(ResourceCleaner cleaner);

    /**
     * @brief Initializes a buffer using the registered initializer callback
     * @param buffer Buffer to initialize
     */
    static void initialize_buffer(const std::shared_ptr<VKBuffer>& buffer);

    /**
     * @brief Cleans up a buffer using the registered cleaner callback
     * @param buffer Buffer to clean up
     */
    static void cleanup_buffer(const std::shared_ptr<VKBuffer>& buffer);

    /**
     * @brief Sets the global buffer initializer and cleaner callbacks
     * @param initializer Callback to initialize buffers
     */
    static void set_initializer(BufferRegistrationCallback initializer);

    /**
     * @brief Sets the global buffer cleaner callback
     * @param cleaner Callback to clean up buffers
     */
    static void set_cleaner(BufferRegistrationCallback cleaner);

private:
    std::function<void(CommandRecorder)> m_execute_immediate;
    std::function<void(CommandRecorder)> m_record_deferred;
    std::function<void(vk::DeviceMemory, size_t, size_t)> m_flush;
    std::function<void(vk::DeviceMemory, size_t, size_t)> m_invalidate;

    static BufferRegistrationCallback s_buffer_initializer;
    static BufferRegistrationCallback s_buffer_cleaner;

    ShaderModuleCreator m_shader_module_creator;
    DescriptorManagerCreator m_descriptor_manager_creator;
    DescriptorLayoutCreator m_descriptor_layout_creator;
    ComputePipelineCreator m_compute_pipeline_creator;
    ResourceCleaner m_resource_cleaner;
};

} // namespace MayaFlux::Buffers
