#include "AnalysisHelper.hpp"

#include <numeric>

#include "MayaFlux/Parallel.hpp"

#include "unsupported/Eigen/FFT"

namespace MayaFlux::Yantra {

std::vector<double> compute_dynamic_range_energy(std::span<const double> data, const size_t num_windows, const uint32_t hop_size, const uint32_t window_size)
{
    std::vector<double> dr_values(num_windows);

    std::vector<size_t> indices(num_windows);
    std::iota(indices.begin(), indices.end(), 0);

    MayaFlux::Parallel::for_each(MayaFlux::Parallel::par_unseq, indices.begin(), indices.end(),
        [&](size_t i) {
            const size_t start_idx = i * hop_size;
            const size_t end_idx = std::min(start_idx + window_size, data.size());
            auto window = data.subspan(start_idx, end_idx - start_idx);

            double min_val = std::numeric_limits<double>::max();
            double max_val = std::numeric_limits<double>::lowest();

            for (double sample : window) {
                double abs_sample = std::abs(sample);
                min_val = std::min(min_val, abs_sample);
                max_val = std::max(max_val, abs_sample);
            }

            if (min_val < 1e-10)
                min_val = 1e-10;
            dr_values[i] = 20.0 * std::log10(max_val / min_val);
        });

    return dr_values;
}

std::vector<double> compute_zero_crossing_energy(std::span<const double> data, const size_t num_windows, const uint32_t hop_size, const uint32_t window_size)
{
    std::vector<double> zcr_values(num_windows);

    std::vector<size_t> indices(num_windows);
    std::iota(indices.begin(), indices.end(), 0);

    std::for_each(std::execution::par_unseq, indices.begin(), indices.end(),
        [&](size_t i) {
            const size_t start_idx = i * hop_size;
            const size_t end_idx = std::min(start_idx + window_size, data.size());
            auto window = data.subspan(start_idx, end_idx - start_idx);

            int zero_crossings = 0;
            for (size_t j = 1; j < window.size(); ++j) {
                if ((window[j] >= 0.0) != (window[j - 1] >= 0.0)) {
                    zero_crossings++;
                }
            }
            zcr_values[i] = static_cast<double>(zero_crossings) / static_cast<double>(window.size() - 1);
        });

    return zcr_values;
}

std::vector<double> compute_power_energy(std::span<const double> data, const size_t num_windows, const uint32_t hop_size, const uint32_t window_size)
{
    std::vector<double> power_values(num_windows);

    std::vector<size_t> indices(num_windows);
    std::iota(indices.begin(), indices.end(), 0);

    std::for_each(std::execution::par_unseq, indices.begin(), indices.end(),
        [&](size_t i) {
            const size_t start_idx = i * hop_size;
            const size_t end_idx = std::min(start_idx + window_size, data.size());
            auto window = data.subspan(start_idx, end_idx - start_idx);

            double sum_squares = 0.0;
            for (double sample : window) {
                sum_squares += sample * sample;
            }
            power_values[i] = sum_squares;
        });

    return power_values;
}

std::vector<double> compute_peak_energy(std::span<const double> data, const uint32_t num_windows, const uint32_t hop_size, const uint32_t window_size)
{
    std::vector<double> peak_values(num_windows);

    std::vector<size_t> indices(num_windows);
    std::iota(indices.begin(), indices.end(), 0);

    std::for_each(std::execution::par_unseq, indices.begin(), indices.end(),
        [&](size_t i) {
            const size_t start_idx = i * hop_size;
            const size_t end_idx = std::min(start_idx + window_size, data.size());
            auto window = data.subspan(start_idx, end_idx - start_idx);

            double max_val = 0.0;
            for (double sample : window) {
                max_val = std::max(max_val, std::abs(sample));
            }
            peak_values[i] = max_val;
        });

    return peak_values;
}

std::vector<double> compute_rms_energy(std::span<const double> data, const uint32_t num_windows, const uint32_t hop_size, const uint32_t window_size)
{
    std::vector<double> rms_values(num_windows);

    std::vector<size_t> indices(num_windows);
    std::iota(indices.begin(), indices.end(), 0);

    std::for_each(std::execution::par_unseq, indices.begin(), indices.end(),
        [&](size_t i) {
            const size_t start_idx = i * hop_size;
            const size_t end_idx = std::min(start_idx + window_size, data.size());
            auto window = data.subspan(start_idx, end_idx - start_idx);

            double sum_squares = 0.0;
            for (double sample : window) {
                sum_squares += sample * sample;
            }
            rms_values[i] = std::sqrt(sum_squares / static_cast<double>(window.size()));
        });

    return rms_values;
}

std::vector<double> compute_spectral_energy(std::span<const double> data, const size_t num_windows, const uint32_t hop_size, const uint32_t window_size)
{
    std::vector<double> spectral_energy(num_windows);

    std::vector<size_t> indices(num_windows);
    std::iota(indices.begin(), indices.end(), 0);

    Eigen::VectorXd hanning_window(window_size);
    for (uint32_t i = 0; i < window_size; ++i) {
        hanning_window(i) = 0.5 * (1.0 - std::cos(2.0 * M_PI * i / (window_size - 1)));
    }

    std::for_each(std::execution::par_unseq, indices.begin(), indices.end(),
        [&](size_t i) {
            const size_t start_idx = i * hop_size;
            const size_t end_idx = std::min(start_idx + window_size, data.size());
            auto window = data.subspan(start_idx, end_idx - start_idx);

            Eigen::VectorXd windowed_data = Eigen::VectorXd::Zero(window_size);
            const size_t actual_size = window.size();

            for (int j = 0; j < actual_size; ++j) {
                windowed_data(j) = window[j] * hanning_window(j);
            }

            Eigen::FFT<double> fft;
            Eigen::VectorXcd fft_result;
            fft.fwd(fft_result, windowed_data);

            double energy = 0.0;
            for (int j = 0; j < fft_result.size(); ++j) {
                energy += std::norm(fft_result(j));
            }

            spectral_energy[i] = energy / static_cast<double>(window_size);
        });

    return spectral_energy;
}

std::vector<double> compute_harmonic_energy(std::span<const double> data, const size_t num_windows, const uint32_t hop_size, const uint32_t window_size)
{
    std::vector<double> harmonic_energy(num_windows);

    std::vector<size_t> indices(num_windows);
    std::iota(indices.begin(), indices.end(), 0);

    Eigen::VectorXd hanning_window(window_size);
    for (uint32_t i = 0; i < window_size; ++i) {
        hanning_window(i) = 0.5 * (1.0 - std::cos(2.0 * M_PI * i / (window_size - 1)));
    }

    std::for_each(std::execution::par_unseq, indices.begin(), indices.end(),
        [&](size_t i) {
            const size_t start_idx = i * hop_size;
            const size_t end_idx = std::min(start_idx + window_size, data.size());
            auto window = data.subspan(start_idx, end_idx - start_idx);

            Eigen::VectorXd windowed_data = Eigen::VectorXd::Zero(window_size);
            const size_t actual_size = window.size();

            for (int j = 0; j < actual_size; ++j) {
                windowed_data(j) = window[j] * hanning_window(j);
            }

            Eigen::FFT<double> fft;
            Eigen::VectorXcd fft_result;
            fft.fwd(fft_result, windowed_data);

            const int harmonic_bins = std::max(1, static_cast<int>(fft_result.size() / 8));
            double energy = 0.0;

            for (int j = 1; j < harmonic_bins; ++j) {
                energy += std::norm(fft_result(j));
            }

            harmonic_energy[i] = energy / static_cast<double>(harmonic_bins);
        });

    return harmonic_energy;
}

std::vector<double> compute_mean_statistic(std::span<const double> data, const size_t num_windows, const uint32_t hop_size, const uint32_t window_size)
{
    std::vector<double> mean_values(num_windows);

    std::vector<size_t> indices(num_windows);
    std::iota(indices.begin(), indices.end(), 0);

    std::for_each(std::execution::par_unseq, indices.begin(), indices.end(),
        [&](size_t i) {
            const size_t start_idx = i * hop_size;
            const size_t end_idx = std::min(start_idx + window_size, data.size());
            auto window = data.subspan(start_idx, end_idx - start_idx);

            double sum = 0.0;
            for (double sample : window) {
                sum += sample;
            }
            mean_values[i] = sum / static_cast<double>(window.size());
        });

    return mean_values;
}

std::vector<double> compute_variance_statistic(std::span<const double> data, const size_t num_windows, const uint32_t hop_size, const uint32_t window_size, bool sample_variance)
{
    std::vector<double> variance_values(num_windows);

    std::vector<size_t> indices(num_windows);
    std::iota(indices.begin(), indices.end(), 0);

    std::for_each(std::execution::par_unseq, indices.begin(), indices.end(),
        [&](size_t i) {
            const size_t start_idx = i * hop_size;
            const size_t end_idx = std::min(start_idx + window_size, data.size());
            auto window = data.subspan(start_idx, end_idx - start_idx);

            if (window.size() <= 1) {
                variance_values[i] = 0.0;
                return;
            }

            double sum = 0.0;
            for (double sample : window) {
                sum += sample;
            }
            double mean = sum / static_cast<double>(window.size());

            double sum_sq_diff = 0.0;
            for (double sample : window) {
                double diff = sample - mean;
                sum_sq_diff += diff * diff;
            }

            double divisor = sample_variance ? static_cast<double>(window.size() - 1) : static_cast<double>(window.size());

            variance_values[i] = sum_sq_diff / divisor;
        });

    return variance_values;
}

std::vector<double> compute_std_dev_statistic(std::span<const double> data, const size_t num_windows, const uint32_t hop_size, const uint32_t window_size, bool sample_variance)
{
    auto variance_values = compute_variance_statistic(data, num_windows, hop_size, window_size, sample_variance);

    std::vector<double> std_dev_values(num_windows);
    std::transform(std::execution::par_unseq, variance_values.begin(), variance_values.end(), std_dev_values.begin(),
        [](double variance) { return std::sqrt(variance); });

    return std_dev_values;
}

std::vector<double> compute_skewness_statistic(std::span<const double> data, const size_t num_windows, const uint32_t hop_size, const uint32_t window_size)
{
    std::vector<double> skewness_values(num_windows);

    std::vector<size_t> indices(num_windows);
    std::iota(indices.begin(), indices.end(), 0);

    std::for_each(std::execution::par_unseq, indices.begin(), indices.end(),
        [&](size_t i) {
            const size_t start_idx = i * hop_size;
            const size_t end_idx = std::min(start_idx + window_size, data.size());
            auto window = data.subspan(start_idx, end_idx - start_idx);

            if (window.size() < 2) {
                skewness_values[i] = 0.0;
                return;
            }

            double sum = 0.0;
            for (double sample : window) {
                sum += sample;
            }
            double mean = sum / static_cast<double>(window.size());

            double sum_sq_diff = 0.0;
            double sum_cube_diff = 0.0;
            for (double sample : window) {
                double diff = sample - mean;
                double sq_diff = diff * diff;
                sum_sq_diff += sq_diff;
                sum_cube_diff += sq_diff * diff;
            }

            double variance = sum_sq_diff / static_cast<double>(window.size());
            if (variance <= 0.0) {
                skewness_values[i] = 0.0;
                return;
            }

            double std_dev = std::sqrt(variance);
            double third_moment = sum_cube_diff / static_cast<double>(window.size());

            skewness_values[i] = third_moment / (std_dev * std_dev * std_dev);
        });

    return skewness_values;
}

std::vector<double> compute_kurtosis_statistic(std::span<const double> data, const size_t num_windows, const uint32_t hop_size, const uint32_t window_size)
{
    std::vector<double> kurtosis_values(num_windows);

    std::vector<size_t> indices(num_windows);
    std::iota(indices.begin(), indices.end(), 0);

    std::for_each(std::execution::par_unseq, indices.begin(), indices.end(),
        [&](size_t i) {
            const size_t start_idx = i * hop_size;
            const size_t end_idx = std::min(start_idx + window_size, data.size());
            auto window = data.subspan(start_idx, end_idx - start_idx);

            if (window.size() < 2) {
                kurtosis_values[i] = 0.0;
                return;
            }

            double sum = 0.0;
            for (double sample : window) {
                sum += sample;
            }
            double mean = sum / static_cast<double>(window.size());

            double sum_sq_diff = 0.0;
            double sum_fourth_diff = 0.0;
            for (double sample : window) {
                double diff = sample - mean;
                double sq_diff = diff * diff;
                sum_sq_diff += sq_diff;
                sum_fourth_diff += sq_diff * sq_diff;
            }

            double variance = sum_sq_diff / static_cast<double>(window.size());
            if (variance <= 0.0) {
                kurtosis_values[i] = 0.0;
                return;
            }

            double fourth_moment = sum_fourth_diff / static_cast<double>(window.size());

            kurtosis_values[i] = (fourth_moment / (variance * variance)) - 3.0;
        });

    return kurtosis_values;
}

std::vector<double> compute_median_statistic(std::span<const double> data, const size_t num_windows, const uint32_t hop_size, const uint32_t window_size)
{
    std::vector<double> median_values(num_windows);

    std::vector<size_t> indices(num_windows);
    std::iota(indices.begin(), indices.end(), 0);

    std::for_each(std::execution::par_unseq, indices.begin(), indices.end(),
        [&](size_t i) {
            const size_t start_idx = i * hop_size;
            const size_t end_idx = std::min(start_idx + window_size, data.size());
            auto window = data.subspan(start_idx, end_idx - start_idx);

            if (window.empty()) {
                median_values[i] = 0.0;
                return;
            }

            std::vector<double> sorted_window(window.begin(), window.end());
            std::ranges::sort(sorted_window);

            size_t n = sorted_window.size();
            if (n % 2 == 0) {
                median_values[i] = (sorted_window[n / 2 - 1] + sorted_window[n / 2]) / 2.0;
            } else {
                median_values[i] = sorted_window[n / 2];
            }
        });

    return median_values;
}

std::vector<double> compute_percentile_statistic(std::span<const double> data, const size_t num_windows, const uint32_t hop_size, const uint32_t window_size, double percentile)
{
    std::vector<double> percentile_values(num_windows);

    if (percentile < 0.0)
        percentile = 0.0;
    if (percentile > 100.0)
        percentile = 100.0;

    std::vector<size_t> indices(num_windows);
    std::iota(indices.begin(), indices.end(), 0);

    std::for_each(std::execution::par_unseq, indices.begin(), indices.end(),
        [&](size_t i) {
            const size_t start_idx = i * hop_size;
            const size_t end_idx = std::min(start_idx + window_size, data.size());
            auto window = data.subspan(start_idx, end_idx - start_idx);

            if (window.empty()) {
                percentile_values[i] = 0.0;
                return;
            }

            std::vector<double> sorted_window(window.begin(), window.end());
            std::ranges::sort(sorted_window);

            if (percentile == 0.0) {
                percentile_values[i] = sorted_window.front();
                return;
            }
            if (percentile == 100.0) {
                percentile_values[i] = sorted_window.back();
                return;
            }

            double index = (percentile / 100.0) * static_cast<double>(sorted_window.size() - 1);
            auto lower_idx = static_cast<size_t>(std::floor(index));
            auto upper_idx = static_cast<size_t>(std::ceil(index));

            if (lower_idx == upper_idx) {
                percentile_values[i] = sorted_window[lower_idx];
            } else {
                double weight = index - static_cast<double>(lower_idx);
                percentile_values[i] = sorted_window[lower_idx] * (1.0 - weight) + sorted_window[upper_idx] * weight;
            }
        });

    return percentile_values;
}

std::vector<double> compute_entropy_statistic(std::span<const double> data, const size_t num_windows, const uint32_t hop_size, const uint32_t window_size, size_t num_bins)
{
    std::vector<double> entropy_values(num_windows);

    std::vector<size_t> indices(num_windows);
    std::iota(indices.begin(), indices.end(), 0);

    std::for_each(std::execution::par_unseq, indices.begin(), indices.end(),
        [&](size_t i) {
            const size_t start_idx = i * hop_size;
            const size_t end_idx = std::min(start_idx + window_size, data.size());
            auto window = data.subspan(start_idx, end_idx - start_idx);

            if (window.empty()) {
                entropy_values[i] = 0.0;
                return;
            }

            size_t bins = num_bins;
            if (bins == 0) {
                bins = static_cast<size_t>(std::ceil(std::log2(window.size()) + 1));
                bins = std::max(bins, static_cast<size_t>(1));
                bins = std::min(bins, window.size());
            }

            auto [min_it, max_it] = std::ranges::minmax_element(window);
            double min_val = *min_it;
            double max_val = *max_it;

            if (max_val <= min_val) {
                entropy_values[i] = 0.0;
                return;
            }

            double bin_width = (max_val - min_val) / static_cast<double>(bins);

            std::vector<size_t> bin_counts(bins, 0);
            for (double value : window) {
                auto bin_idx = static_cast<size_t>((value - min_val) / bin_width);
                if (bin_idx >= bins)
                    bin_idx = bins - 1;
                bin_counts[bin_idx]++;
            }

            double entropy = 0.0;
            size_t total_count = window.size();

            for (size_t count : bin_counts) {
                if (count > 0) {
                    double probability = static_cast<double>(count) / static_cast<double>(total_count);
                    entropy -= probability * std::log2(probability);
                }
            }

            entropy_values[i] = entropy;
        });

    return entropy_values;
}

std::vector<double> compute_min_statistic(std::span<const double> data, const size_t num_windows, const uint32_t hop_size, const uint32_t window_size)
{
    std::vector<double> min_values(num_windows);

    std::vector<size_t> indices(num_windows);
    std::iota(indices.begin(), indices.end(), 0);

    std::for_each(std::execution::par_unseq, indices.begin(), indices.end(),
        [&](size_t i) {
            const size_t start_idx = i * hop_size;
            const size_t end_idx = std::min(start_idx + window_size, data.size());
            auto window = data.subspan(start_idx, end_idx - start_idx);

            min_values[i] = *std::ranges::min_element(window);
        });

    return min_values;
}

std::vector<double> compute_max_statistic(std::span<const double> data, const size_t num_windows, const uint32_t hop_size, const uint32_t window_size)
{
    std::vector<double> max_values(num_windows);

    std::vector<size_t> indices(num_windows);
    std::iota(indices.begin(), indices.end(), 0);

    std::for_each(std::execution::par_unseq, indices.begin(), indices.end(),
        [&](size_t i) {
            const size_t start_idx = i * hop_size;
            const size_t end_idx = std::min(start_idx + window_size, data.size());
            auto window = data.subspan(start_idx, end_idx - start_idx);

            max_values[i] = *std::ranges::max_element(window);
        });

    return max_values;
}

std::vector<double> compute_range_statistic(std::span<const double> data, const size_t num_windows, const uint32_t hop_size, const uint32_t window_size)
{
    std::vector<double> range_values(num_windows);

    std::vector<size_t> indices(num_windows);
    std::iota(indices.begin(), indices.end(), 0);

    std::for_each(std::execution::par_unseq, indices.begin(), indices.end(),
        [&](size_t i) {
            const size_t start_idx = i * hop_size;
            const size_t end_idx = std::min(start_idx + window_size, data.size());
            auto window = data.subspan(start_idx, end_idx - start_idx);

            auto [min_it, max_it] = std::ranges::minmax_element(window);
            range_values[i] = *max_it - *min_it;
        });

    return range_values;
}

std::vector<double> compute_sum_statistic(std::span<const double> data, const size_t num_windows, const uint32_t hop_size, const uint32_t window_size)
{
    std::vector<double> sum_values(num_windows);

    std::vector<size_t> indices(num_windows);
    std::iota(indices.begin(), indices.end(), 0);

    std::for_each(std::execution::par_unseq, indices.begin(), indices.end(),
        [&](size_t i) {
            const size_t start_idx = i * hop_size;
            const size_t end_idx = std::min(start_idx + window_size, data.size());
            auto window = data.subspan(start_idx, end_idx - start_idx);

            sum_values[i] = std::accumulate(window.begin(), window.end(), 0.0);
        });

    return sum_values;
}

std::vector<double> compute_count_statistic(std::span<const double> data, const size_t num_windows, const uint32_t hop_size, const uint32_t window_size)
{
    std::vector<double> count_values(num_windows);

    std::vector<size_t> indices(num_windows);
    std::iota(indices.begin(), indices.end(), 0);

    std::for_each(std::execution::par_unseq, indices.begin(), indices.end(),
        [&](size_t i) {
            const size_t start_idx = i * hop_size;
            const size_t end_idx = std::min(start_idx + window_size, data.size());

            count_values[i] = static_cast<double>(end_idx - start_idx);
        });

    return count_values;
}

std::vector<double> compute_mad_statistic(std::span<const double> data, const size_t num_windows, const uint32_t hop_size, const uint32_t window_size)
{
    std::vector<double> mad_values(num_windows);

    std::vector<size_t> indices(num_windows);
    std::iota(indices.begin(), indices.end(), 0);

    std::for_each(std::execution::par_unseq, indices.begin(), indices.end(),
        [&](size_t i) {
            const size_t start_idx = i * hop_size;
            const size_t end_idx = std::min(start_idx + window_size, data.size());
            auto window = data.subspan(start_idx, end_idx - start_idx);

            if (window.empty()) {
                mad_values[i] = 0.0;
                return;
            }

            std::vector<double> sorted_window(window.begin(), window.end());
            std::ranges::sort(sorted_window);

            double median {};
            size_t n = sorted_window.size();
            if (n % 2 == 0) {
                median = (sorted_window[n / 2 - 1] + sorted_window[n / 2]) / 2.0;
            } else {
                median = sorted_window[n / 2];
            }

            std::vector<double> abs_deviations;
            abs_deviations.reserve(window.size());
            for (double val : window) {
                abs_deviations.push_back(std::abs(val - median));
            }

            std::ranges::sort(abs_deviations);
            size_t mad_n = abs_deviations.size();
            if (mad_n % 2 == 0) {
                mad_values[i] = (abs_deviations[mad_n / 2 - 1] + abs_deviations[mad_n / 2]) / 2.0;
            } else {
                mad_values[i] = abs_deviations[mad_n / 2];
            }
        });

    return mad_values;
}

std::vector<double> compute_cv_statistic(std::span<const double> data, const size_t num_windows, const uint32_t hop_size, const uint32_t window_size, bool sample_variance)
{
    auto mean_vals = compute_mean_statistic(data, num_windows, hop_size, window_size);
    auto std_vals = compute_std_dev_statistic(data, num_windows, hop_size, window_size, sample_variance);

    std::vector<double> cv_values(num_windows);
    std::transform(std::execution::par_unseq, mean_vals.begin(), mean_vals.end(), std_vals.begin(), cv_values.begin(),
        [](double mean, double std_dev) {
            return (std::abs(mean) > 1e-15) ? std_dev / mean : 0.0;
        });

    return cv_values;
}

std::vector<double> compute_mode_statistic(std::span<const double> data, const size_t num_windows, const uint32_t hop_size, const uint32_t window_size)
{
    std::vector<double> mode_values(num_windows);
    constexpr double tolerance = 1e-10;

    std::vector<size_t> indices(num_windows);
    std::iota(indices.begin(), indices.end(), 0);

    std::for_each(std::execution::par_unseq, indices.begin(), indices.end(),
        [&](size_t i) {
            const size_t start_idx = i * hop_size;
            const size_t end_idx = std::min(start_idx + window_size, data.size());
            auto window = data.subspan(start_idx, end_idx - start_idx);

            if (window.empty()) {
                mode_values[i] = 0.0;
                return;
            }

            std::map<int64_t, std::pair<double, size_t>> frequency_map;

            for (double value : window) {
                auto bucket = static_cast<int64_t>(std::round(value / tolerance));
                if (frequency_map.find(bucket) == frequency_map.end()) {
                    frequency_map[bucket] = { value, 1 };
                } else {
                    frequency_map[bucket].second++;
                    frequency_map[bucket].first = (frequency_map[bucket].first * static_cast<double>(frequency_map[bucket].second - 1) + value) / static_cast<double>(frequency_map[bucket].second);
                }
            }

            auto max_count_it = std::ranges::max_element(frequency_map,
                [](const auto& a, const auto& b) { return a.second.second < b.second.second; });

            mode_values[i] = max_count_it != frequency_map.end() ? max_count_it->second.first : window[0];
        });

    return mode_values;
}

std::vector<double> compute_zscore_statistic(std::span<const double> data, const size_t num_windows, const uint32_t hop_size, const uint32_t window_size, bool sample_variance)
{
    std::vector<double> zscore_values(num_windows);

    std::vector<size_t> indices(num_windows);
    std::iota(indices.begin(), indices.end(), 0);

    std::for_each(std::execution::par_unseq, indices.begin(), indices.end(),
        [&](size_t i) {
            const size_t start_idx = i * hop_size;
            const size_t end_idx = std::min(start_idx + window_size, data.size());
            auto window = data.subspan(start_idx, end_idx - start_idx);

            if (window.empty()) {
                zscore_values[i] = 0.0;
                return;
            }

            double sum = 0.0;
            for (double sample : window) {
                sum += sample;
            }
            double mean = sum / static_cast<double>(window.size());

            double sum_sq_diff = 0.0;
            for (double sample : window) {
                double diff = sample - mean;
                sum_sq_diff += diff * diff;
            }

            double divisor = sample_variance ? static_cast<double>(window.size() - 1) : static_cast<double>(window.size());

            double variance = sum_sq_diff / divisor;
            double std_dev = std::sqrt(variance);

            if (std_dev > 0.0) {
                double sum_zscore = 0.0;
                for (double val : window) {
                    sum_zscore += (val - mean) / std_dev;
                }
                zscore_values[i] = sum_zscore / static_cast<double>(window.size());
            } else {
                zscore_values[i] = 0.0;
            }
        });

    return zscore_values;
}

}
