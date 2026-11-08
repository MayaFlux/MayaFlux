#pragma once

#include "RootAudioBuffer.hpp"

namespace MayaFlux::Buffers {

/**
 * @class RootParallelAudioBuffer
 * @brief Root buffer of an AUDIO_PARALLEL unit
 *
 * Mixes its children exactly as RootAudioBuffer does, but prefers the
 * AUDIO_PARALLEL token and does not police the default processor token of its
 * children, so a CPU source and a GPU compute child can share the same root.
 */
class MAYAFLUX_API RootParallelAudioBuffer : public RootAudioBuffer {
public:
    /**
     * @brief Creates a parallel root buffer for a channel
     * @param channel_id Channel identifier
     * @param num_samples Buffer capacity in samples (default: 512)
     */
    RootParallelAudioBuffer(uint32_t channel_id, uint32_t num_samples = 512);
};

}
