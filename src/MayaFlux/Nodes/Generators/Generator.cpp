#include "Generator.hpp"

namespace MayaFlux::Nodes::Generator {

void Generator::enable_mock_process(bool mock_process)
{
    if (mock_process) {
        atomic_add_flag(m_state, Utils::NodeState::MOCK_PROCESS);
    } else {
        atomic_remove_flag(m_state, Utils::NodeState::MOCK_PROCESS);
    }
}

bool Generator::should_mock_process() const
{
    return m_state.load() & Utils::NodeState::MOCK_PROCESS;
}

std::unique_ptr<NodeContext> Generator::create_context(double value)
{
    if (m_gpu_compatible) {
        return std::make_unique<GeneratorContextGpu>(
            value,
            m_frequency,
            m_amplitude,
            m_phase,
            get_gpu_data_buffer());
    }
    return std::make_unique<GeneratorContext>(value, m_frequency, m_amplitude, m_phase);
}

void Generator::set_amplitude(double amplitude)
{
    m_amplitude = amplitude;
}

double Generator::get_amplitude() const
{
    return m_amplitude;
}

void Generator::set_frequency(float frequency)
{
    m_frequency = frequency;
}

} // namespace MayaFlux::Nodes::Generator
