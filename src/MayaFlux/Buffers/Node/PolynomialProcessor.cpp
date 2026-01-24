#include "PolynomialProcessor.hpp"
#include "MayaFlux/Buffers/AudioBuffer.hpp"

namespace MayaFlux::Buffers {

PolynomialProcessor::PolynomialProcessor(
    const std::shared_ptr<Nodes::Generator::Polynomial>& polynomial,
    ProcessMode mode,
    size_t window_size)
    : m_polynomial(polynomial)
    , m_process_mode(mode)
    , m_window_size(window_size)
{
}

void PolynomialProcessor::process_span(std::span<double> data)
{
    if (!m_polynomial) {
        return;
    }

    const auto& state = m_polynomial->m_state.load();

    if (state == Utils::NodeState::INACTIVE) {
        for (double& i : data) {
            i = m_polynomial->process_sample(i);
        }
    } else {
        m_polynomial->save_state();
        for (double& i : data) {
            i = m_polynomial->process_sample(i);
        }
        m_polynomial->restore_state();
    }
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

    case ProcessMode::WINDOWED: {
        std::span<double> data_span { data };
        for (size_t window_start = 0; window_start < data_span.size(); window_start += m_window_size) {
            m_polynomial->reset();
            size_t window_size = std::min(m_window_size, data_span.size() - window_start);
            process_span(data_span.subspan(window_start, window_size));
        }
        break;
    }
    case ProcessMode::BUFFER_CONTEXT: {
        m_polynomial->set_buffer_context(std::span<double>(data.data(), data.size()));
        process_span(std::span<double>(data.data(), data.size()));
        m_polynomial->clear_buffer_context();
        break;
    }
    }
}

void PolynomialProcessor::on_attach(const std::shared_ptr<Buffer>& /*buffer*/)
{
    m_polynomial->reset();
}

} // namespace MayaFlux::Buffers
