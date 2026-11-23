#pragma once

#include "MayaFlux/Kakshya/Semantic/MeshAssemblyMotionFrames.hpp"
#include "MayaFlux/Kriya/Reflow.hpp"

namespace MayaFlux::Nodes::Network {
class MeshNetwork;
class InstanceNetwork;
enum class MeshPartPlacement : uint8_t;
}

namespace MayaFlux::Buffers {
class RecordedGeometryBuffer;
}

namespace MayaFlux::Kriya {

/** @brief A reflow of the local placements of an assembly's rows, such as a MeshAssemblyMotionFrames plays. */
using MotionReflow = Reflow<Kakshya::MeshAssemblyLocals>;

/** @brief Describes a MotionReflow. */
using MotionReflowBuilder = ReflowBuilder<Kakshya::MeshAssemblyLocals>;

/**
 * @brief A sink that writes each frame's placements into the slots of a MeshNetwork.
 *
 * Matrix i of a frame becomes the local transform of slot i and the slot is marked dirty, so
 * the network propagates the hierarchy and uploads the change on its next cycle. The network
 * must be one whose slot i is row i of the assembly the frames come from, as
 * Nodes::Network::make_mesh_network builds it. A frame with more matrices than the network
 * has slots, or fewer, writes the slots both have, and says so once.
 *
 * This sets what a slot's transform is, so a MeshTransformOperator that binds the same slot
 * writes it again on its own cycle. The network is held weakly, so a reflow never keeps it
 * alive, and a network that is gone ends the writes quietly. The sink runs where the reflow
 * does, on the graphics frame clock when started.
 *
 * @param network The network to move.
 */
[[nodiscard]] MAYAFLUX_API MotionReflow::Sink slot_sink(const std::shared_ptr<Nodes::Network::MeshNetwork>& network);

/**
 * @brief A sink that writes the placements of some rows into the slots of a network that shows them.
 * @param network The network, such as the one of a Nodes::Network::MeshNetworkPart.
 * @param rows rows[i] is the row slot i shows, as MeshNetworkPart::rows gives it.
 * @param placement Flat reads each row's world placement (MeshAssemblyLocals::world) and writes it
 *        as the slot's transform. Chained reads the local placement (matrices) and leaves the
 *        network to propagate the hierarchy.
 *
 * A slot is marked dirty only when its transform changes. A frame that does not have the
 * placements the part needs, or too few of them, writes the rows it has and says so once. The
 * network is held weakly, as for slot_sink, and a flat part's slots are written again by any
 * operator that binds them, so an operator on a flat part moves that slot alone.
 */
[[nodiscard]] MAYAFLUX_API MotionReflow::Sink slot_sink(
    const std::shared_ptr<Nodes::Network::MeshNetwork>& network, std::vector<uint64_t> rows,
    Nodes::Network::MeshPartPlacement placement);

/**
 * @brief A sink that places the instances of an InstanceNetwork at the world placements of their rows.
 * @param instances The network, such as one made by Nodes::Network::make_instance_network.
 * @param rows rows[i] is the row instance slot i shows, in the order of the group the network was made from.
 *
 * Marks an instance dirty only when its transform changes. The network is held weakly.
 */
[[nodiscard]] MAYAFLUX_API MotionReflow::Sink instance_sink(
    const std::shared_ptr<Nodes::Network::InstanceNetwork>& instances, std::vector<uint64_t> rows);

/**
 * @brief A sink that places a buffer that shows points or curves at the world placement of a row.
 * @param buffer The buffer. Its render processor is given a geometry transform source the first
 *        time a frame arrives after rendering is set up, which replaces any placement it had.
 * @param row The row the buffer shows.
 *
 * The placement is held in a cell the draw reads, so the sink may run on any thread. The buffer is
 * held weakly.
 */
[[nodiscard]] MAYAFLUX_API MotionReflow::Sink placement_sink(
    const std::shared_ptr<Buffers::RecordedGeometryBuffer>& buffer, uint64_t row);

} // namespace MayaFlux::Kriya
