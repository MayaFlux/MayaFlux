#pragma once

#include "MayaFlux/Kakshya/NDData/SpatialData.hpp"
#include "MayaFlux/Kriya/Reflow.hpp"

namespace MayaFlux::Buffers {
class RecordedGeometryBuffer;
}

namespace MayaFlux::Kriya {

/** @brief A reflow of spatial frames, such as the samples of an Alembic stream (IO::SpatialFileFrames). */
using SpatialReflow = Reflow<Kakshya::SpatialData>;

/** @brief Describes a SpatialReflow. */
using SpatialReflowBuilder = ReflowBuilder<Kakshya::SpatialData>;

/**
 * @brief A sink that shows each frame it is given in a RecordedGeometryBuffer.
 *
 * The frame replaces the buffer's geometry through RecordedGeometryBuffer::write_frame, and
 * the buffer's own processors run after it as usual, so the spatial stages see the frame.
 * A frame the buffer cannot show, because of its topology or its size, is dealt with by the
 * buffer.
 *
 * The buffer is held weakly, so a reflow never keeps it alive, and a buffer that is gone
 * ends the writes quietly. write_frame never blocks, so the sink may run on any thread,
 * including the graphics frame clock where a started reflow runs.
 *
 * @param buffer The buffer to show the frames in.
 */
[[nodiscard]] MAYAFLUX_API SpatialReflow::Sink spatial_sink(const std::shared_ptr<Buffers::RecordedGeometryBuffer>& buffer);

} // namespace MayaFlux::Kriya
