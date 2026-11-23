#include "MeshReflow.hpp"

#include "MayaFlux/Buffers/Geometry/MeshBuffer.hpp"
#include "MayaFlux/Nodes/Graphics/MeshWriterNode.hpp"
#include "MayaFlux/Nodes/Network/InstanceNetwork.hpp"
#include "MayaFlux/Nodes/Network/MeshNetwork.hpp"

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

MeshReflow::Sink slot_geometry_sink(const std::shared_ptr<Nodes::Network::MeshNetwork>& network, uint32_t slot)
{
    return [target = std::weak_ptr<Nodes::Network::MeshNetwork>(network), slot](
               const std::shared_ptr<const Kakshya::MeshData>& frame, const FrameContext&) {
        if (!frame) {
            return;
        }
        const auto held = target.lock();
        if (!held || slot >= held->slot_count()) {
            return;
        }
        if (const auto& node = held->get_slot(slot).node) {
            node->set_mesh(*frame);
        }
    };
}

MeshReflow::Sink instance_geometry_sink(const std::shared_ptr<Nodes::Network::InstanceNetwork>& instances)
{
    return [target = std::weak_ptr<Nodes::Network::InstanceNetwork>(instances)](
               const std::shared_ptr<const Kakshya::MeshData>& frame, const FrameContext&) {
        if (!frame) {
            return;
        }
        const auto held = target.lock();
        if (!held || held->slot_count() == 0) {
            return;
        }
        if (const auto node = std::dynamic_pointer_cast<Nodes::GpuSync::MeshWriterNode>(held->get_slot(0).node)) {
            node->set_mesh(*frame);
        }
    };
}

} // namespace MayaFlux::Kriya
