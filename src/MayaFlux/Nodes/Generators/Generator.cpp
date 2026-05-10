#include "Generator.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nodes::Generator {

void Generator::enable_mock_process(bool mock_process)
{
    if (mock_process) {
        atomic_add_flag(m_state, NodeState::MOCK_PROCESS);
    } else {
        atomic_remove_flag(m_state, NodeState::MOCK_PROCESS);
    }
}

bool Generator::should_mock_process() const
{
    return m_state.load() & NodeState::MOCK_PROCESS;
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

void operator*(const std::shared_ptr<Node>& node, double value)
{
    if (auto gen = std::dynamic_pointer_cast<Generator>(node)) {
        gen->set_amplitude(value);
    } else {
        MF_ERROR(Journal::Component::API, Journal::Context::NodeProcessing,
            "Cannot multiply non-generator node by a scalar. "
            "Use set_[params] methods or create a BinaryOpNode.");
    }
}

void Generator::notify_tick(double value)
{
    update_context(value);
    auto& ctx = get_last_context();
    for (auto& cb : m_callbacks) {
        cb(ctx);
    }
    for (auto& [cb, cond] : m_conditional_callbacks) {
        if (cond(ctx)) {
            cb(ctx);
        }
    }
}

void Generator::on_tick(const TypedHook<GeneratorContext>& callback)
{
    m_callbacks.emplace_back([callback](NodeContext& ctx) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
        callback(static_cast<GeneratorContext&>(ctx));
    });
}

void Generator::on_tick_if(const NodeCondition& condition, const TypedHook<GeneratorContext>& callback)
{
    m_conditional_callbacks.emplace_back([callback](NodeContext& ctx) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
        callback(static_cast<GeneratorContext&>(ctx));
    },
        condition);
}

} // namespace MayaFlux::Nodes::Generator
