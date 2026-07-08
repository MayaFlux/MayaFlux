#pragma once

#include "ShaderFoundry.hpp"

namespace MayaFlux::Portal::Graphics {

using ComputePipelineID = uint64_t;

constexpr ComputePipelineID INVALID_COMPUTE_PIPELINE = 0;

//==========================================================================
// Multi-Pipeline Sequencing
//==========================================================================

/**
 * @struct HazardResource
 * @brief One resource this stage's dispatch reads/writes that a later
 *        stage in the sequence depends on, paired with its binding
 *        metadata for barrier construction.
 *
 * ComputePress owns no resource registry (that lives in callers like
 * Yantra::GpuResourceManager), so the raw handle must travel with the
 * binding description rather than being looked up by index.
 */
struct HazardResource {
    GpuBufferBinding binding; ///< Direction/element_type describing this resource.
    vk::Image image; ///< Valid when binding.element_type is IMAGE_STORAGE/IMAGE_SAMPLED.
    vk::Buffer buffer; ///< Valid when binding.element_type is anything else.
};

/**
 * @struct ComputeStage
 * @brief One pipeline dispatch within a ComputePress::record_sequence call.
 *
 * Bundles everything needed to bind a pipeline, push constants, dispatch,
 * and determine the barrier owed to the next stage in the sequence.
 * Pipeline and descriptor sets must already exist (created via
 * create_pipeline / allocate_pipeline_descriptors) — this struct only
 * describes how to use them within a recorded sequence, it does not
 * own their lifetime.
 */
struct ComputeStage {
    ComputePipelineID pipeline_id; ///< Pipeline to bind for this stage.
    std::vector<DescriptorSetID> descriptor_set_ids; ///< Descriptor sets to bind.
    std::array<uint32_t, 3> groups; ///< Workgroup dispatch counts {x, y, z}.
    std::vector<uint8_t> push_constant_data; ///< Raw push constant bytes for this stage.

    /**
     * @brief Resources this stage reads or writes that a subsequent
     *        stage in the same sequence depends on.
     *
     * Drives the compute-to-compute barrier inserted immediately after
     * this stage's dispatch, before the next stage binds its pipeline.
     * Only entries whose binding.direction is INPUT_OUTPUT trigger a
     * barrier; include a resource here if a later stage must observe
     * this stage's writes to it. Leave empty if nothing downstream
     * depends on this stage's output within the sequence.
     */
    std::vector<HazardResource> hazard_resources;
};

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
    void stop();
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

    /**
     * @brief Record multiple pipeline dispatches into one already-open
     *        command buffer, inserting a barrier between each consecutive
     *        stage for that stage's hazard_bindings.
     *
     * Vulkan permits binding different pipelines sequentially within a
     * single command buffer recording — this is not a render-pass-style
     * batching mechanism, only direct multi-pipeline recording plus the
     * hazard barriers each transition needs. Does not submit; caller owns
     * the command buffer's lifetime via ShaderFoundry (begin_commands /
     * submit_and_wait or submit_async), the same as any other recorded
     * sequence of commands.
     *
     * Image bindings receive an eGeneral -> eGeneral shader write/read
     * hazard barrier via ShaderFoundry::image_barrier. Non-image bindings
     * receive a buffer_barrier with the same access/stage masks. Both use
     * compute-to-compute pipeline stages throughout, since this method is
     * for chaining compute stages only.
     *
     * @param cmd_id Command buffer to record into. Must already be active
     *               (begun via ShaderFoundry::begin_commands).
     * @param stages Ordered list of pipeline dispatches to record.
     */
    void record_sequence(CommandBufferID cmd_id, const std::vector<ComputeStage>& stages);

private:
    ComputePress() = default;
    ~ComputePress() { shutdown(); }
    void cleanup_pipelines();

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
