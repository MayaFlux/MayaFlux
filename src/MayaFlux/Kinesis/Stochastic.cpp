#include "Stochastic.hpp"

namespace MayaFlux::Kinesis::Stochastic {

Stochastic::Stochastic(Algorithm algo)
    : m_engine(std::random_device {}())
    , m_algorithm(algo)
    , m_xorshift_state(std::random_device {}() | (static_cast<uint64_t>(std::random_device {}()) << 32))
{
    if (m_xorshift_state == 0)
        m_xorshift_state = 0xDEADBEEFCAFEBABE;
}

void Stochastic::seed(uint64_t seed)
{
    m_engine.seed(static_cast<std::mt19937::result_type>(seed));
    m_xorshift_state = seed;
    if (m_xorshift_state == 0)
        m_xorshift_state = 0xDEADBEEFCAFEBABE;
    reset_state();
}

void Stochastic::set_algorithm(Algorithm algo)
{
    if (m_algorithm != algo) {
        m_algorithm = algo;
        reset_state();
        m_dist_dirty = true;
    }
}

void Stochastic::configure(const std::string& key, std::any value)
{
    m_config[key] = std::move(value);
    m_dist_dirty = true;
}

std::optional<std::any> Stochastic::get_config(const std::string& key) const
{
    auto it = m_config.find(key);
    if (it != m_config.end()) {
        return it->second;
    }
    return std::nullopt;
}

double Stochastic::operator()(double min, double max)
{
    validate_range(min, max);

    switch (m_algorithm) {
    case Algorithm::UNIFORM:
    case Algorithm::NORMAL:
    case Algorithm::EXPONENTIAL:
    case Algorithm::POISSON:
        return generate_memoryless(min, max);

    case Algorithm::PERLIN:
        return generate_perlin_impl(m_state.phase, 0.0, 0.0);

    case Algorithm::GENDY:
        return generate_gendy_impl(min, max);

    case Algorithm::BROWNIAN:
        return generate_brownian_impl(min, max);

    case Algorithm::PINK:
    case Algorithm::BLUE:
        return generate_colored_noise_impl(min, max);

    default:
        return generate_memoryless(min, max);
    }
}

double Stochastic::at(double x, double y, double z)
{
    if (m_algorithm != Algorithm::PERLIN) {
        throw std::runtime_error("Multi-dimensional access only supported for PERLIN algorithm");
    }
    return generate_perlin_impl(x, y, z);
}

std::vector<double> Stochastic::batch(double min, double max, size_t count)
{
    validate_range(min, max);

    std::vector<double> result;
    result.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        result.push_back((*this)(min, max));
    }

    return result;
}

void Stochastic::reset_state()
{
    m_state.reset();
}

double Stochastic::generate_memoryless(double min, double max)
{
    if (min != m_cached_min || max != m_cached_max) {
        m_cached_min = min;
        m_cached_max = max;
        m_dist_dirty = true;
    }

    double raw_value = 0.0;

    switch (m_algorithm) {
    [[likely]] case Algorithm::UNIFORM:
        return min + fast_uniform() * (max - min);

    case Algorithm::NORMAL: {
        rebuild_distributions_if_needed(min, max);
        raw_value = m_normal_dist(m_engine);
        return std::clamp(raw_value, min, max);
    }

    case Algorithm::EXPONENTIAL: {
        rebuild_distributions_if_needed(min, max);
        raw_value = m_exponential_dist(m_engine);
        raw_value /= max;
        return min + raw_value * (max - min);
    }

    case Algorithm::POISSON: {
        std::poisson_distribution<int> dist(static_cast<int>(max - min));
        return static_cast<double>(dist(m_engine));
    }

    default:
        return min + fast_uniform() * (max - min);
    }
}

double Stochastic::generate_stateful(double min, double max)
{
    m_state.step_count++;
    return m_state.current_value;
}

double Stochastic::generate_perlin_impl(double x, double y, double z)
{
    return 0.0;
}

double Stochastic::generate_gendy_impl(double min, double max)
{
    return min + fast_uniform() * (max - min);
}

double Stochastic::generate_brownian_impl(double min, double max)
{
    double step_size = 0.01;
    if (auto cfg = get_config("step_size"); cfg.has_value()) {
        step_size = std::any_cast<double>(*cfg);
    }

    double step = (fast_uniform() - 0.5) * 2.0 * step_size;
    m_state.current_value += step;
    m_state.current_value = std::clamp(m_state.current_value, min, max);

    m_state.step_count++;
    return m_state.current_value;
}

double Stochastic::generate_colored_noise_impl(double min, double max)
{
    return min + fast_uniform() * (max - min);
}

void Stochastic::validate_range(double min, double max) const
{
    if (min > max) {
        throw std::invalid_argument("Stochastic: min must be <= max");
    }
}

void Stochastic::rebuild_distributions_if_needed(double min, double max)
{
    if (!m_dist_dirty)
        return;

    const double range = max - min;

    if (m_algorithm == Algorithm::NORMAL) {
        double spread = 4.0;
        if (auto cfg = get_config("spread"); cfg.has_value()) {
            spread = std::any_cast<double>(*cfg);
        }
        m_normal_dist = std::normal_distribution<double>(0.0, range / spread);
    } else if (m_algorithm == Algorithm::EXPONENTIAL) {
        m_exponential_dist = std::exponential_distribution<double>(1.0);
    }

    m_dist_dirty = false;
}

}
