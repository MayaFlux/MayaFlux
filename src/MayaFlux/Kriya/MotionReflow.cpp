#include "MotionReflow.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Buffers/Network/RecordedGeometryBuffer.hpp"
#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Nodes/Network/InstanceNetwork.hpp"
#include "MayaFlux/Nodes/Network/MeshNetwork.hpp"
#include "MayaFlux/Transitive/Memory/SnapshotCell.hpp"

namespace MayaFlux::Kriya {

FrameOperation<Kakshya::MeshAssemblyLocals> placement_edit(
    const std::shared_ptr<const Kakshya::MeshAssemblyContainer>& assembly, PlacementEdit edit)
{
    return FrameOperation<Kakshya::MeshAssemblyLocals>::transform(
        [assembly, edit = std::move(edit)](const FrameContext& context, std::shared_ptr<const Kakshya::MeshAssemblyLocals> frame)
            -> std::shared_ptr<const Kakshya::MeshAssemblyLocals> {
            if (!frame || !assembly || !edit) {
                return frame;
            }
            std::vector<glm::mat4> locals = frame->matrices;
            edit(std::span<glm::mat4>(locals), context);
            auto edited = Kakshya::resolve_world(*assembly, std::move(locals));
            edited.weights = frame->weights;
            return std::make_shared<const Kakshya::MeshAssemblyLocals>(std::move(edited));
        });
}

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
        if (matrices < slots && !*warned) {
            *warned = true;
            MF_RT_WARN(Journal::Component::Kriya, Journal::Context::CoroutineScheduling,
                "slot_sink: a frame has {} placements and the network {} slots; the ones both have are written",
                matrices, slots);
        }

        const size_t count = std::min(matrices, slots);
        for (size_t i = 0; i < count; ++i) {
            auto& slot = held->get_slot(static_cast<uint32_t>(i));
            slot.local_transform = frame->matrices[i];
            if (i < frame->weights.size() && !frame->weights[i].empty()) {
                slot.morph_weights = frame->weights[i];
            }
            slot.dirty = true;
        }
    };
}

MotionReflow::Sink slot_sink(
    const std::shared_ptr<Nodes::Network::MeshNetwork>& network, std::vector<uint64_t> rows,
    Nodes::Network::MeshPartPlacement placement)
{
    return [target = std::weak_ptr<Nodes::Network::MeshNetwork>(network), rows = std::move(rows), placement,
               warned = std::make_shared<bool>(false)](
               const std::shared_ptr<const Kakshya::MeshAssemblyLocals>& frame, const FrameContext&) {
        if (!frame) {
            return;
        }
        const auto held = target.lock();
        if (!held) {
            return;
        }

        const auto& matrices = placement == Nodes::Network::MeshPartPlacement::Flat ? frame->world : frame->matrices;
        const size_t slots = std::min(rows.size(), held->slot_count());
        for (size_t i = 0; i < slots; ++i) {
            if (rows[i] >= matrices.size()) {
                if (!*warned) {
                    *warned = true;
                    MF_RT_WARN(Journal::Component::Kriya, Journal::Context::CoroutineScheduling,
                        "slot_sink: a frame has {} placements and the part shows row {}; a flat part needs the world placements, which resolve_world fills",
                        matrices.size(), rows[i]);
                }
                continue;
            }
            auto& slot = held->get_slot(static_cast<uint32_t>(i));
            if (rows[i] < frame->weights.size() && !frame->weights[rows[i]].empty()) {
                slot.morph_weights = frame->weights[rows[i]];
            }
            if (slot.local_transform != matrices[rows[i]]) {
                slot.local_transform = matrices[rows[i]];
                slot.dirty = true;
            }
        }
    };
}

MotionReflow::Sink instance_sink(
    const std::shared_ptr<Nodes::Network::InstanceNetwork>& instances, std::vector<uint64_t> rows)
{
    return [target = std::weak_ptr<Nodes::Network::InstanceNetwork>(instances), rows = std::move(rows),
               warned = std::make_shared<bool>(false)](
               const std::shared_ptr<const Kakshya::MeshAssemblyLocals>& frame, const FrameContext&) {
        if (!frame) {
            return;
        }
        const auto held = target.lock();
        if (!held) {
            return;
        }

        const size_t slots = std::min(rows.size(), held->slot_count());
        for (size_t i = 0; i < slots; ++i) {
            if (rows[i] >= frame->world.size()) {
                if (!*warned) {
                    *warned = true;
                    MF_RT_WARN(Journal::Component::Kriya, Journal::Context::CoroutineScheduling,
                        "instance_sink: a frame has {} world placements and the instances show row {}; resolve_world fills them",
                        frame->world.size(), rows[i]);
                }
                continue;
            }
            auto& slot = held->get_slot(static_cast<uint32_t>(i));
            if (rows[i] < frame->weights.size() && !frame->weights[rows[i]].empty()
                && slot.morph_weights != frame->weights[rows[i]]) {
                slot.morph_weights = frame->weights[rows[i]];
                slot.dirty = true;
            }
            if (slot.transform != frame->world[rows[i]]) {
                slot.transform = frame->world[rows[i]];
                slot.dirty = true;
            }
        }
    };
}

MotionReflow::Sink placement_sink(const std::shared_ptr<Buffers::RecordedGeometryBuffer>& buffer, uint64_t row)
{
    struct Placed {
        Memory::SnapshotCell<glm::mat4> cell;
        glm::mat4 last { 1.0F };
        bool published {};
        bool installed {};
        bool warned {};
    };

    return [target = std::weak_ptr<Buffers::RecordedGeometryBuffer>(buffer), row, placed = std::make_shared<Placed>()](
               const std::shared_ptr<const Kakshya::MeshAssemblyLocals>& frame, const FrameContext&) {
        if (!frame) {
            return;
        }
        const auto held = target.lock();
        if (!held) {
            return;
        }

        if (row >= frame->world.size()) {
            if (!placed->warned) {
                placed->warned = true;
                MF_RT_WARN(Journal::Component::Kriya, Journal::Context::CoroutineScheduling,
                    "placement_sink: a frame has {} world placements and the buffer shows row {}; resolve_world fills them",
                    frame->world.size(), row);
            }
            return;
        }

        if (!placed->published || placed->last != frame->world[row]) {
            placed->last = frame->world[row];
            placed->published = true;
            placed->cell.publish(placed->last);
        }

        if (!placed->installed) {
            if (const auto renderer = held->get_render_processor()) {
                renderer->set_geometry_transform_source([placed] {
                    const auto pinned = placed->cell.read();
                    return pinned ? *pinned : glm::mat4(1.0F);
                });
                placed->installed = true;
            }
        }
    };
}

} // namespace MayaFlux::Kriya
