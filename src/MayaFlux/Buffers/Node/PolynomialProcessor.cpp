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
{
}

void PolynomialProcessor::process(std::shared_ptr<AudioBuffer> buffer)
{
    if (!m_polynomial || !buffer || buffer->get_data().empty()) {
        return;
    }

    auto& data = buffer->get_data();

    switch (m_process_mode) {
    case ProcessMode::SAMPLE_BY_SAMPLE:
        for (size_t i = 0; i < data.size(); ++i) {
            data[i] = m_polynomial->process_sample(data[i]);
        }
        break;

    case ProcessMode::BATCH:
        m_polynomial->reset();
        for (size_t i = 0; i < data.size(); ++i) {
            data[i] = m_polynomial->process_sample(data[i]);
        }
        break;

    case ProcessMode::WINDOWED:
        for (size_t window_start = 0; window_start < data.size(); window_start += m_window_size) {
            m_polynomial->reset();
            size_t window_end = std::min(window_start + m_window_size, data.size());
            for (size_t i = window_start; i < window_end; ++i) {
                data[i] = m_polynomial->process_sample(data[i]);
            }
        }
        break;
    }
}

void PolynomialProcessor::on_attach(std::shared_ptr<AudioBuffer> buffer)
{
    m_polynomial->reset();
}

} // namespace MayaFlux::Buffers
