#include "FilterProcessor.hpp"
#include "MayaFlux/Buffers/AudioBuffer.hpp"

#include "MayaFlux/Nodes/Filters/Filter.hpp"

namespace MayaFlux::Buffers {

void FilterProcessor::process_single_sample(double& sample)
{
    if (m_use_internal) {
        sample = m_filter->process_sample(sample);
        return;
    }

    Nodes::atomic_inc_modulator_count(m_filter->m_modulator_count, 1);
    uint32_t state = m_filter->m_state.load();

    if (state & Utils::NodeState::PROCESSED) {
        sample = m_filter->get_last_output();
    } else {
        sample = m_filter->process_sample(sample);
        Nodes::atomic_add_flag(m_filter->m_state, Utils::NodeState::PROCESSED);
    }

    Nodes::atomic_dec_modulator_count(m_filter->m_modulator_count, 1);
    Nodes::try_reset_processed_state(m_filter);
}

void FilterProcessor::processing_function(std::shared_ptr<Buffer> buffer)
{
    if (!m_filter || !buffer)
        return;

    auto audio_buffer = std::dynamic_pointer_cast<AudioBuffer>(buffer);
    if (!audio_buffer || audio_buffer->get_data().empty())
        return;

    if (m_pending_filter) {
        m_filter = m_pending_filter;
        m_pending_filter.reset();
        m_use_internal = true;
    }

    auto& data = audio_buffer->get_data();

    const auto& state = m_filter->m_state.load();

    if (state == Utils::NodeState::INACTIVE) {
        for (size_t i = 0; i < data.size(); ++i) {
            m_filter->set_input_context(std::span<double>(data.data(), i));
            data[i] = m_filter->process_sample(data[i]);
        }
    } else {
        m_filter->save_state();
        for (size_t i = 0; i < data.size(); ++i) {
            m_filter->set_input_context(std::span<double>(data.data(), i));
            data[i] = m_filter->process_sample(data[i]);
        }
        m_filter->restore_state();
    }

    m_filter->clear_input_context();
}

void FilterProcessor::on_attach(const std::shared_ptr<Buffer>& /*buffer*/)
{
    if (m_filter)
        m_filter->reset();
}

} // namespace MayaFlux::Buffers
