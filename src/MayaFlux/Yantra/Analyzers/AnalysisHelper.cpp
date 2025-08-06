#include "AnalysisHelper.hpp"

#ifdef MAYAFLUX_PLATFORM_MACOS
#include "oneapi/dpl/algorithm"
#include "oneapi/dpl/execution"
#include "oneapi/dpl/numeric"
#else
#include "execution"
#endif

#include "unsupported/Eigen/FFT"

namespace MayaFlux::Yantra {

std::vector<double> compute_dynamic_range_energy(std::span<const double> data, const size_t num_windows, const u_int32_t hop_size, const u_int32_t window_size)
{
    std::vector<double> dr_values(num_windows);

    std::vector<size_t> indices(num_windows);
    std::iota(indices.begin(), indices.end(), 0);

    std::for_each(std::execution::par_unseq, indices.begin(), indices.end(),
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

std::vector<double> compute_zero_crossing_energy(std::span<const double> data, const size_t num_windows, const u_int32_t hop_size, const u_int32_t window_size)
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

std::vector<double> compute_power_energy(std::span<const double> data, const size_t num_windows, const u_int32_t hop_size, const u_int32_t window_size)
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

std::vector<double> compute_peak_energy(std::span<const double> data, const u_int32_t num_windows, const u_int32_t hop_size, const u_int32_t window_size)
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

std::vector<double> compute_rms_energy(std::span<const double> data, const u_int32_t num_windows, const u_int32_t hop_size, const u_int32_t window_size)
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

std::vector<double> compute_spectral_energy(std::span<const double> data, const size_t num_windows, const u_int32_t hop_size, const u_int32_t window_size)
{
    std::vector<double> spectral_energy(num_windows);

    std::vector<size_t> indices(num_windows);
    std::iota(indices.begin(), indices.end(), 0);

    Eigen::VectorXd hanning_window(window_size);
    for (u_int32_t i = 0; i < window_size; ++i) {
        hanning_window(i) = 0.5 * (1.0 - std::cos(2.0 * M_PI * i / (window_size - 1)));
    }

    std::for_each(std::execution::par_unseq, indices.begin(), indices.end(),
        [&](size_t i) {
            const size_t start_idx = i * hop_size;
            const size_t end_idx = std::min(start_idx + window_size, data.size());
            auto window = data.subspan(start_idx, end_idx - start_idx);

            Eigen::VectorXd windowed_data = Eigen::VectorXd::Zero(window_size);
            const size_t actual_size = window.size();

            for (size_t j = 0; j < actual_size; ++j) {
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

std::vector<double> compute_harmonic_energy(std::span<const double> data, const size_t num_windows, const u_int32_t hop_size, const u_int32_t window_size)
{
    std::vector<double> harmonic_energy(num_windows);

    std::vector<size_t> indices(num_windows);
    std::iota(indices.begin(), indices.end(), 0);

    Eigen::VectorXd hanning_window(window_size);
    for (u_int32_t i = 0; i < window_size; ++i) {
        hanning_window(i) = 0.5 * (1.0 - std::cos(2.0 * M_PI * i / (window_size - 1)));
    }

    std::for_each(std::execution::par_unseq, indices.begin(), indices.end(),
        [&](size_t i) {
            const size_t start_idx = i * hop_size;
            const size_t end_idx = std::min(start_idx + window_size, data.size());
            auto window = data.subspan(start_idx, end_idx - start_idx);

            Eigen::VectorXd windowed_data = Eigen::VectorXd::Zero(window_size);
            const size_t actual_size = window.size();

            for (size_t j = 0; j < actual_size; ++j) {
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

}
