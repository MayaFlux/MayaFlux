#pragma once

#include "MayaFlux/Kakshya/NDData/VolumeData.hpp"
#include "MayaFlux/Kriya/Reflow.hpp"

namespace MayaFlux::Buffers {
class VolumeGridBuffer;
}

namespace MayaFlux::Kriya {

/** @brief A reflow of volume frames, such as the files of a VolumeFileSequence. */
using VolumeReflow = Reflow<Kakshya::VolumeData>;

/** @brief Describes a VolumeReflow. */
using VolumeReflowBuilder = ReflowBuilder<Kakshya::VolumeData>;

/**
 * @brief A sink that replaces the fields of a VolumeGridBuffer with each frame it is given.
 *
 * Every field of the frame the buffer declares is written into the field's current read
 * slot through VolumeGridBuffer::seed_raw, a vector field padded from vec3 to the vec4 the
 * buffer stores. Whatever processors the buffer has run after the write as usual, so a
 * surface, a raymarch or a simulation chain all see the frame.
 *
 * What the sink needs of the frame is checked, and each failure is logged once until it
 * clears: the frame's lattice must equal the buffer's, and a field the buffer does not
 * declare, or declares with the other kind of stride, is left out. A frame that leaves
 * nothing to write writes nothing. The scratch for padding is kept between frames.
 *
 * The buffer is held weakly, so a reflow never keeps it alive, and a buffer that is gone
 * ends the writes quietly.
 *
 * seed_raw records a transfer that the next one resolves, so call from the thread that
 * runs the graphics frame clock, which is where a started reflow runs.
 *
 * @param volume The buffer to write. Made over the lattice of the frames, with the fields
 * IO::volume_field_decls gives for the first of them.
 */
[[nodiscard]] MAYAFLUX_API VolumeReflow::Sink volume_sink(const std::shared_ptr<Buffers::VolumeGridBuffer>& volume);

} // namespace MayaFlux::Kriya
