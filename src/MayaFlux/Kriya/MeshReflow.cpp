#include "MeshReflow.hpp"

#include "MayaFlux/Buffers/Geometry/MeshBuffer.hpp"

namespace MayaFlux::Kriya {

MeshReflow::Sink mesh_sink(const std::shared_ptr<Buffers::MeshBuffer>& buffer)
{
    return [target = std::weak_ptr<Buffers::MeshBuffer>(buffer)](
               const std::shared_ptr<const Kakshya::MeshData>& frame, const FrameContext&) {
        if (!frame) {
            return;
        }
        if (const auto held = target.lock()) {
            held->write_frame(frame);
        }
    };
}

} // namespace MayaFlux::Kriya
