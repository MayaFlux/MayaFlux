#pragma once

#include "vulkan/vulkan.hpp"

namespace MayaFlux::Registry::Service {
struct ComputeService;
}

namespace MayaFlux::Core {

class VKContext;
class VKShaderModule;
class VKDescriptorManager;
class VKComputePipeline;
class VKGraphicsPipeline;

struct GraphicsPipelineConfig;

/**
 * @class BackendPipelineManager
 * @brief Manages Vulkan pipelines (compute, graphics) and related resources
 */
class MAYAFLUX_API BackendPipelineManager {
public:
    explicit BackendPipelineManager(VKContext& context);
    ~BackendPipelineManager() = default;

    void setup_backend_service(const std::shared_ptr<Registry::Service::ComputeService>& compute_service);

    // ========================================================================
    // Sampler management
    // ========================================================================

    /** @brief Create a shader module from SPIR-V binary
     * @param spirv_path Path to the SPIR-V binary file
     * @param stage Shader stage (e.g., vertex, fragment, compute)
     * @return Shared pointer to the created VKShaderModule
     */
    std::shared_ptr<VKShaderModule> create_shader_module(const std::string& spirv_path, uint32_t stage);

    // ========================================================================
    // Descriptor management
    // ========================================================================

    /** @brief Create a descriptor manager for managing descriptor sets
     * @param pool_size Number of descriptor sets to allocate in the pool
     * @return Shared pointer to the created VKDescriptorManager
     */
    std::shared_ptr<VKDescriptorManager> create_descriptor_manager(uint32_t pool_size);

    /** @brief Create a descriptor set layout
     * @param manager Descriptor manager to use for allocation
     * @param bindings Vector of pairs specifying binding index and descriptor type
     * @return Created vk::DescriptorSetLayout
     */
    vk::DescriptorSetLayout create_descriptor_layout(
        const std::shared_ptr<VKDescriptorManager>& manager,
        const std::vector<std::pair<uint32_t, uint32_t>>& bindings);

    // ========================================================================
    // Pipeline management
    // ========================================================================

    /** @brief Create a compute pipeline
     * @param shader Shader module to use for the compute pipeline
     * @param layouts Vector of descriptor set layouts used by the pipeline
     * @param push_constant_size Size of push constant range in bytes
     * @return Shared pointer to the created VKComputePipeline
     */
    std::shared_ptr<VKComputePipeline> create_compute_pipeline(
        const std::shared_ptr<VKShaderModule>& shader,
        const std::vector<vk::DescriptorSetLayout>& layouts,
        uint32_t push_constant_size);

    /**
     * @brief Create graphics pipeline
     * @param config Graphics pipeline configuration
     * @return Shared pointer to created pipeline, or nullptr on failure
     */
    std::shared_ptr<VKGraphicsPipeline> create_graphics_pipeline(
        const GraphicsPipelineConfig& config);

    // ========================================================================
    // Cleanup
    // ========================================================================

    /** @brief Cleanup a compute resource allocated by the backend
     * @param resource Pointer to the resource to cleanup
     */
    void cleanup_compute_resource(void* resource);

    void cleanup();

private:
    VKContext& m_context;

    std::vector<std::shared_ptr<VKShaderModule>> m_managed_shaders;
    std::vector<std::shared_ptr<VKDescriptorManager>> m_managed_descriptor_managers;
    std::vector<std::shared_ptr<VKComputePipeline>> m_managed_compute_pipelines;
    std::vector<std::shared_ptr<VKGraphicsPipeline>> m_managed_graphics_pipelines;

    void track_shader(const std::shared_ptr<VKShaderModule>& shader);
    void track_descriptor_manager(const std::shared_ptr<VKDescriptorManager>& manager);
    void track_compute_pipeline(const std::shared_ptr<VKComputePipeline>& pipeline);
    void track_graphics_pipeline(const std::shared_ptr<VKGraphicsPipeline>& pipeline);
};

} // namespace MayaFlux::Core
