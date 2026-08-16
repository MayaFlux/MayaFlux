#pragma once

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Kinesis/Spatial/Lattice.hpp"

namespace MayaFlux::Buffers {

/**
 * @class RaymarchProcessor
 * @brief Stages a scalar field and its march parameters onto the buffer a
 *        RenderProcessor draws, so the fragment stage integrates the field
 *        through a proxy volume.
 *
 * Dispatches nothing and records no commands. Each cycle it resolves a
 * vk::Buffer from its field source, writes a DescriptorBindingInfo for it
 * into the attached buffer's pipeline context, and writes its parameter
 * block as a staged push constant fragment. RenderProcessor unifies both
 * into the pipeline it builds and updates the descriptor before binding
 * the set, which is the designed path for handing resources to the
 * renderer without a processor of its own.
 *
 * The field source is a callable rather than a handle because a
 * double-buffered field's read slot moves whenever an upstream stage
 * swaps. Calling it fresh each cycle picks that up. It is also what keeps
 * this class free of any dependency on VolumeGridBuffer: any callable
 * returning a valid handle to a float array of the lattice's cell count
 * works, whatever owns it.
 *
 * Set 1 binding 0 by default. Set 0 is the engine's ViewTransform
 * reservation and cannot be used.
 */
class MAYAFLUX_API RaymarchProcessor : public VKBufferProcessor {
public:
    /**
     * @brief Callable resolving the handle to sample this cycle.
     *
     * Returning a null handle skips the cycle's staging, leaving the
     * previous cycle's descriptor in place.
     */
    using FieldSource = std::function<vk::Buffer()>;

    /**
     * @struct MarchParams
     * @brief Parameter block staged as a push constant fragment.
     *
     * Offsets are fixed by this declaration. The fragment is staged at
     * offset zero, so these are absolute offsets in the render pipeline's
     * push constant range.
     */
    struct MarchParams {
        uint32_t width;
        uint32_t height;
        uint32_t depth;
        uint32_t max_steps;
        float bounds_min_x;
        float bounds_min_y;
        float bounds_min_z;
        float step_scale;
        float cell_size_x;
        float cell_size_y;
        float cell_size_z;
        float density_scale;
        float cool_r;
        float cool_g;
        float cool_b;
        float absorption;
        float hot_r;
        float hot_g;
        float hot_b;
        float emission;
        float threshold;
        float pad0;
        float pad1;
        float pad2;
    };

    static_assert(sizeof(MarchParams) == 96);
    static_assert(sizeof(MarchParams) % 16 == 0);

    /**
     * @brief Construct a march staging processor.
     * @param source Callable resolving the field handle each cycle.
     * @param field_bytes Byte size of one slot of that field.
     * @param lattice Discretization the field is stored over. Must match
     *        the lattice the field was allocated against.
     * @param set Descriptor set index. Must not be zero.
     * @param binding Binding index within that set.
     */
    RaymarchProcessor(
        FieldSource source,
        size_t field_bytes,
        Kinesis::Lattice3D lattice,
        uint32_t set = 1,
        uint32_t binding = 0);

    ~RaymarchProcessor() override = default;

    /**
     * @brief Set the sample count along the longest ray through the volume.
     * @param steps Step count. Higher resolves thin structure, linearly
     *        more expensive per covered pixel.
     */
    void set_max_steps(uint32_t steps);

    /**
     * @brief Set the step length as a fraction of one cell.
     * @param scale Below one oversamples, above one undersamples and bands.
     */
    void set_step_scale(float scale);

    /**
     * @brief Set the multiplier applied to every sample before integration.
     * @param scale Raises or lowers apparent opacity without touching the field.
     */
    void set_density_scale(float scale);

    /**
     * @brief Set the extinction coefficient.
     * @param absorption Higher makes the volume opaque in a shorter distance.
     */
    void set_absorption(float absorption);

    /**
     * @brief Set the emission colours interpolated by sample value.
     * @param cool Colour at the threshold.
     * @param hot Colour at and above unity.
     */
    void set_emission_ramp(const glm::vec3& cool, const glm::vec3& hot);

    /**
     * @brief Set the emissive strength.
     * @param emission Zero gives pure absorption, a density shadow.
     */
    void set_emission(float emission);

    /**
     * @brief Set the sample value below which a step contributes nothing.
     * @param threshold Cutoff. Suppresses the smeared tail advection leaves.
     */
    void set_threshold(float threshold);

    /** @brief The lattice the sampled field is stored over. */
    [[nodiscard]] const Kinesis::Lattice3D& get_lattice() const { return m_lattice; }

    /** @brief The staged parameter block. */
    [[nodiscard]] const MarchParams& get_params() const { return m_params; }

    void on_attach(const std::shared_ptr<Buffer>& buffer) override;
    void processing_function(const std::shared_ptr<Buffer>& buffer) override;

private:
    /**
     * @brief Write the lattice-derived words of the parameter block.
     */
    void write_lattice_params();

    /**
     * @brief Insert or replace the descriptor entry for the field handle.
     * @param buffer Buffer whose pipeline context is staged onto.
     * @param handle Handle resolved this cycle.
     */
    void stage_descriptor(const std::shared_ptr<VKBuffer>& buffer, vk::Buffer handle);

    /**
     * @brief Insert or replace the push constant fragment.
     * @param buffer Buffer whose pipeline context is staged onto.
     */
    void stage_params(const std::shared_ptr<VKBuffer>& buffer);

    FieldSource m_source;
    size_t m_field_bytes;
    Kinesis::Lattice3D m_lattice;
    uint32_t m_set;
    uint32_t m_binding;

    MarchParams m_params {};
};

} // namespace MayaFlux::Buffers
