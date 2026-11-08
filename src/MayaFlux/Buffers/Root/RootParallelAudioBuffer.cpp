#include "RootParallelAudioBuffer.hpp"

namespace MayaFlux::Buffers {

RootParallelAudioBuffer::RootParallelAudioBuffer(uint32_t channel_id, uint32_t num_samples)
    : RootAudioBuffer(channel_id, num_samples)
{
    m_preferred_processing_token = ProcessingToken::AUDIO_PARALLEL;
    m_token_enforcement_strategy = TokenEnforcementStrategy::IGNORE;

    for (auto& slot : m_slots) {
        slot.assign(get_num_samples(), 0.0);
    }
}

void RootParallelAudioBuffer::resize(uint32_t num_samples)
{
    RootAudioBuffer::resize(num_samples);

    for (auto& slot : m_slots) {
        slot.assign(num_samples, 0.0);
    }
}

void RootParallelAudioBuffer::handle_cycle_complete()
{
    const auto& mix = get_data();
    m_slots[m_write].assign(mix.begin(), mix.end());
    m_slot_generation[m_write] = ++m_produced;

    const uint32_t previous = m_middle.exchange(m_write | k_fresh, std::memory_order_acq_rel);
    m_write = previous & k_index_mask;
}

std::span<const double> RootParallelAudioBuffer::read_block()
{
    if ((m_middle.load(std::memory_order_relaxed) & k_fresh) != 0U) {
        const uint32_t previous = m_middle.exchange(m_read, std::memory_order_acq_rel);
        m_read = previous & k_index_mask;
    }

    return m_slots[m_read];
}

uint64_t RootParallelAudioBuffer::block_generation() const
{
    return m_slot_generation[m_read];
}

}
