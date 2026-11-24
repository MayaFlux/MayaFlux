#pragma once

#include "MayaFlux/Buffers/VKBuffer.hpp"

namespace MayaFlux::Buffers {

/**
 * @class StateFieldBuffer
 * @brief A VKBuffer that carries named auxiliary state fields beside its own storage.
 *
 * Every spatial stage (hash, claim, population, the field pass) and the field
 * read and write processors need only this: a buffer whose vertex storage is
 * its own and whose named fields they can look up, bind and swap. Nothing here
 * knows what fills the vertex storage, so a buffer fed by a NodeNetwork and one
 * fed by anything else are the same to those stages.
 *
 * Storage lives as raw handle pairs in the base VKBuffer's back_buffers,
 * addressed by name rather than wrapped in a VKBuffer of its own. A processor
 * resolves the handle it needs through read_state_handle/write_state_handle and
 * writes its own descriptor against ShaderFoundry, the way VolumeFieldProcessor
 * does for volume fields.
 */
class MAYAFLUX_API StateFieldBuffer : public VKBuffer {
public:
    ~StateFieldBuffer() override = default;

    /**
     * @brief Declare a named auxiliary state field alongside the vertex data.
     * @param name Lookup key, unique within this buffer.
     * @param element_count Number of elements the field holds. Fixed at
     *        declaration: it does not track later growth of the vertex buffer
     *        itself. Declare at the expected element count.
     * @param stride_bytes Bytes per element.
     * @param double_buffered False resolves read and write to one slot,
     *        which suits a value recomputed from other state each cycle.
     * @return True if the field was registered.
     */
    bool declare_state(
        const std::string& name,
        size_t element_count,
        size_t stride_bytes,
        bool double_buffered = true);

    /** @brief Whether a state field of this name was declared. */
    [[nodiscard]] bool has_state(const std::string& name) const;

    /** @brief Byte size of one slot of the named state field, or 0 if undeclared. */
    [[nodiscard]] size_t get_state_bytes(const std::string& name) const;

    /**
     * @brief Handle a stage should read the named state field from.
     * @return Vulkan buffer handle, or nullptr if undeclared.
     */
    [[nodiscard]] vk::Buffer read_state_handle(const std::string& name) const;

    /**
     * @brief Full back_buffers slot a stage should read the named state field from.
     * @return The slot, or a default-constructed (null-handle) slot if undeclared.
     *
     * A CPU-side readback through StagingUtils::download_back_buffer needs the
     * whole slot (it checks mapped_ptr for the host-visible fast path), not
     * only the handle.
     */
    [[nodiscard]] VKBufferResources::GenerationSlot read_state_slot(const std::string& name) const;

    /**
     * @brief Handle a stage should write the named state field to.
     * @return Vulkan buffer handle, or nullptr if undeclared. Equals
     *         read_state_handle() for single-slot fields.
     */
    [[nodiscard]] vk::Buffer write_state_handle(const std::string& name) const;

    /**
     * @brief Full back_buffers slot a stage should write the named state field to.
     * @return The slot, or a default-constructed (null-handle) slot if undeclared.
     *         Equals read_state_slot() for single-slot fields.
     */
    [[nodiscard]] VKBufferResources::GenerationSlot write_state_slot(const std::string& name) const;

    /**
     * @brief Exchange read and write slots for the named state field.
     * @param name Field name. No effect on single-slot fields.
     *
     * Called by whichever stage last wrote the field, after its dispatch, so
     * the next stage reading it observes the new values.
     */
    void swap_state(const std::string& name);

    /**
     * @brief Ensure the hash_cluster_id state field exists.
     * @return True if hash_cluster_id exists by the time this returns.
     *
     * This base has no source of cluster ids, so it reports only whether the
     * field was already declared. A buffer that can derive them overrides this
     * to declare and populate the field, and keeps the has_state guard so
     * whichever caller reaches it first does the work.
     */
    virtual bool ensure_cluster_ids();

protected:
    /** @brief As VKBuffer's constructor. */
    StateFieldBuffer(size_t size_bytes, Usage usage, Kakshya::DataModality modality)
        : VKBuffer(size_bytes, usage, modality)
    {
    }

private:
    struct StateField {
        size_t element_count;
        size_t stride_bytes;
        uint32_t slot_a;
        uint32_t slot_b;
        bool read_is_a;
    };
    std::unordered_map<std::string, StateField> m_state_fields;

    /**
     * @brief Resolve a declared state field by name.
     * @param context Caller identifier used in the error path.
     * @return Pointer to the field, or nullptr with an error logged.
     */
    [[nodiscard]] const StateField* find_state_field(const std::string& name, const char* context) const;
};

} // namespace MayaFlux::Buffers
