#include "Estimate.hpp"

namespace MayaFlux::Kinesis::Stochastic {

Estimate::Estimate(EstimateModel model, double adapt_rate, size_t window)
    : m_model(model)
    , m_adapt_rate(adapt_rate)
    , m_window(window == 0 ? 1 : window)
    , m_history(m_window)
{
}

void Estimate::set_model(EstimateModel model)
{
    if (m_model != model) {
        m_model = model;
        reset();
    }
}

void Estimate::set_window(size_t window)
{
    m_window = window == 0 ? 1 : window;
    m_history.resize(m_window);
    m_state.sample_count = 0;
}

void Estimate::reset()
{
    m_state.reset();
    m_history.reset();
}

double Estimate::update(double sample)
{
    m_state.last_raw_sample = sample;
    m_state.sample_count++;

    switch (m_model) {
    case EstimateModel::ROLLING_VARIANCE:
        return update_rolling_variance(sample);
    case EstimateModel::EWM_VARIANCE:
        return update_ewm_variance(sample);
    case EstimateModel::MEDIAN_ABSOLUTE_DEVIATION:
        return update_mad(sample);
    case EstimateModel::QUIET_PERIOD_FLOOR:
        return update_quiet_period(sample);
    case EstimateModel::TREND:
        return update_trend(sample);
    default:
        return update_ewm_variance(sample);
    }
}

double Estimate::confidence(double step) const
{
    const double f = m_state.floor;
    if (f < 1e-12)
        return (std::abs(step) > 0.0) ? 1.0 : 0.0;

    const double ratio = std::abs(step) / f;
    return std::clamp(ratio / 3.0, 0.0, 1.0);
}

double Estimate::update_rolling_variance(double sample)
{
    m_history.push(sample);
    const auto view = m_history.linearized_view();

    const double v = variance(view);
    const double mean = std::accumulate(view.begin(), view.end(), 0.0) / static_cast<double>(view.size());

    m_state.running_mean = mean;
    m_state.running_variance = v;
    m_state.floor = std::sqrt(v);
    m_state.filtered_value = mean;

    return m_state.floor;
}

double Estimate::update_ewm_variance(double sample)
{
    if (m_state.sample_count == 1) {
        m_state.running_mean = sample;
        m_state.running_variance = 0.0;
        m_state.floor = 0.0;
        m_state.filtered_value = sample;
        return m_state.floor;
    }

    const double delta = sample - m_state.running_mean;
    m_state.running_mean += m_adapt_rate * delta;

    const double delta2 = sample - m_state.running_mean;
    m_state.running_variance = (1.0 - m_adapt_rate) * (m_state.running_variance + m_adapt_rate * delta * delta2);

    m_state.floor = std::sqrt(m_state.running_variance);
    m_state.filtered_value = m_state.running_mean;

    return m_state.floor;
}

double Estimate::update_mad(double sample)
{
    m_history.push(sample);
    const auto view = m_history.linearized_view();

    std::vector<double> sorted(view.begin(), view.end());
    std::ranges::sort(sorted);
    const double median = sorted[sorted.size() / 2];

    m_state.running_mean = median;
    m_state.floor = median_absolute_deviation(view);
    m_state.filtered_value = median;

    return m_state.floor;
}

double Estimate::update_quiet_period(double sample)
{
    m_history.push(sample);

    if (m_state.sample_count < m_window) {
        m_state.filtered_value = sample;
        return m_state.floor;
    }

    const auto view = m_history.linearized_view();

    std::vector<double> chronological(view.rbegin(), view.rend());
    std::span<const double> ordered(chronological);

    const double mean = std::accumulate(chronological.begin(), chronological.end(), 0.0) / static_cast<double>(chronological.size());
    const double window_stddev = stddev(view);
    const double trend_ratio = trend_explained_ratio(ordered);

    const bool looks_quiet = trend_ratio < 0.5;

    if (looks_quiet) {
        m_state.running_mean = mean;
        m_state.running_variance = window_stddev * window_stddev;
        m_state.floor = window_stddev;
        m_state.filtered_value = mean;
    } else {
        m_state.filtered_value = sample;
    }

    return m_state.floor;
}

double Estimate::update_trend(double sample)
{
    m_history.push(sample);

    if (m_state.sample_count < 2) {
        m_state.trend_slope = 0.0;
        m_state.trend_explained_ratio = 0.0;
        m_state.floor = 0.0;
        m_state.filtered_value = sample;
        return m_state.floor;
    }

    const auto view = m_history.linearized_view();

    std::vector<double> chronological(view.rbegin(), view.rend());
    std::span<const double> ordered(chronological);

    const double slope = trend_slope(ordered);
    m_state.trend_slope = slope;
    m_state.trend_explained_ratio = trend_explained_ratio(ordered);

    const double mean = std::accumulate(chronological.begin(), chronological.end(), 0.0) / static_cast<double>(chronological.size());
    m_state.running_mean = mean;

    const auto n = static_cast<double>(chronological.size());
    double sum_x = 0.0;
    for (size_t i = 0; i < chronological.size(); ++i)
        sum_x += static_cast<double>(i);
    const double mean_x = sum_x / n;
    const double intercept = mean - slope * mean_x;
    const auto newest_x = static_cast<double>(chronological.size() - 1);
    m_state.filtered_value = intercept + slope * newest_x;

    const double first = chronological.front();
    const double last = chronological.back();
    const auto span_n = static_cast<double>(chronological.size() - 1);

    double residual_sq_sum = 0.0;
    for (size_t i = 0; i < chronological.size(); ++i) {
        const double t = static_cast<double>(i) / span_n;
        const double trend_value = first + t * (last - first);
        const double residual = chronological[i] - trend_value;
        residual_sq_sum += residual * residual;
    }

    const double residual_var = (chronological.size() > 2)
        ? residual_sq_sum / static_cast<double>(chronological.size() - 2)
        : 0.0;

    m_state.running_variance = residual_var;
    m_state.floor = std::sqrt(residual_var);

    return m_state.floor;
}

// =============================================================================
// Stateless span characterization
// =============================================================================

double Estimate::variance(std::span<const double> samples) noexcept
{
    if (samples.size() < 2)
        return 0.0;

    const double mean = std::accumulate(samples.begin(), samples.end(), 0.0) / static_cast<double>(samples.size());

    double sq_sum = 0.0;
    for (double v : samples)
        sq_sum += (v - mean) * (v - mean);

    return sq_sum / static_cast<double>(samples.size() - 1);
}

double Estimate::stddev(std::span<const double> samples) noexcept
{
    return std::sqrt(variance(samples));
}

double Estimate::median_absolute_deviation(std::span<const double> samples) noexcept
{
    if (samples.empty())
        return 0.0;

    std::vector<double> sorted(samples.begin(), samples.end());
    std::ranges::sort(sorted);
    const double median = sorted[sorted.size() / 2];

    std::vector<double> deviations;
    deviations.reserve(sorted.size());
    for (double v : sorted)
        deviations.push_back(std::abs(v - median));
    std::ranges::sort(deviations);

    return deviations[deviations.size() / 2] * 1.4826;
}

std::vector<size_t> Estimate::flag_outliers(std::span<const double> samples, double threshold_mad) noexcept
{
    std::vector<size_t> result;
    if (samples.size() < 2)
        return result;

    std::vector<double> sorted(samples.begin(), samples.end());
    std::ranges::sort(sorted);
    const double median = sorted[sorted.size() / 2];
    const double scaled_mad = median_absolute_deviation(samples);

    if (scaled_mad < 1e-12)
        return result;

    for (size_t i = 0; i < samples.size(); ++i) {
        const double dev = std::abs(samples[i] - median) / scaled_mad;
        if (dev > threshold_mad)
            result.push_back(i);
    }

    return result;
}

double Estimate::trend_explained_ratio(std::span<const double> samples) noexcept
{
    if (samples.size() < 2)
        return 0.0;

    const double first = samples.front();
    const double last = samples.back();
    const auto n = static_cast<double>(samples.size() - 1);

    const double total_mean = std::accumulate(samples.begin(), samples.end(), 0.0) / static_cast<double>(samples.size());

    double total_var = 0.0;
    double residual_var = 0.0;

    for (size_t i = 0; i < samples.size(); ++i) {
        const double t = static_cast<double>(i) / n;
        const double trend_value = first + t * (last - first);

        const double total_dev = samples[i] - total_mean;
        total_var += total_dev * total_dev;

        const double residual = samples[i] - trend_value;
        residual_var += residual * residual;
    }

    if (total_var < 1e-12)
        return 0.0;

    return std::clamp(1.0 - (residual_var / total_var), 0.0, 1.0);
}

double Estimate::trend_slope(std::span<const double> samples) noexcept
{
    if (samples.size() < 2)
        return 0.0;

    const auto n = static_cast<double>(samples.size());

    double sum_x = 0.0;
    double sum_y = 0.0;
    double sum_xy = 0.0;
    double sum_xx = 0.0;

    for (size_t i = 0; i < samples.size(); ++i) {
        const auto x = static_cast<double>(i);
        const double y = samples[i];
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_xx += x * x;
    }

    const double denom = (n * sum_xx) - (sum_x * sum_x);
    if (std::abs(denom) < 1e-12)
        return 0.0;

    return ((n * sum_xy) - (sum_x * sum_y)) / denom;
}

} // namespace MayaFlux::Kinesis::Stochastic
