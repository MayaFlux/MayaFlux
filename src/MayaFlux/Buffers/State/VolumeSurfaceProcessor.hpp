#pragma once

#include "MayaFlux/Buffers/Shaders/ComputeProcessor.hpp"

namespace MayaFlux::Buffers {

class VolumeGridBuffer;

/**
 * @class VolumeSurfaceProcessor
 * @brief ComputeProcessor resampling one scalar field of a VolumeGridBuffer
 *        into the corner grid layout the marching cubes stage consumes.
 *
 * VolumeGridBuffer stores one value per cell centre; mc_emit.comp reads one
 * value per lattice corner, of which there are (res+1) on each axis. This
 * stage bridges the two by trilinear resampling, which also decouples the
 * extraction resolution from the simulation resolution: surfacing a 128
 * cube simulation at 64 costs a quarter of the triangles.
 *
 * Marching cubes crosses at iso_level with the interior conventionally
 * negative, while a density field is high inside. The shader therefore
 * writes threshold minus density, placing the surface where density equals
 * threshold with negative interior, so the consuming SDFMeshProcessor runs
 * at iso_level zero.
 *
 * Owns the corner grid buffer. SDFPrepProcessor is not needed on this path:
 * every corner is written each cycle, and SDFMeshProcessor zeroes the
 * atomic counter itself.
 *
 * Chain order:
 *   flat[n]   - the volume simulation stages
 *   flat[n+1] - VolumeSurfaceProcessor (field -> corner grid)
 *   flat[n+2] - SDFMeshProcessor       (corner grid -> vertices)
 *   final     - RenderProcessor
 *
 * The attached buffer must be the VolumeGridBuffer for the field read to
 * resolve, but the vertex output belongs to whichever buffer
 * SDFMeshProcessor is attached to.
 */
class MAYAFLUX_API VolumeSurfaceProcessor : public ComputeProcessor {
public:
    /**
     * @struct SurfaceParams
     * @brief Push constant block the resample shader receives.
     *
     * Lattice dimensions occupy the leading fields, matching the convention
     * AdvectProcessor::AdvectParams establishes for volume stages,
     * followed by the extraction resolution and the surface threshold.
     */
    struct SurfaceParams {
        uint32_t width;
        uint32_t height;
        uint32_t depth;
        uint32_t pad0;
        uint32_t res_x;
        uint32_t res_y;
        uint32_t res_z;
        float threshold;
    };

    static_assert(sizeof(SurfaceParams) % 16 == 0);

    /**
     * @brief Construct a surface extraction bridge.
     * @param field_name Name of the scalar field resampled. Must have
     *        stride sizeof(float).
     * @param res_x Extraction cell count along X. Minimum 1.
     * @param res_y Extraction cell count along Y. Minimum 1.
     * @param res_z Extraction cell count along Z. Minimum 1.
     * @param threshold Field value the surface is placed at.
     * @param shader_path Path to the compute shader.
     */
    VolumeSurfaceProcessor(
        std::shared_ptr<VolumeGridBuffer> volume,
        std::string field_name,
        uint32_t res_x,
        uint32_t res_y,
        uint32_t res_z,
        float threshold,
        const std::string& shader_path = "volume_to_sdf_grid.comp");

    ~VolumeSurfaceProcessor() override = default;

    /**
     * @brief The corner grid this stage writes.
     *
     * Pass to SDFMeshProcessor's externally-owned-buffers constructor.
     * Valid immediately after construction.
     */
    [[nodiscard]] std::shared_ptr<VKBuffer> grid_buf() const { return m_grid_buf; }

    /**
     * @brief Set the field value the surface is placed at.
     * @param threshold Surface level. Takes effect next cycle.
     */
    void set_threshold(float threshold);

    /** @brief Field value the surface is placed at. */
    [[nodiscard]] float get_threshold() const { return m_threshold; }

    /** @brief Extraction cell count along X. */
    [[nodiscard]] uint32_t get_res_x() const { return m_res_x; }

    /** @brief Extraction cell count along Y. */
    [[nodiscard]] uint32_t get_res_y() const { return m_res_y; }

    /** @brief Extraction cell count along Z. */
    [[nodiscard]] uint32_t get_res_z() const { return m_res_z; }

    /**
     * @brief Corner count of the grid, (res_x+1)*(res_y+1)*(res_z+1).
     *
     * The grid buffer holds this many floats.
     */
    [[nodiscard]] uint32_t corner_count() const noexcept
    {
        return (m_res_x + 1) * (m_res_y + 1) * (m_res_z + 1);
    }

    /**
     * @brief Upper bound on vertices mc_emit can produce at this resolution.
     *
     * Fifteen per voxel, the maximum five triangles the triangle table
     * encodes. Size the vertex buffer to at least this many vertices:
     * mc_emit allocates slots via atomicAdd without a capacity check.
     */
    [[nodiscard]] uint32_t worst_case_vertices() const noexcept
    {
        return m_res_x * m_res_y * m_res_z * 15U;
    }

protected:
    /**
     * @brief Validate the named field, bind the grid, and size the dispatch.
     * @param buffer The attached buffer, expected to be a VolumeGridBuffer.
     */
    void on_attach(const std::shared_ptr<Buffer>& buffer) override;

    /**
     * @brief Write the field descriptor for the current slot assignment.
     */
    void on_descriptors_created() override;

    /**
     * @brief Reject buffers that are not VolumeGridBuffer.
     * @param cmd_id Command buffer this cycle's dispatch will be recorded into.
     * @param buffer The attached buffer, received as VKBuffer.
     * @return True if the attached buffer is a VolumeGridBuffer.
     */
    bool on_before_execute(Portal::Graphics::CommandBufferID cmd_id, const std::shared_ptr<VKBuffer>& buffer) override;

    /**
     * @brief Write the field descriptor for this cycle's slot assignment,
     *        then run the normal shader processing path.
     *
     * The field's read slot changes whenever an upstream stage swaps it, so
     * the binding is rewritten every cycle rather than once.
     */
    void processing_function(const std::shared_ptr<Buffer>& buffer) override;

private:
    /**
     * @brief Issue the direct ShaderFoundry descriptor write for the field
     *        read slot. The grid buffer is a real VKBuffer and binds through
     *        the normal bind_buffer path.
     */
    void write_field_descriptor();

    /**
     * @brief Size the push constant block to at least SurfaceParams and
     *        write the lattice dimensions, extraction resolution, and
     *        threshold.
     */
    void write_params();

    /**
     * @brief Allocate the corner grid buffer for the current resolution.
     */
    void rebuild_grid_buffer();

    std::string m_field_name;
    uint32_t m_res_x;
    uint32_t m_res_y;
    uint32_t m_res_z;
    float m_threshold;

    std::shared_ptr<VKBuffer> m_grid_buf;
    std::shared_ptr<VolumeGridBuffer> m_volume; ///< The attached volume, cached for the descriptor write.
};

} // namespace MayaFlux::Buffers
