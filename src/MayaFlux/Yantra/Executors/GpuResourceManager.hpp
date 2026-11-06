#pragma once

#include "MayaFlux/Portal/Graphics/ComputePress.hpp"
#include "MayaFlux/Yantra/OperationSpec/ExecutionContext.hpp"

namespace MayaFlux::Yantra {

using GpuComputeConfig = Portal::Graphics::GpuComputeConfig;
using GpuBufferBinding = Portal::Graphics::GpuBufferBinding;

struct GpuResourceManagerImpl;

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
    GpuResourceManager();
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
    void bind_descriptor(const std::string& key, size_t index, const GpuBufferBinding& spec);

    /**
     * @brief Bind every non-image buffer binding in one Vulkan call.
     *
     * Equivalent to calling bind_descriptor or bind_shared_descriptor once
     * per non-image entry in bindings (routed the same way, by whether a
     * shared slot exists at that (set, binding)), except every entry is
     * submitted through a single vkUpdateDescriptorSets call. Image entries
     * are skipped, matching bind_all_descriptors' existing split between
     * buffer descriptors here and image descriptors bound separately per
     * staged image.
     */
    void bind_descriptors_batch(const std::string& key, const std::vector<GpuBufferBinding>& bindings);

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
    void bind_shared_descriptor(const std::string& key, uint32_t set, size_t binding_index, const GpuBufferBinding& spec);
    void download_shared(uint32_t set, size_t binding_index, void* dest, size_t byte_size);
    void upload_shared_raw(uint32_t set, size_t binding_index, const uint8_t* data, size_t byte_size);

    /**
     * @brief Bind a storage image descriptor at the given slot index.
     * @param key     Pipeline unit to bind into.
     * @param index   Slot index matching the binding declaration.
     * @param image   VKImage to bind. Must be initialised and in eGeneral layout.
     * @param spec    Binding declaration. element_type must be IMAGE_STORAGE.
     */
    void bind_image_storage(const std::string& key, size_t index,
        const std::shared_ptr<Core::VKImage>& image,
        const GpuBufferBinding& spec);

    /**
     * @brief Bind a combined image+sampler descriptor at the given slot index.
     * @param key     Pipeline unit to bind into.
     * @param index   Slot index matching the binding declaration.
     * @param image   VKImage to bind. Must be in eShaderReadOnlyOptimal layout.
     * @param sampler Vulkan sampler handle from SamplerForge.
     * @param spec    Binding declaration. element_type must be IMAGE_SAMPLED.
     */
    void bind_image_sampled(const std::string& key, size_t index,
        const std::shared_ptr<Core::VKImage>& image,
        vk::Sampler sampler,
        const GpuBufferBinding& spec);

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
     * @brief Bind every image in one Vulkan call.
     *
     * Equivalent to calling bind_image_storage or bind_image_sampled once
     * per entry (routed by spec.element_type), except every entry is
     * submitted through a single vkUpdateDescriptorSets call. Also records
     * each image in the pipeline unit's image_slots, same as the per-call
     * methods.
     */
    void bind_images_batch(const std::string& key, const std::vector<ImageBind>& images);

    /**
     * @brief Transition a VKImage to a new layout, ordered before the next dispatch.
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
     * fixed groups. Every stage is followed by a barrier, so each stage sees
     * the writes of all earlier ones.
     */
    void dispatch_sequence(
        const std::vector<std::string>& keys,
        const std::vector<std::array<uint32_t, 3>>& groups_per_key,
        const std::vector<std::vector<uint8_t>>& push_constants_per_key,
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
     * @brief Begin a command buffer for Yantra dispatch work.
     *
     * Dispatches record on the graphics queue, where the images and buffers
     * they write are consumed, so no queue family ownership transfer is needed.
     */
    [[nodiscard]] static Portal::Graphics::CommandBufferID begin_dispatch_commands();
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
        const std::vector<Portal::Graphics::IndirectDispatch>& indirect_per_key);

    struct SharedBuffers;
    std::unique_ptr<SharedBuffers> m_shared;

    [[nodiscard]] PipelineUnit& unit_for(const std::string& key);
    [[nodiscard]] const PipelineUnit* find_unit(const std::string& key) const;

    std::unordered_map<std::string, std::unique_ptr<PipelineUnit>> m_units;
}; // class GpuResourceManager

} // namespace MayaFlux::Yantra
