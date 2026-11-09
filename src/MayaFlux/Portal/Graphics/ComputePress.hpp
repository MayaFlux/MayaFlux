#pragma once

#include "ShaderFoundry.hpp"

namespace MayaFlux::Portal::Graphics {

using ComputePipelineID = uint64_t;

constexpr ComputePipelineID INVALID_COMPUTE_PIPELINE = 0;

//==========================================================================
// Multi-Pipeline Sequencing
//==========================================================================

/**
 * @struct IndirectDispatch
 * @brief Optional GPU-resident source of a stage's workgroup counts.
 *
 * The buffer holds three consecutive uint32 values {x, y, z} at offset, and
 * must have been created with indirect usage. A null buffer means the stage
 * dispatches its fixed counts.
 *
 * The counts are expected to be written by an earlier compute stage of the
 * same sequence, or by the host before submission. Any offset may be used, so
 * one buffer can hold the counts of several stages.
 */
struct IndirectDispatch {
    vk::Buffer buffer {};
    vk::DeviceSize offset { 0 };
};

/**
 * @struct ComputeStage
 * @brief One pipeline dispatch within a ComputePress::record_sequence call.
 *
 * Bundles everything needed to bind a pipeline, push constants and dispatch.
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
     * @brief Writes for the pipeline's push descriptor set, recorded before
     *        this stage's dispatch. Empty for a pipeline without a push set.
     */
    std::vector<PushDescriptorWrite> push_writes;

    /**
     * @brief When set, the stage dispatches with counts read from this buffer
     *        and groups is ignored.
     *
     * record_sequence orders the read after earlier shader writes to the
     * buffer.
     */
    IndirectDispatch indirect;
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

    /**
     * @brief Check if ComputePress is initialized
     */
    [[nodiscard]] bool is_initialized() const { return m_shader_foundry != nullptr; }

    /**
     * @brief Check if a pipeline id names a live pipeline
     * @param pipeline_id Id returned by create_pipeline
     */
    [[nodiscard]] bool has_pipeline(ComputePipelineID pipeline_id) const { return m_pipelines.contains(pipeline_id); }

    //==========================================================================
    // Pipeline Creation (COMPUTE-SPECIFIC)
    //==========================================================================

    /**
     * @brief Create compute pipeline
     * @param push_set Index into descriptor_sets of the set to create as a push
     *        descriptor set; it must be the first or the last set. That set is
     *        written with push_descriptors() and gets no allocated set.
     */
    ComputePipelineID create_pipeline(
        ShaderID shader_id,
        const std::vector<std::vector<DescriptorBindingInfo>>& descriptor_sets = {},
        size_t push_constant_size = 0,
        std::optional<size_t> push_set = std::nullopt);

    /**
     * @brief Create pipeline with auto-reflection
     * @param push_set Index of the reflected set to create as a push descriptor set
     */
    ComputePipelineID create_pipeline_auto(
        ShaderID shader_id,
        size_t push_constant_size = 0,
        std::optional<size_t> push_set = std::nullopt);

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
     * @brief Write the pipeline's push descriptor set into the command buffer
     *
     * Does nothing and logs an error when the pipeline has no push set.
     * @param leases Slots the writes point into; the command buffer keeps each
     *        claimed until its submission completes.
     */
    void push_descriptors(
        CommandBufferID cmd_id,
        ComputePipelineID pipeline_id,
        const std::vector<PushDescriptorWrite>& writes,
        std::span<const SlotLease> leases = {});

    /**
     * @brief Push constants to active command buffer
     */
    void push_constants(
        CommandBufferID cmd_id,
        ComputePipelineID pipeline_id,
        const void* data,
        size_t size);

    /**
     * @brief Push part of the push constant block, starting at a byte offset
     * @param offset Byte offset into the block
     *
     * The range [offset, offset + size) must lie inside the push constant size
     * the pipeline was created with; otherwise nothing is recorded.
     */
    void push_constants(
        CommandBufferID cmd_id,
        ComputePipelineID pipeline_id,
        const void* data,
        size_t size,
        uint32_t offset);

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
     *        command buffer, with a barrier after each stage.
     *
     * Does not submit; the caller owns the command buffer's lifetime via
     * ShaderFoundry (begin_commands, then submit_and_wait or submit_async).
     *
     * Every stage is followed by one memory barrier from compute shader
     * writes to compute shader reads and writes and indirect command reads,
     * so each later stage sees every earlier stage's writes to buffers and
     * General layout images alike.
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
        size_t push_constant_size {};
        std::optional<size_t> push_set;
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
