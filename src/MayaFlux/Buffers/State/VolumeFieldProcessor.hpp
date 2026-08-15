#pragma once

#include "MayaFlux/Buffers/Shaders/ComputeProcessor.hpp"

namespace MayaFlux::Buffers {

class VolumeGridBuffer;

/**
 * @class VolumeFieldProcessor
 * @brief ComputeProcessor operating on named fields of a VolumeGridBuffer.
 *
 * Holds everything the lattice stages share: validating field names
 * against the attached volume, sizing the dispatch from the lattice,
 * writing the common leading push constant words, rewriting field
 * descriptors every cycle, and swapping slots after the dispatch.
 * Subclasses supply only a binding table and whatever parameter words
 * follow the shared prefix.
 *
 * The binding table drives both descriptor writes and validation. A field
 * appearing with both READ and WRITE access is required to be
 * double-buffered, since the stage would otherwise read a neighbourhood
 * of the storage it is writing. A field appearing only as WRITE carries
 * no such requirement.
 *
 * Descriptors are written directly through
 * ShaderFoundry::update_descriptor_buffer rather than bind_buffer, since
 * VolumeGridBuffer's field storage consists of raw handle pairs with no
 * VKBuffer wrapper. The write happens in processing_function, before
 * execute_shader binds the set into the command buffer, because a
 * descriptor set already bound into an open command buffer cannot be
 * rewritten.
 *
 * Slot swaps happen in processing_function after the parent call rather
 * than in on_after_execute, which fires more than once per cycle. A swap
 * is not idempotent.
 */
class MAYAFLUX_API VolumeFieldProcessor : public ComputeProcessor {
public:
    /**
     * @enum FieldAccess
     * @brief Which slot of a field a binding resolves to.
     */
    enum class FieldAccess {
        READ, ///< Resolves to VolumeGridBuffer::read_handle.
        WRITE ///< Resolves to VolumeGridBuffer::write_handle.
    };

    /**
     * @struct FieldBinding
     * @brief One shader binding and the field slot it draws from.
     */
    struct FieldBinding {
        std::string name; ///< Shader binding name, as declared in ShaderConfig.
        uint32_t binding; ///< Binding index within set 0.
        std::string field; ///< Field name on the attached volume.
        FieldAccess access; ///< Which slot of that field.
    };

    /**
     * @struct LatticeParams
     * @brief Leading words of every volume stage's push constant block.
     *
     * Words three and seven are left to the subclass: PressureProcessor
     * carries parity in word three, AdvectProcessor carries its time step
     * in word seven, and the others leave both zero. Parameters beyond
     * word seven are written through write_param_tail.
     */
    struct LatticeParams {
        uint32_t width;
        uint32_t height;
        uint32_t depth;
        uint32_t word3;
        float cell_size_x;
        float cell_size_y;
        float cell_size_z;
        float word7;
    };

    /** @brief The attached volume, or null if attachment failed validation. */
    [[nodiscard]] const std::shared_ptr<VolumeGridBuffer>& get_volume() const { return m_volume; }

protected:
    /**
     * @brief Construct from a shader path.
     * @param bindings Binding table. Entries are registered into
     *        m_config.bindings in order.
     * @param shader_path Path to the compute shader.
     */
    VolumeFieldProcessor(std::vector<FieldBinding> bindings, const std::string& shader_path);

    /**
     * @brief Construct from a generated ShaderSpec.
     * @param bindings Binding table.
     * @param spec ShaderSpec implementing the stage.
     */
    VolumeFieldProcessor(std::vector<FieldBinding> bindings, const Portal::Graphics::ShaderSpec& spec);

    /**
     * @brief Cache and validate the volume, size the dispatch, write the
     *        shared parameter prefix, then call on_volume_ready.
     * @param buffer The attached buffer, expected to be a VolumeGridBuffer.
     *
     * On any validation failure the cached volume is reset, which makes
     * on_before_execute reject every subsequent cycle.
     */
    void on_attach(const std::shared_ptr<Buffer>& buffer) override;

    /**
     * @brief Write every binding in the table for the current slot assignment.
     */
    void on_descriptors_created() override;

    /**
     * @brief Reject buffers that are not the validated volume.
     * @param cmd_id Command buffer this cycle's dispatch will be recorded into.
     * @param buffer The attached buffer, received as VKBuffer.
     * @return True if a volume survived validation and matches the argument.
     */
    bool on_before_execute(Portal::Graphics::CommandBufferID cmd_id, const std::shared_ptr<VKBuffer>& buffer) override;

    /**
     * @brief Rewrite field descriptors, run the shader, then swap.
     * @param buffer Buffer under processing.
     *
     * Read slots change whenever an upstream stage swaps, so the bindings
     * are rewritten every cycle rather than once.
     */
    void processing_function(const std::shared_ptr<Buffer>& buffer) override;

    /**
     * @brief Hook for subclass parameters, called at the end of on_attach.
     *
     * The volume is non-null and the shared prefix is already written when
     * this runs. Subclasses writing parameters past the prefix override
     * this rather than on_attach.
     */
    virtual void on_volume_ready() { }

    /**
     * @brief Whether this cycle's dispatch should be followed by a swap.
     * @return True by default.
     *
     * PressureProcessor overrides this: after an even number of Jacobi
     * passes the result is already in the read slot.
     */
    [[nodiscard]] virtual bool wants_swap() const { return true; }

    /**
     * @brief Register a field to swap after each dispatch.
     * @param field Field name. Order of registration is order of swap.
     */
    void add_swap_field(std::string field);

    /**
     * @brief Write the lattice dimensions and cell size into the staged
     *        push constant block, preserving word three.
     */
    void write_lattice_params();

    /**
     * @brief Write word three of the shared prefix.
     * @param value Subclass-defined word, typically a mode or parity flag.
     */
    void write_lattice_word3(uint32_t value);

    /**
     * @brief Write subclass parameters past the shared prefix.
     * @param offset Byte offset from the start of the push constant block.
     *        Values below sizeof(LatticeParams) are rejected.
     * @param data Source bytes.
     * @param size Byte count.
     */
    void write_param_tail(size_t offset, const void* data, size_t size);

    /**
     * @brief Raise the push constant block to at least this size.
     * @param size Byte count the subclass's full parameter struct occupies.
     *
     * Called from the subclass constructor. The base raises the block to
     * sizeof(LatticeParams) independently.
     */
    void reserve_param_size(size_t size);

private:
    /**
     * @brief Register the binding table into m_config.bindings.
     */
    void register_bindings();

    /**
     * @brief Check every named field exists and that read-write fields
     *        carry two slots.
     * @return True if the cached volume satisfies the binding table.
     */
    bool validate_fields();

    /**
     * @brief Size the manual dispatch to cover the lattice.
     */
    void size_dispatch();

    /**
     * @brief Issue descriptor writes for every entry in the binding table.
     */
    void write_field_descriptors();

    std::vector<FieldBinding> m_bindings;
    std::vector<std::string> m_swap_fields;

    std::shared_ptr<VolumeGridBuffer> m_volume; ///< The attached volume, null until on_attach validates it.
};

} // namespace MayaFlux::Buffers
