#pragma once

#include "MayaFlux/Kakshya/NDData/MeshData.hpp"
#include "MayaFlux/Kakshya/NDData/SpatialData.hpp"
#include "MayaFlux/Kakshya/NDData/VolumeData.hpp"
#include "MayaFlux/Kakshya/Semantic/MeshAssemblyMotionFrames.hpp"
#include "MayaFlux/Kriya/FrameOperation.hpp"

namespace MayaFlux::Kriya {

/**
 * @brief A blend of volume frames: every field becomes the weighted mean of the frames that hold it.
 *
 * A field is blended across the frames that have a field of that name, kind and size. The frames
 * must share a lattice: when they do not, or only one frame has weight, the heaviest frame is the
 * result. A frame that is the same shared frame is returned as it is, never copied.
 */
[[nodiscard]] MAYAFLUX_API FrameOperation<Kakshya::VolumeData> volume_blend();

/**
 * @brief A blend of mesh frames that have the same triangles: vertices become the weighted mean.
 *
 * Position, colour, weight, uv, normal and tangent are blended, the normal and the tangent's
 * direction renormalised. Frames whose indices or vertex counts differ cannot be blended, and the
 * heaviest frame is the result, with its submeshes and faces.
 */
[[nodiscard]] MAYAFLUX_API FrameOperation<Kakshya::MeshData> mesh_blend();

/**
 * @brief A blend of point or curve frames: positions, velocities and attributes become the weighted mean.
 *
 * Frames blend when they have the same topology and the same number of points, in the same order or
 * (when every frame carries ids) matched by id. An attribute of float or vector values with the same
 * name, scope and size in every frame is blended; anything else comes from the heaviest frame. Frames
 * that cannot be matched give the heaviest frame.
 */
[[nodiscard]] MAYAFLUX_API FrameOperation<Kakshya::SpatialData> spatial_blend();

/**
 * @brief A blend of the placements of an assembly: per row, translation and scale are averaged and
 *        rotation is averaged along the short way, then the world placements are resolved again.
 * @param assembly The assembly the frames place, for the hierarchy the world is resolved through.
 *        Null leaves the world empty.
 *
 * Frames with another number of rows than the heaviest are left out. This is the crossfade between
 * two motions of one assembly, or the blend of any number of weighted frames.
 */
[[nodiscard]] MAYAFLUX_API FrameOperation<Kakshya::MeshAssemblyLocals> placement_blend(
    const std::shared_ptr<const Kakshya::MeshAssemblyContainer>& assembly);

} // namespace MayaFlux::Kriya
