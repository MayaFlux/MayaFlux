#include "SpatialAssembly.hpp"

#include "CameraSequence.hpp"
#include "MeshSequence.hpp"
#include "SpatialSequence.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kakshya/Semantic/MeshAssemblyContainer.hpp"

namespace MayaFlux::IO {

std::shared_ptr<Kakshya::MeshAssemblyContainer> open_spatial_assembly(const std::string& path)
{
    const auto archive = SpatialArchive::open(path);
    if (!archive) {
        return nullptr;
    }

    auto assembly = archive->read([](SpatialReader& reader) { return reader.create_mesh_assembly(); });
    if (!assembly) {
        return nullptr;
    }

    const size_t sources = assembly->get_data().sources.size();
    for (size_t index = 0; index < sources; ++index) {
        const auto& source = assembly->get_data().sources[index];
        if (source.origin.empty() || source.variance == Kakshya::TopologyVariance::Constant) {
            continue;
        }

        bool attached = false;
        if (std::holds_alternative<Kakshya::MeshData>(source.geometry)) {
            const auto frames = MeshFileFrames::create(archive, source.origin);
            attached = frames && assembly->set_mesh_frames(index, frames);
        } else if (std::holds_alternative<Kinesis::Lens>(source.geometry)) {
            const auto frames = CameraFileFrames::create(archive, source.origin);
            attached = frames && assembly->set_lens_frames(index, frames);
        } else if (std::holds_alternative<Kakshya::SpatialData>(source.geometry)) {
            const auto frames = SpatialFileFrames::create(archive, source.origin);
            attached = frames && assembly->set_spatial_frames(index, frames, frames->max_vertex_count());
        }

        if (!attached) {
            MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                "open_spatial_assembly: the frames of '{}' did not open, so it keeps its first shot", source.origin);
        }
    }
    return assembly;
}

} // namespace MayaFlux::IO
