#pragma once

#include "MayaFlux/Portal/Graphics/ComputePress.hpp"

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

    void ensure_buffer(const std::string& key, size_t index, size_t required_bytes);
    void upload(const std::string& key, size_t index, const float* data, size_t byte_size);
    void upload_raw(const std::string& key, size_t index, const uint8_t* data, size_t byte_size);
    void download(const std::string& key, size_t index, float* dest, size_t byte_size);
    void bind_descriptor(const std::string& key, size_t index, const GpuBufferBinding& spec);

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
     * @brief Transition a VKImage layout via an immediate command submission.
     *
     * Not tied to any pipeline unit; operates on the image directly.
     * @param image      Image to transition.
     * @param old_layout Source layout.
     * @param new_layout Target layout.
     */
    void transition_image(const std::shared_ptr<Core::VKImage>& image,
        vk::ImageLayout old_layout,
        vk::ImageLayout new_layout);

    void dispatch(const std::string& key,
        const std::array<uint32_t, 3>& groups,
        const std::vector<GpuBufferBinding>& bindings,
        const uint8_t* push_constant_data,
        size_t push_constant_size);

    void dispatch_batched(const std::string& key,
        uint32_t pass_count,
        const std::array<uint32_t, 3>& groups,
        const std::vector<GpuBufferBinding>& bindings,
        const std::function<void(uint32_t pass, std::vector<uint8_t>&)>& push_constant_updater,
        size_t push_constant_size,
        const std::unordered_map<std::string, std::any>& execution_metadata = {});

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
     * @param bindings           Binding declarations; output barriers inserted.
     * @param push_constant_data Pointer to push constant bytes, or nullptr.
     * @param push_constant_size Byte count of push constant data.
     * @return FenceID to poll with ShaderFoundry::is_fence_signaled.
     *         Returns INVALID_FENCE if not ready or submission fails.
     */
    [[nodiscard]] Portal::Graphics::FenceID dispatch_async(const std::string& key,
        const std::array<uint32_t, 3>& groups,
        const std::vector<GpuBufferBinding>& bindings,
        const uint8_t* push_constant_data,
        size_t push_constant_size);

    /**
     * @brief Record a dispatch for each requested key into one command
     *        buffer via ComputePress::record_sequence, submitted once.
     *
     * Each key must already be initialise()'d. Vectors are parallel,
     * indexed by position, one entry per key in the same order as keys.
     */
    void dispatch_sequence(
        const std::vector<std::string>& keys,
        const std::vector<std::array<uint32_t, 3>>& groups_per_key,
        const std::vector<std::vector<uint8_t>>& push_constants_per_key,
        const std::vector<std::vector<Portal::Graphics::HazardResource>>& hazards_per_key);

    /**
     * @brief Destroy the pipeline, shader, descriptor sets, and buffers
     *        for a single key, without affecting any other key.
     */
    void release(const std::string& key);

    /**
     * @brief Destroy every key currently held. Called from the destructor.
     */
    void cleanup();

    [[nodiscard]] bool is_ready(const std::string& key) const;
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

    [[nodiscard]] PipelineUnit& unit_for(const std::string& key);
    [[nodiscard]] const PipelineUnit* find_unit(const std::string& key) const;

    std::unordered_map<std::string, std::unique_ptr<PipelineUnit>> m_units;
}; // class GpuResourceManager

} // namespace MayaFlux::Yantra
