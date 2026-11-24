#include "SpatialReflow.hpp"

#include "MayaFlux/Buffers/Network/RecordedGeometryBuffer.hpp"

namespace MayaFlux::Kriya {

SpatialReflow::Sink spatial_sink(const std::shared_ptr<Buffers::RecordedGeometryBuffer>& buffer)
{
    return [target = std::weak_ptr<Buffers::RecordedGeometryBuffer>(buffer)](
               const std::shared_ptr<const Kakshya::SpatialData>& frame, const FrameContext&) {
        if (!frame) {
            return;
        }
        if (const auto held = target.lock()) {
            held->write_frame(frame);
        }
    };
}

} // namespace MayaFlux::Kriya
