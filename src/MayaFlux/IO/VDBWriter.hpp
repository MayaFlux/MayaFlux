#pragma once

#include "MayaFlux/IO/VolumeWriter.hpp"

namespace MayaFlux::IO {

/**
 * @class VDBWriter
 * @brief VolumeWriter implementation backed by tinyvdb.
 *
 * Writes one .vdb per call, carrying every field of the supplied
 * VolumeData as a separately named grid in that single file. Scalar
 * fields become float grids, vector fields become vec3s grids. Values
 * below the activity threshold are left inactive and take the background
 * value, which is what makes the output sparse and therefore worth being
 * a .vdb rather than a raw dump.
 *
 * The lattice becomes a linear transform: scale by cell_size(), then
 * translate by bounds.min plus half a cell, so index (i,j,k) maps to the
 * world position Lattice3D::cell_center reports for it.
 *
 * LatticeSemantics maps directly. LatticeValueClass corresponds to
 * OpenVDB's GridClass and VectorVariance to its VecType, both one to one,
 * so a field declared as a fog volume or a contravariant velocity arrives
 * in Houdini, Blender or Maya resampling correctly rather than by
 * coincidence.
 *
 * No time dimension: an animated sequence is a sequence of calls with
 * numbered paths, which is how every consumer of .vdb expects to find one.
 *
 * Options honored:
 *   - VolumeWriteOptions::activity_threshold : global inactive cutoff
 *   - VolumeWriteOptions::field_thresholds   : per-field override
 *   - VolumeWriteOptions::background         : value of inactive cells
 *   - VolumeWriteOptions::half_float         : store values at half precision
 *   - VolumeWriteOptions::compression        : -1 selects the default
 */
class MAYAFLUX_API VDBWriter : public VolumeWriter {
public:
    VDBWriter() = default;
    ~VDBWriter() override = default;

    [[nodiscard]] bool can_write(const std::string& filepath) const override;

    bool write(
        const std::string& filepath,
        const Kakshya::VolumeData& data,
        const VolumeWriteOptions& options = {}) override;

    [[nodiscard]] std::vector<std::string> get_supported_extensions() const override;
    [[nodiscard]] std::string get_last_error() const override { return m_last_error; }

    /**
     * @brief Register this writer with the VolumeWriterRegistry.
     *
     * Called from engine/subsystem init alongside the image writers.
     * Idempotent.
     */
    static void register_with_registry();

private:
    mutable std::string m_last_error;
};

} // namespace MayaFlux::IO
