#pragma once

#include "SpatialArchive.hpp"

namespace MayaFlux::Kakshya {
class MeshAssemblyContainer;
}

namespace MayaFlux::IO {

/**
 * @brief Open an Alembic archive as an assembly whose animated sources can be played.
 *
 * What SpatialReader::create_mesh_assembly builds: the hierarchy as placement rows, every
 * mesh, points and curves object as a source with its first shot. A source whose geometry changes
 * over time (its topology variance is not Constant) also gets the frames of its object, a
 * MeshFileFrames, a SpatialFileFrames or a CameraFileFrames, set with MeshAssemblyContainer::set_mesh_frames,
 * set_spatial_frames or set_lens_frames. A camera is a source its row shows, and its lens is the source. Every one of them reads through the one archive, so the file is opened once
 * however many objects animate, and a points source is told the most vertices its frames hold.
 *
 * @param path Path to the .abc file.
 * @return The assembly, or nullptr, with the reason logged, for a file that cannot be read. A
 * source whose frames cannot be opened is left with its first shot and a warning.
 */
[[nodiscard]] MAYAFLUX_API std::shared_ptr<Kakshya::MeshAssemblyContainer> open_spatial_assembly(const std::string& path);

} // namespace MayaFlux::IO
