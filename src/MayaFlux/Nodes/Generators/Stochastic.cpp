#include "Stochastic.hpp"

namespace MayaFlux::Nodes::Generator::Stochastics {

NoiseEngine::NoiseEngine(Utils::distribution type)
    : m_random_engine(std::random_device {}())
    , m_current_start(-1.0f)
    , m_current_end(1.0f)
    , m_amplitude(1.0f)
    , m_normal_spread(4.0f)
    , m_type(type)
{
}

double NoiseEngine::process_sample(double input)
{
    return input + random_sample(m_current_start, m_current_end);
}

double NoiseEngine::random_sample(double start, double end)
{
    validate_range(start, end);
    m_current_start = start;
    m_current_end = end;
    return transform_sample(generate_distributed_sample(), start, end) * m_amplitude;
}

std::vector<double> NoiseEngine::random_array(double start, double end, unsigned int num_samples)
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

std::vector<double> NoiseEngine::processFull(unsigned int num_samples)
{
    return random_array(m_current_start, m_current_end, num_samples);
}

double NoiseEngine::generate_distributed_sample()
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

double NoiseEngine::transform_sample(double sample, double start, double end) const
{
    if (m_type == Utils::distribution::NORMAL) {
        sample = std::max(start, std::min(end, sample));
    } else if (m_type == Utils::distribution::EXPONENTIAL) {
        sample /= end;
        sample = start + sample * (end - start);
    }
    return sample;
}

void NoiseEngine::validate_range(double start, double end) const
{
    if (start > end) {
        throw std::invalid_argument("Start must be less than or equal to end");
    }
}

void NoiseEngine::printGraph()
{
    // When opengl, vulkan or sciplot plugged in
}

void NoiseEngine::printCurrent()
{
    // When opengl, vulkan or sciplot plugged in
}
}
