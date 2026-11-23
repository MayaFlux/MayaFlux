#pragma once

#include "MayaFlux/Kakshya/Semantic/MeshAssemblyMotionFrames.hpp"
#include "MayaFlux/Kriya/Reflow.hpp"

namespace MayaFlux::Nodes::Network {
class MeshNetwork;
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

} // namespace MayaFlux::Kriya
