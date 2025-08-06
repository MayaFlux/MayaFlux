#include "AnalysisHelper.hpp"

#ifdef MAYAFLUX_PLATFORM_MACOS
#include "oneapi/dpl/algorithm"
#include "oneapi/dpl/execution"
#include "oneapi/dpl/numeric"
#else
#include "execution"
#endif

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

}
