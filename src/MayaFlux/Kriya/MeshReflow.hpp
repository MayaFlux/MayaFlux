#pragma once

#include "MayaFlux/Kakshya/NDData/MeshData.hpp"
#include "MayaFlux/Kriya/Reflow.hpp"

namespace MayaFlux::Buffers {
class MeshBuffer;
}

namespace MayaFlux::Nodes::Network {
class MeshNetwork;
class InstanceNetwork;
}

namespace MayaFlux::Kriya {

/** @brief A reflow of mesh frames, each a whole MeshData. */
using MeshReflow = Reflow<Kakshya::MeshData>;

/** @brief Describes a MeshReflow. */
using MeshReflowBuilder = ReflowBuilder<Kakshya::MeshData>;

/**
 * @brief A sink that shows each frame it is given in a MeshBuffer.
 *
 * The frame replaces the buffer's geometry through MeshBuffer::write_frame, and the
 * buffer's own processors run after it as usual. The buffer uploads the indices only
 * when they differ from the ones it holds, so frames that share their connectivity cost
 * a vertex upload.
 *
 * The buffer is held weakly, so a reflow never keeps it alive, and a buffer that is gone
 * ends the writes quietly. write_frame never blocks, so the sink may run on any thread,
 * including the graphics frame clock where a started reflow runs.
 *
 * @param buffer The buffer to show the frames in.
 */
[[nodiscard]] MAYAFLUX_API MeshReflow::Sink mesh_sink(const std::shared_ptr<Buffers::MeshBuffer>& buffer);

/**
 * @brief A sink that replaces the geometry of one slot of a MeshNetwork with each frame it is given.
 *
 * The slot's node takes the frame whole, whatever its size, with its submeshes and polygon faces,
 * and the network uploads the change on its next cycle. A network uploads the geometry of all its
 * slots when one changes, so keep what animates in a network of its own. A slot with no node, or
 * one out of range, takes nothing. The network is held weakly.
 *
 * @param network The network.
 * @param slot The slot to show the frames in.
 */
[[nodiscard]] MAYAFLUX_API MeshReflow::Sink slot_geometry_sink(
    const std::shared_ptr<Nodes::Network::MeshNetwork>& network, uint32_t slot);

/**
 * @brief A sink that replaces the geometry every instance of an InstanceNetwork shows.
 *
 * The instances draw one template, the first slot's node, so the frame goes there once however
 * many instances there are. The network is held weakly.
 */
[[nodiscard]] MAYAFLUX_API MeshReflow::Sink instance_geometry_sink(
    const std::shared_ptr<Nodes::Network::InstanceNetwork>& instances);

} // namespace MayaFlux::Kriya
