#include "RootParallelAudioBuffer.hpp"

namespace MayaFlux::Buffers {

RootParallelAudioBuffer::RootParallelAudioBuffer(uint32_t channel_id, uint32_t num_samples)
    : RootAudioBuffer(channel_id, num_samples)
{
    m_preferred_processing_token = ProcessingToken::AUDIO_PARALLEL;
    m_token_enforcement_strategy = TokenEnforcementStrategy::IGNORE;
}

}
