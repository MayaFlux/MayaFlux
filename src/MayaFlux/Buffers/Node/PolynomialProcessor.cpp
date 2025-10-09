#include "PolynomialProcessor.hpp"
#include "MayaFlux/Buffers/AudioBuffer.hpp"

namespace MayaFlux::Buffers {

PolynomialProcessor::PolynomialProcessor(
    std::shared_ptr<Nodes::Generator::Polynomial> polynomial,
    ProcessMode mode,
    size_t window_size)
    : m_polynomial(polynomial)
    , m_process_mode(mode)
    , m_window_size(window_size)
    , m_use_internal(false)
{
}

void PolynomialProcessor::process_span(std::span<double> data)
{
    if (!m_polynomial) {
        return;
    }

    for (size_t i = 0; i < data.size(); i++) {
        process_single_sample(data[i]);
    }
}

void PolynomialProcessor::process_single_sample(double& sample)
{
    if (m_use_internal) {
        sample = m_polynomial->process_sample(sample);
        return;
    }

    Nodes::atomic_inc_modulator_count(m_polynomial->m_modulator_count, 1);
    uint32_t state = m_polynomial->m_state.load();

    if (state & Utils::NodeState::PROCESSED) {
        sample = m_polynomial->get_last_output();
    } else {
        sample = m_polynomial->process_sample(sample);
        Nodes::atomic_add_flag(m_polynomial->m_state, Utils::NodeState::PROCESSED);
    }

    Nodes::atomic_dec_modulator_count(m_polynomial->m_modulator_count, 1);
    Nodes::try_reset_processed_state(m_polynomial);
}

void PolynomialProcessor::processing_function(std::shared_ptr<Buffer> buffer)
{
    if (!m_polynomial || !buffer || std::dynamic_pointer_cast<AudioBuffer>(buffer)->get_data().empty()) {
        return;
    }
    if (m_pending_polynomial) {
        m_polynomial = m_pending_polynomial;
        m_pending_polynomial.reset();
        m_use_internal = true;
    }

    auto& data = std::dynamic_pointer_cast<AudioBuffer>(buffer)->get_data();

    switch (m_process_mode) {
    case ProcessMode::SAMPLE_BY_SAMPLE:
        process_span(std::span<double>(data.data(), data.size()));
        break;

    case ProcessMode::BATCH:
        m_polynomial->reset();
        process_span(std::span<double>(data.data(), data.size()));
        break;

    case ProcessMode::WINDOWED:
        for (size_t window_start = 0; window_start < data.size(); window_start += m_window_size) {
            m_polynomial->reset();
            size_t window_end = std::min(window_start + m_window_size, data.size());
            process_span(std::span<double>(data.data() + window_start, window_end - window_start));
        }
        break;
    }
}

void PolynomialProcessor::on_attach(std::shared_ptr<Buffer> buffer)
{
    m_polynomial->reset();
}

} // namespace MayaFlux::Buffers
