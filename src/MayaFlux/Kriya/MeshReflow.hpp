#pragma once

#include "MayaFlux/Kakshya/NDData/MeshData.hpp"
#include "MayaFlux/Kriya/Reflow.hpp"

namespace MayaFlux::Buffers {
class MeshBuffer;
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

} // namespace MayaFlux::Kriya
