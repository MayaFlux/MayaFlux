#pragma once

#include "ShaderFoundry.hpp"

namespace MayaFlux::Portal::Graphics {

using ComputePipelineID = uint64_t;

constexpr ComputePipelineID INVALID_COMPUTE_PIPELINE = 0;

/**
 * @class ComputePress
 * @brief Compute-specific pipeline and dispatch orchestration
 *
 * Responsibilities:
 * - Create compute pipelines
 * - Bind compute pipelines
 * - Dispatch compute workgroups (direct + indirect)
 * - Compute-specific optimizations
 *
 * Uses ShaderFoundry for:
 * - Shaders, descriptors, commands, sync, barriers
 */
class MAYAFLUX_API ComputePress {
public:
    static ComputePress& instance()
    {
        static ComputePress press;
        return press;
    }

    ComputePress(const ComputePress&) = delete;
    ComputePress& operator=(const ComputePress&) = delete;
    ComputePress(ComputePress&&) noexcept = delete;
    ComputePress& operator=(ComputePress&&) noexcept = delete;

    bool initialize();
    void shutdown();

    //==========================================================================
    // Pipeline Creation (COMPUTE-SPECIFIC)
    //==========================================================================

    /**
     * @brief Create compute pipeline
     */
    ComputePipelineID create_pipeline(
        ShaderID shader_id,
        const std::vector<std::vector<DescriptorBindingInfo>>& descriptor_sets = {},
        size_t push_constant_size = 0);

    /**
     * @brief Create pipeline with auto-reflection
     */
    ComputePipelineID create_pipeline_auto(
        ShaderID shader_id,
        size_t push_constant_size = 0);

    void destroy_pipeline(ComputePipelineID pipeline_id);

    //==========================================================================
    // Pipeline Binding (COMPUTE-SPECIFIC)
    //==========================================================================

    /**
     * @brief Bind pipeline to active command buffer
     */
    void bind_pipeline(CommandBufferID cmd_id, ComputePipelineID pipeline_id);

    /**
     * @brief Bind descriptor sets to active command buffer
     */
    void bind_descriptor_sets(
        CommandBufferID cmd_id,
        ComputePipelineID pipeline_id,
        const std::vector<DescriptorSetID>& descriptor_set_ids);

    /**
     * @brief Push constants to active command buffer
     */
    void push_constants(
        CommandBufferID cmd_id,
        ComputePipelineID pipeline_id,
        const void* data,
        size_t size);

    //==========================================================================
    // Dispatch (COMPUTE-SPECIFIC)
    //==========================================================================

    /**
     * @brief Dispatch compute workgroups
     */
    void dispatch(CommandBufferID cmd_id, uint32_t x, uint32_t y, uint32_t z);

    /**
     * @brief Dispatch compute workgroups indirectly
     */
    void dispatch_indirect(
        CommandBufferID cmd_id,
        vk::Buffer indirect_buffer,
        vk::DeviceSize offset = 0);

    //==========================================================================
    // Convenience Wrappers
    //==========================================================================

    /**
     * @brief All-in-one: allocate descriptors for pipeline
     */
    std::vector<DescriptorSetID> allocate_pipeline_descriptors(ComputePipelineID pipeline_id);

    /**
     * @brief All-in-one: bind pipeline + descriptors + push constants
     */
    void bind_all(
        CommandBufferID cmd_id,
        ComputePipelineID pipeline_id,
        const std::vector<DescriptorSetID>& descriptor_set_ids,
        const void* push_constants_data = nullptr,
        size_t push_constant_size = 0);

private:
    ComputePress() = default;
    ~ComputePress() { shutdown(); }

    struct PipelineState {
        ShaderID shader_id;
        std::shared_ptr<Core::VKComputePipeline> pipeline;
        std::vector<vk::DescriptorSetLayout> layouts;
        vk::PipelineLayout layout;
    };

    std::shared_ptr<Core::VKDescriptorManager> m_descriptor_manager;
    std::unordered_map<ComputePipelineID, PipelineState> m_pipelines;
    std::atomic<uint64_t> m_next_pipeline_id { 1 };
    ShaderFoundry* m_shader_foundry = nullptr;

    static bool s_initialized;
};

inline MAYAFLUX_API ComputePress& get_compute_press()
{
    return ComputePress::instance();
}

} // namespace MayaFlux::Portal::Graphics
