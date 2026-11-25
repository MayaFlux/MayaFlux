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

FrameOperation<Kakshya::MeshAssemblyLocals> placement_time_offset(
    const std::shared_ptr<const Kakshya::MeshAssemblyContainer>& assembly,
    const std::shared_ptr<const Kakshya::SampledFrames<Kakshya::MeshAssemblyLocals>>& sampled,
    Kinesis::SpatialField offset)
{
    return FrameOperation<Kakshya::MeshAssemblyLocals>::transform(
        [assembly, sampled, offset = std::move(offset)](const FrameContext& context, std::shared_ptr<const Kakshya::MeshAssemblyLocals> frame)
            -> std::shared_ptr<const Kakshya::MeshAssemblyLocals> {
            if (!frame || !assembly || !sampled || !offset.fn || !context.course) {
                return frame;
            }

            std::vector<glm::mat4> locals = frame->matrices;
            auto weights = frame->weights;
            std::map<double, std::shared_ptr<const Kakshya::MeshAssemblyLocals>> read;
            bool moved = false;

            const size_t rows = std::min(locals.size(), frame->world.size());
            for (size_t row = 0; row < rows; ++row) {
                const float seconds = offset(glm::vec3(frame->world[row][3]));
                if (seconds == 0.0F) {
                    continue;
                }
                const auto time = context.course->time_at(seconds);
                if (!time) {
                    continue;
                }

                auto [held, missing] = read.try_emplace(*time);
                if (missing) {
                    held->second = sampled->sample(*time);
                }
                const auto& other = held->second;
                if (!other || row >= other->matrices.size()) {
                    continue;
                }

                locals[row] = other->matrices[row];
                if (row < other->weights.size() && row < weights.size()) {
                    weights[row] = other->weights[row];
                }
                moved = true;
            }

            if (!moved) {
                return frame;
            }
            auto offsetted = Kakshya::resolve_world(*assembly, std::move(locals));
            offsetted.weights = std::move(weights);
            return std::make_shared<const Kakshya::MeshAssemblyLocals>(std::move(offsetted));
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

MotionReflow::Sink viewpoint_sink(
    const std::shared_ptr<Memory::OwnedCell<Kinesis::Viewpoint>>& view, uint32_t driver, uint64_t row,
    const std::shared_ptr<const Kakshya::MeshAssemblyContainer>& assembly)
{
    std::optional<Kinesis::Frustum> still;
    if (assembly) {
        const auto placement = assembly->placement(row);
        if (placement && placement->source && !assembly->lens_frames(*placement->source)) {
            if (const auto* lens = assembly->lens(*placement->source)) {
                still = Kakshya::frustum_of(*lens);
            }
        }
    }

    return [target = std::weak_ptr<Memory::OwnedCell<Kinesis::Viewpoint>>(view), driver, row, still](
               const std::shared_ptr<const Kakshya::MeshAssemblyLocals>& frame, const FrameContext&) {
        const auto cell = target.lock();
        if (!frame || !cell || row >= frame->world.size() || !cell->drives(driver)) {
            return;
        }
        cell->write(driver, { frame->world[row], still ? *still : cell->read().second });
    };
}

Reflow<Kakshya::MeshAssemblyLens>::Sink frustum_sink(
    const std::shared_ptr<Memory::OwnedCell<Kinesis::Viewpoint>>& view, uint32_t driver)
{
    return [target = std::weak_ptr<Memory::OwnedCell<Kinesis::Viewpoint>>(view), driver](
               const std::shared_ptr<const Kakshya::MeshAssemblyLens>& frame, const FrameContext&) {
        const auto cell = target.lock();
        if (frame && cell && cell->drives(driver)) {
            cell->write(driver, { cell->read().first, Kakshya::frustum_of(*frame) });
        }
    };
}

} // namespace MayaFlux::Kriya
