#pragma once

#include "MayaFlux/Portal/Graphics/ComputePress.hpp"
#include "MayaFlux/Yantra/OperationSpec/ExecutionContext.hpp"

namespace MayaFlux::Yantra {

using GpuComputeConfig = Portal::Graphics::GpuComputeConfig;
using GpuBufferBinding = Portal::Graphics::GpuBufferBinding;

struct GpuResourceManagerImpl;
/**
 * @class GpuDispatchRecorder
 * @brief Shared command buffer for GPU dispatch work.
 *
 * Several GpuResourceManagers can append to one recorder when their compute
 * isolation setting agrees. Submission happens at a readback, an owner-defined
 * handoff, or an explicit submit(). Pending releases remain alive through it.
 */
class MAYAFLUX_API GpuDispatchRecorder {
public:
    /**
     * @param compute_isolated True for work confined to this compute workflow;
     *        false for resources shared with graphics. Isolated work uses the
     *        graphics queue when no distinct compute queue is available.
     */
    explicit GpuDispatchRecorder(bool compute_isolated = true)
        : m_compute_isolated(compute_isolated)
    {
    }
    ~GpuDispatchRecorder();

    GpuDispatchRecorder(const GpuDispatchRecorder&) = delete;
    GpuDispatchRecorder& operator=(const GpuDispatchRecorder&) = delete;
    GpuDispatchRecorder(GpuDispatchRecorder&&) = delete;
    GpuDispatchRecorder& operator=(GpuDispatchRecorder&&) = delete;

    /**
     * @brief The command buffer to record into, begun on first use.
     */
    [[nodiscard]] Portal::Graphics::CommandBufferID command();

    /**
     * @brief True while commands are recorded and not yet submitted.
     */
    [[nodiscard]] bool is_open() const { return m_cmd != Portal::Graphics::INVALID_COMMAND_BUFFER; }
    [[nodiscard]] bool compute_isolated() const { return m_compute_isolated; }

    /**
     * @brief Run release once everything recorded so far has completed.
     *
     * Held until the next submit while the recorder is open; otherwise handed
     * to ShaderFoundry::release_after_submit at once.
     */
    void release_after(std::function<void()> release);

    /**
     * @brief Submit what has been recorded, without waiting.
     *
     * Ends the recording with a barrier that orders later commands and host
     * reads behind its writes. Does nothing when nothing is recorded.
     *
     * @return The fence of the most recent submission, owned by the
     *         recorder: poll it with is_complete(), never release it.
     *         INVALID_FENCE when nothing is outstanding.
     */
    Portal::Graphics::FenceID submit();

    /**
     * @brief True once the submission behind fence has completed.
     *
     * Every submission goes to the selected queue in order and ends with the
     * closing barrier, so the latest one completes after all earlier ones.
     * Only the latest fence is kept; an earlier fence counts as complete once
     * the latest has signaled.
     */
    [[nodiscard]] bool is_complete(Portal::Graphics::FenceID fence);

    /**
     * @brief Submit what has been recorded and wait until every submission of
     *        this recorder has completed.
     */
    void wait();

    /**
     * @brief Record the barrier every submission of Yantra dispatch work ends with.
     *
     * Orders every later command, and host reads after completion, behind the
     * compute and transfer writes recorded so far.
     */
    static void record_closing_barrier(Portal::Graphics::CommandBufferID cmd_id);

private:
    bool m_compute_isolated;
    Portal::Graphics::CommandBufferID m_cmd { Portal::Graphics::INVALID_COMMAND_BUFFER };
    Portal::Graphics::FenceID m_last { Portal::Graphics::INVALID_FENCE };
    std::vector<std::function<void()>> m_releases;
};

/**
 * @class GpuResourceManager
 * @brief Encapsulates all Vulkan resource lifecycle behind Portal facades.
 *
 * Manages a keyed set of pipelines, each with its own storage buffers,
 * pipeline, shader, and descriptor sets. There is no distinct single-
 * pipeline mode: a caller using exactly one shader simply uses one key
 * throughout. PIMPL hides vk:: types from the header entirely.
 */
class MAYAFLUX_API GpuResourceManager {
public:
    /**
     * @param compute_isolated True when GPU resources stay within this compute
     *        workflow. False when work shares GPU resources with graphics.
     */
    explicit GpuResourceManager(bool compute_isolated = true);
    ~GpuResourceManager();

    GpuResourceManager(const GpuResourceManager&) = delete;
    GpuResourceManager& operator=(const GpuResourceManager&) = delete;
    GpuResourceManager(GpuResourceManager&&) = delete;
    GpuResourceManager& operator=(GpuResourceManager&&) = delete;

    /**
     * @brief Create (or confirm existing) pipeline for the given key.
     *
     * Create-if-absent: if a unit already exists and is ready for this
     * key, returns true immediately without recreating anything. Call
     * release(key) first to force recreation with a different config.
     */
    bool initialise(const std::string& key,
        const GpuComputeConfig& config,
        const std::vector<GpuBufferBinding>& bindings);

    void ensure_buffer(const std::string& key, size_t index, size_t required_bytes,
        Portal::Graphics::BufferUsageHint usage_hint = Portal::Graphics::BufferUsageHint::COMPUTE_STORAGE);
    void upload(const std::string& key, size_t index, const float* data, size_t byte_size);
    void upload_raw(const std::string& key, size_t index, const uint8_t* data, size_t byte_size);
    void download(const std::string& key, size_t index, float* dest, size_t byte_size);

    /**
     * @brief Bind every non-image buffer binding of a unit.
     *
     * An entry resolves to the shared buffer at its (set, binding) when one
     * exists, otherwise to the unit's own buffer at that binding. Set 0
     * bindings become push descriptor writes, recorded into each command
     * buffer that dispatches the unit. Bindings in other sets are written into
     * the unit's allocated sets in one vkUpdateDescriptorSets call, and only
     * when the bound buffer or its size changed since the last write. Image
     * entries are skipped; they go through bind_images_batch.
     */
    void bind_descriptors_batch(const std::string& key, const std::vector<GpuBufferBinding>& bindings);

    /**
     * @brief The unit's current set 0 push descriptor writes.
     *
     * Copied into each stage of a dependency sequence at staging time, so
     * stages of the same unit that bind different resources keep their own.
     * Empty when the unit has no set 0.
     */
    [[nodiscard]] const std::vector<Portal::Graphics::PushDescriptorWrite>& push_writes(const std::string& key);

    void ensure_shared_buffer(uint32_t set, size_t binding_index, size_t element_count,
        GpuBufferBinding::ElementType element_type,
        Portal::Graphics::BufferUsageHint usage_hint = Portal::Graphics::BufferUsageHint::COMPUTE_STORAGE);

    /**
     * @brief Non-owning view of the shared buffer at (set, binding_index).
     *
     * The manager keeps ownership. A null handle is returned when no such
     * buffer exists, and the view is invalidated if ensure_shared_buffer later
     * grows the slot.
     */
    [[nodiscard]] Portal::Graphics::GpuBufferHandle shared_buffer_handle(uint32_t set, size_t binding_index) const;
    void download_shared(uint32_t set, size_t binding_index, void* dest, size_t byte_size);
    void upload_shared_raw(uint32_t set, size_t binding_index, const uint8_t* data, size_t byte_size);

    /**
     * @brief Fill the whole shared buffer at (set, binding_index) with one
     *        32-bit value.
     *
     * Recorded as vkCmdFillBuffer when a recording is attached, so a reset
     * runs in order with the dispatches around it; written from the host
     * otherwise.
     */
    void fill_shared(uint32_t set, size_t binding_index, uint32_t value);

    /**
     * @brief Append this manager's work to a recording with the same compute
     *        isolation setting, or pass null to submit again.
     *
     * Given once, by whoever owns the recording, before the first dispatch.
     * While attached:
     * - dispatch_async, dispatch_sequence_async, record_stage and the batched
     *   dispatches append to the recording and return INVALID_FENCE where a
     *   fence is returned;
     * - dispatch and dispatch_sequence append, then submit and wait;
     * - uploads and fills are recorded in order with the dispatches;
     * - downloads submit and wait for the recording before reading;
     * - image transitions are recorded into it;
     * - retired buffers and pipelines are freed only after the recording's
     *   next submission completes.
     */
    void set_recording(std::shared_ptr<GpuDispatchRecorder> recording);

    [[nodiscard]] bool has_recording() const { return m_recording != nullptr; }

    /**
     * @brief Record one dispatch of a unit with its current bindings, push
     *        constants and optional indirect counts.
     *
     * Appended to the attached recording; without one, submitted and awaited
     * on its own.
     */
    void record_stage(const std::string& key,
        const std::array<uint32_t, 3>& groups,
        const std::vector<uint8_t>& push_constants,
        const Portal::Graphics::IndirectDispatch& indirect = {});

    /**
     * @brief One image slot for bind_images_batch: a storage image entry
     *        leaves sampler null.
     */
    struct ImageBind {
        size_t index;
        std::shared_ptr<Core::VKImage> image;
        vk::Sampler sampler;
        GpuBufferBinding spec;
    };

    /**
     * @brief Bind every image of a unit, routed by spec.element_type.
     *
     * Storage images bind in General layout, sampled images in
     * ShaderReadOnlyOptimal with entry.sampler. Set 0 images become push
     * descriptor writes; images in other sets are written in one
     * vkUpdateDescriptorSets call. Each image is also kept in the unit's
     * image_slots.
     */
    void bind_images_batch(const std::string& key, const std::vector<ImageBind>& images);

    /**
     * @brief Transition a VKImage to a new layout, ordered before the next dispatch.
     *
     * Recorded into the attached recording, covering every mip level and
     * layer, so it runs after the recorded work that wrote the image; queued
     * as deferred work through TextureLoom otherwise.
     *
     * Not tied to any pipeline unit; operates on the image directly.
     * @param image      Image to transition.
     * @param new_layout Target layout.
     */
    void transition_image(const std::shared_ptr<Core::VKImage>& image,
        vk::ImageLayout new_layout);

    /**
     * @brief Record one dispatch, submit it and wait.
     *
     * Every submission from this class ends with a barrier that orders later
     * commands and host reads after completion behind its compute writes.
     */
    void dispatch(const std::string& key,
        const std::array<uint32_t, 3>& groups,
        const uint8_t* push_constant_data,
        size_t push_constant_size);

    /**
     * @brief Record ChainedParams::pass_count dispatches in batches, with a
     *        barrier after every pass, waiting for each batch.
     */
    void dispatch_batched(const std::string& key,
        const std::array<uint32_t, 3>& groups,
        size_t push_constant_size,
        const ExecutionContext& ctx);

    /**
     * @brief dispatch_batched with counts read from the shared buffer at
     *        (indirect_set, indirect_binding), seeded with groups.
     */
    void dispatch_batched_indirect(const std::string& key,
        uint32_t indirect_set, size_t indirect_binding,
        const std::array<uint32_t, 3>& groups,
        size_t push_constant_size,
        const ExecutionContext& ctx);

    /**
     * @brief Submit a compute dispatch without blocking.
     *
     * Records the same command sequence as dispatch() but submits via
     * ShaderFoundry::submit_async instead of submit_and_wait. Returns a
     * FenceID immediately. Readback must be deferred until
     * ShaderFoundry::is_fence_signaled returns true for the returned ID.
     *
     * @param key                Pipeline unit to dispatch.
     * @param groups             Workgroup counts {x, y, z}.
     * @param push_constant_data Pointer to push constant bytes, or nullptr.
     * @param push_constant_size Byte count of push constant data.
     * @return FenceID to poll with ShaderFoundry::is_fence_signaled.
     *         Returns INVALID_FENCE if not ready or submission fails.
     */
    [[nodiscard]] Portal::Graphics::FenceID dispatch_async(const std::string& key,
        const std::array<uint32_t, 3>& groups,
        const uint8_t* push_constant_data,
        size_t push_constant_size);

    /**
     * @brief Record a dispatch for each requested key into one command
     *        buffer via ComputePress::record_sequence, submitted once.
     *
     * Each key must already be initialise()'d. Vectors are parallel,
     * indexed by position, one entry per key in the same order as keys.
     * indirect_per_key is either empty or parallel too; a stage whose entry
     * has a buffer dispatches with the counts stored there instead of its
     * fixed groups. push_writes_per_key is parallel too and holds each
     * stage's set 0 push descriptor writes. Every stage is followed by a
     * barrier, so each stage sees the writes of all earlier ones.
     */
    void dispatch_sequence(
        const std::vector<std::string>& keys,
        const std::vector<std::array<uint32_t, 3>>& groups_per_key,
        const std::vector<std::vector<uint8_t>>& push_constants_per_key,
        const std::vector<std::vector<Portal::Graphics::PushDescriptorWrite>>& push_writes_per_key,
        const std::vector<Portal::Graphics::IndirectDispatch>& indirect_per_key = {});

    /**
     * @brief Non-blocking counterpart of dispatch_sequence.
     *
     * Records the identical command buffer and submits it via
     * ShaderFoundry::submit_async instead of submit_and_wait.
     *
     * @return FenceID to poll with ShaderFoundry::is_fence_signaled and to
     *         release once signaled.
     */
    [[nodiscard]] Portal::Graphics::FenceID dispatch_sequence_async(
        const std::vector<std::string>& keys,
        const std::vector<std::array<uint32_t, 3>>& groups_per_key,
        const std::vector<std::vector<uint8_t>>& push_constants_per_key,
        const std::vector<std::vector<Portal::Graphics::PushDescriptorWrite>>& push_writes_per_key,
        const std::vector<Portal::Graphics::IndirectDispatch>& indirect_per_key = {});

    /**
     * @brief Destroy the pipeline, shader, descriptor sets, and buffers
     *        for a single key, without affecting any other key.
     *
     * The pipeline and buffers are released once work submitted so far has
     * completed.
     */
    void release(const std::string& key);

    /**
     * @brief Wait for submitted work, then destroy every key and shared buffer.
     *        Called from the destructor.
     */
    void cleanup();

    [[nodiscard]] bool is_ready(const std::string& key) const;

    /**
     * @brief Begin Yantra dispatch work on the selected queue.
     *
     * Isolated work uses a distinct compute queue when available. Shared work
     * and devices without a distinct compute queue use the graphics queue.
     */
    [[nodiscard]] static Portal::Graphics::CommandBufferID begin_dispatch_commands(bool compute_isolated);
    [[nodiscard]] size_t buffer_allocated_bytes(const std::string& key, size_t index) const;

private:
    struct PipelineUnit {
        Portal::Graphics::ShaderID shader_id { Portal::Graphics::INVALID_SHADER };
        Portal::Graphics::ComputePipelineID pipeline_id { Portal::Graphics::INVALID_COMPUTE_PIPELINE };
        std::vector<Portal::Graphics::DescriptorSetID> descriptor_set_ids;

        std::unique_ptr<GpuResourceManagerImpl> impl;

        struct BufferSlot {
            size_t allocated_bytes {};
        };
        std::vector<BufferSlot> buffer_slots;
        std::vector<std::shared_ptr<Core::VKImage>> image_slots;

        bool ready {};
    };

    /**
     * @brief Record one command buffer holding a dispatch per key, a barrier
     *        after each, and the trailing barrier.
     *
     * Shared by dispatch_sequence and dispatch_sequence_async, which differ
     * only in how they submit the returned buffer.
     */
    [[nodiscard]] Portal::Graphics::CommandBufferID record_sequence_commands(
        const std::vector<std::string>& keys,
        const std::vector<std::array<uint32_t, 3>>& groups_per_key,
        const std::vector<std::vector<uint8_t>>& push_constants_per_key,
        const std::vector<std::vector<Portal::Graphics::PushDescriptorWrite>>& push_writes_per_key,
        const std::vector<Portal::Graphics::IndirectDispatch>& indirect_per_key);

    /**
     * @brief The attached recording's command buffer, or a new one when none
     *        is attached.
     */
    [[nodiscard]] Portal::Graphics::CommandBufferID open_commands();

    /**
     * @brief Record one dispatch of unit with its push descriptor writes and
     *        the barrier that follows every dispatch.
     */
    static void record_unit(Portal::Graphics::CommandBufferID cmd_id, const PipelineUnit& unit,
        const std::array<uint32_t, 3>& groups,
        const uint8_t* push_constant_data,
        size_t push_constant_size,
        const Portal::Graphics::IndirectDispatch& indirect = {});

    /**
     * @brief Submit cmd_id and wait: the attached recording, or the command
     *        buffer itself ended with the closing barrier.
     */
    void finish_commands(Portal::Graphics::CommandBufferID cmd_id);

    bool m_compute_isolated;
    std::shared_ptr<GpuDispatchRecorder> m_recording;

    struct SharedBuffers;
    std::unique_ptr<SharedBuffers> m_shared;

    [[nodiscard]] PipelineUnit& unit_for(const std::string& key);
    [[nodiscard]] const PipelineUnit* find_unit(const std::string& key) const;

    std::unordered_map<std::string, std::unique_ptr<PipelineUnit>> m_units;
}; // class GpuResourceManager

} // namespace MayaFlux::Yantra
