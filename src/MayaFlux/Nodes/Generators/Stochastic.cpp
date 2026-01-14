#include "Stochastic.hpp"

namespace MayaFlux::Nodes::Generator::Stochastics {

Random::Random(Utils::distribution type)
    : m_random_engine(std::random_device {}())
    , m_current_start(-1.0F)
    , m_current_end(1.0F)
    , m_normal_spread(4.0F)
    , m_type(type)
{
}

double Random::process_sample(double input)
{
    if ((!m_state_saved || (m_state_saved && m_fire_events_during_snapshot))
        && !m_networked_node) {
        notify_tick(input);
    }
    return m_last_output;
}

double Random::random_sample(double start, double end)
{
    validate_range(start, end);
    m_current_start = start;
    m_current_end = end;
    return transform_sample(generate_distributed_sample(), start, end) * m_amplitude;
}

std::vector<double> Random::random_array(double start, double end, unsigned int num_samples)
{
    validate_range(start, end);
    m_current_start = start;
    m_current_end = end;

    std::vector<double> samples;
    samples.reserve(num_samples);

    for (unsigned int i = 0; i < num_samples; ++i) {
        samples.push_back(transform_sample(generate_distributed_sample(), start, end) * m_amplitude);
    }

    return samples;
}

std::vector<double> Random::process_batch(unsigned int num_samples)
{
    return random_array(m_current_start, m_current_end, num_samples);
}

double Random::generate_distributed_sample()
{
    switch (m_type) {
    case Utils::distribution::UNIFORM: {
        std::uniform_real_distribution<double> dist(m_current_start, m_current_end);
        return dist(m_random_engine);
    }
    case Utils::distribution::NORMAL: {
        const double range = m_current_end - m_current_start;
        std::normal_distribution<double> dist(0.0, range / m_normal_spread);
        return dist(m_random_engine);
    }
    case Utils::distribution::EXPONENTIAL: {
        std::exponential_distribution<double> dist(1.0);
        return dist(m_random_engine);
    }
    case Utils::distribution::POISSON: {
        std::poisson_distribution<int> dist(m_current_end - m_current_start);
        return static_cast<double>(dist(m_random_engine));
    }
    default:
        throw std::invalid_argument("Invalid distribution type");
    }
}

double Random::transform_sample(double sample, double start, double end) const
{
    if (m_type == Utils::distribution::NORMAL) {
        sample = std::max(start, std::min(end, sample));
    } else if (m_type == Utils::distribution::EXPONENTIAL) {
        sample /= end;
        sample = start + sample * (end - start);
    }
    return sample;
}

void Random::validate_range(double start, double end) const
{
    if (start > end) {
        throw std::invalid_argument("Start must be less than or equal to end");
    }
}

std::unique_ptr<NodeContext> Random::create_context(double value)
{
    if (m_gpu_compatible) {
        return std::make_unique<StochasticContextGpu>(
            value,
            m_type,
            m_amplitude,
            m_current_start,
            m_current_end,
            m_normal_spread,
            get_gpu_data_buffer());
    }
    return std::make_unique<StochasticContext>(value, m_type, m_amplitude, m_current_start, m_current_end, m_normal_spread);
}

void Random::notify_tick(double value)
{
    m_last_context = create_context(value);
    for (auto& callback : m_callbacks) {
        callback(*m_last_context);
    }
    for (auto& [callback, condition] : m_conditional_callbacks) {
        if (condition(*m_last_context)) {
            callback(*m_last_context);
        }
    }
}

void Random::printGraph()
{
    // When opengl, vulkan or sciplot plugged in
}

void Random::printCurrent()
{
    // When opengl, vulkan or sciplot plugged in
}
}
