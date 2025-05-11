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
    double sample = input + random_sample(m_current_start, m_current_end);
    notify_tick(sample);
    return sample;
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

std::unique_ptr<NodeContext> NoiseEngine::create_context(double value)
{
    return std::make_unique<StochasticContext>(value, m_type, m_amplitude, m_current_start, m_current_end, m_normal_spread);
}

void NoiseEngine::notify_tick(double value)
{
    auto context = create_context(value);
    for (auto& callback : m_callbacks) {
        callback(*context);
    }
    for (auto& [callback, condition] : m_conditional_callbacks) {
        if (condition(*context)) {
            callback(*context);
        }
    }
}

void NoiseEngine::on_tick(NodeHook callback)
{
    m_callbacks.push_back(callback);
}

void NoiseEngine::on_tick_if(NodeHook callback, NodeCondition condition)
{
    m_conditional_callbacks.emplace_back(callback, condition);
}

bool NoiseEngine::remove_hook(const NodeHook& callback)
{
    auto it = std::find_if(m_callbacks.begin(), m_callbacks.end(),
        [&callback](const NodeHook& hook) {
            return hook.target_type() == callback.target_type();
        });

    if (it != m_callbacks.end()) {
        m_callbacks.erase(it);
        return true;
    }
    return false;
}

bool NoiseEngine::remove_conditional_hook(const NodeCondition& callback)
{
    auto it = std::find_if(m_conditional_callbacks.begin(), m_conditional_callbacks.end(),
        [&callback](const std::pair<NodeHook, NodeCondition>& pair) {
            return pair.first.target_type() == callback.target_type();
        });

    if (it != m_conditional_callbacks.end()) {
        m_conditional_callbacks.erase(it);
        return true;
    }
    return false;
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
