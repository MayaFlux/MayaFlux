#include "Random.hpp"

namespace MayaFlux::Nodes::Generator {

Random::Random(Kinesis::Stochastic::Algorithm type)
    : m_generator(type)
    , m_type(type)
    , m_context(0.0, type, 1.0, m_current_start, m_current_end, m_normal_spread)
    , m_context_gpu(0.0, type, 1.0, m_current_start, m_current_end, m_normal_spread, get_gpu_data_buffer())
{
}

void Random::set_type(Kinesis::Stochastic::Algorithm type)
{
    m_generator.set_algorithm(type);
    m_type = type;
}

double Random::process_sample(double input)
{
    m_last_output = input + m_generator(m_current_start, m_current_end) * m_amplitude;

    if ((!m_state_saved || (m_state_saved && m_fire_events_during_snapshot))
        && !m_networked_node) {
        notify_tick(m_last_output);
    }

    return m_last_output;
}

std::vector<double> Random::process_batch(unsigned int num_samples)
{
    auto samples = m_generator.batch(m_current_start, m_current_end, num_samples);

    for (auto& sample : samples) {
        sample *= m_amplitude;
    }

    return samples;
}

void Random::set_normal_spread(double spread)
{
    m_generator.configure("spread", spread);
}

void Random::set_range(double start, double end)
{
    m_current_start = start;
    m_current_end = end;
}

void Random::update_context(double value)
{
    if (m_type != m_generator.get_algorithm()) {
        m_type = m_generator.get_algorithm();
    }

    if (m_gpu_compatible) {
        m_context_gpu.value = value;
        m_context_gpu.distribution_type = m_type;
        m_context_gpu.amplitude = m_amplitude;
        m_context_gpu.range_start = m_current_start;
        m_context_gpu.range_end = m_current_end;
        m_context_gpu.normal_spread = m_normal_spread;
    } else {
        m_context.value = value;
        m_context.distribution_type = m_type;
        m_context.amplitude = m_amplitude;
        m_context.range_start = m_current_start;
        m_context.range_end = m_current_end;
        m_context.normal_spread = m_normal_spread;
    }
}

void Random::notify_tick(double value)
{
    update_context(value);
    auto& ctx = get_last_context();

    for (auto& callback : m_callbacks) {
        callback(ctx);
    }
    for (auto& [callback, condition] : m_conditional_callbacks) {
        if (condition(ctx)) {
            callback(ctx);
        }
    }
}

NodeContext& Random::get_last_context()
{
    if (m_gpu_compatible) {
        return m_context_gpu;
    }
    return m_context;
}

}
