#include "Stochastic.hpp"

namespace MayaFlux::Nodes::Generator::Stochastics {

Random::Random(Utils::distribution type)
    : m_random_engine(std::random_device {}())
    , m_current_start(-1.0F)
    , m_current_end(1.0F)
    , m_normal_spread(4.0F)
    , m_type(type)
    , m_xorshift_state(std::random_device {}() | (static_cast<uint64_t>(std::random_device {}()) << 32))
    , m_context(0.0, type, 1.0, m_current_start, m_current_end, m_normal_spread)
    , m_context_gpu(0.0, type, 1.0, m_current_start, m_current_end, m_normal_spread, get_gpu_data_buffer())
{
    if (m_xorshift_state == 0)
        m_xorshift_state = 0xDEADBEEFCAFEBABE;
}

double Random::process_sample(double input)
{
    m_last_output = input + random_sample(m_current_start, m_current_end);
    if ((!m_state_saved || (m_state_saved && m_fire_events_during_snapshot))
        && !m_networked_node) {
        notify_tick(m_last_output);
    }
    return m_last_output;
}

double Random::random_sample(double start, double end)
{
    validate_range(start, end);

    if (start != m_current_start || end != m_current_end) {
        m_current_start = start;
        m_current_end = end;
        m_dist_dirty = true;
    }

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
    [[likely]] case Utils::distribution::UNIFORM:
        return m_current_start + fast_uniform() * (m_current_end - m_current_start);

    case Utils::distribution::NORMAL:
        rebuild_distributions_if_needed();
        return m_normal_dist(m_random_engine);

    case Utils::distribution::EXPONENTIAL:
        rebuild_distributions_if_needed();
        return m_exponential_dist(m_random_engine);

    case Utils::distribution::POISSON: {
        std::poisson_distribution<int> dist(
            static_cast<int>(m_current_end - m_current_start));
        return static_cast<double>(dist(m_random_engine));
    }
    default:
        return m_current_start + fast_uniform() * (m_current_end - m_current_start);
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

void Random::rebuild_distributions_if_needed() noexcept
{
    if (!m_dist_dirty)
        return;

    const double range = m_current_end - m_current_start;
    m_normal_dist = std::normal_distribution<double>(0.0, range / m_normal_spread);
    m_exponential_dist = std::exponential_distribution<double>(1.0);

    m_cached_start = m_current_start;
    m_cached_end = m_current_end;
    m_cached_spread = m_normal_spread;
    m_dist_dirty = false;
}

void Random::set_normal_spread(double spread)
{
    if (spread != m_normal_spread) {
        m_normal_spread = spread;
        m_dist_dirty = true;
    }
}

void Random::update_context(double value)
{
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

void Random::printGraph()
{
    // When opengl, vulkan or sciplot plugged in
}

void Random::printCurrent()
{
    // When opengl, vulkan or sciplot plugged in
}
}
