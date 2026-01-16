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

void Generator::update_context(double value)
{
    if (m_gpu_compatible) {
        m_context_gpu.value = value;
        m_context_gpu.frequency = m_frequency;
        m_context_gpu.amplitude = m_amplitude;
        m_context_gpu.phase = m_phase;
    } else {
        m_context.value = value;
        m_context.frequency = m_frequency;
        m_context.amplitude = m_amplitude;
        m_context.phase = m_phase;
    }
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

NodeContext& Generator::get_last_context()
{
    if (is_gpu_compatible()) {
        return m_context_gpu;
    }
    return m_context;
}

} // namespace MayaFlux::Nodes::Generator
