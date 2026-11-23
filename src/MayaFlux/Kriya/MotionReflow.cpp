#include "MotionReflow.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Nodes/Network/MeshNetwork.hpp"

namespace MayaFlux::Kriya {

MotionReflow::Sink slot_sink(const std::shared_ptr<Nodes::Network::MeshNetwork>& network)
{
    return [target = std::weak_ptr<Nodes::Network::MeshNetwork>(network), warned = std::make_shared<bool>(false)](
               const std::shared_ptr<const Kakshya::MeshAssemblyLocals>& frame, const FrameContext&) {
        if (!frame) {
            return;
        }
        const auto held = target.lock();
        if (!held) {
            return;
        }

        const size_t slots = held->slot_count();
        const size_t matrices = frame->matrices.size();
        if (matrices != slots && !*warned) {
            *warned = true;
            MF_RT_WARN(Journal::Component::Kriya, Journal::Context::CoroutineScheduling,
                "slot_sink: a frame has {} placements and the network {} slots; the ones both have are written",
                matrices, slots);
        }

        const size_t count = std::min(matrices, slots);
        for (size_t i = 0; i < count; ++i) {
            auto& slot = held->get_slot(static_cast<uint32_t>(i));
            slot.local_transform = frame->matrices[i];
            slot.dirty = true;
        }
    };
}

} // namespace MayaFlux::Kriya
