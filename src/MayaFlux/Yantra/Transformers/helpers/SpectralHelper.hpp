#pragma once

#include "MayaFlux/Nodes/Generators/WindowGenerator.hpp"

#include "MayaFlux/Yantra/OperationSpec/OperationHelper.hpp"

#include <unsupported/Eigen/FFT>

/**
 * @file SpectralTransform.hpp
 * @brief Spectral transformation functions leveraging existing ecosystem
 *
 * - Uses existing FFT infrastructure from AnalysisHelper
 * - Leverages existing windowing functions
 * - Preserves structural data through OperationHelper
 * - Integrates with EnergyAnalyzer for spectral analysis
 * - Thread-safe operations
 */
namespace MayaFlux::Yantra {

inline uint64_t smallest_size(std::vector<std::vector<double>>& data)
{
    if (data.empty())
        return 0;

    auto smallest = *std::ranges::min_element(data,
        [](const auto& a, const auto& b) {
            return a.size() < b.size();
        });

    return static_cast<uint64_t>(smallest.size());
}

/**
 * @brief Common spectral processing helper to eliminate code duplication
 * @tparam ProcessorFunc Function type for spectral processing
 * @param data Input data span
 * @param window_size Size of analysis window
 * @param hop_size Hop size between windows
 * @param processor Function to process spectrum in each window
 * @return Processed audio data
 */
template <typename ProcessorFunc>
std::vector<double> process_spectral_windows(
    std::span<double> data,
    uint32_t window_size,
    uint32_t hop_size,
    ProcessorFunc&& processor)
{
    const size_t num_windows = (data.size() >= window_size) ? (data.size() - window_size) / hop_size + 1 : 0;

    if (num_windows == 0) {
        return { data.begin(), data.end() };
    }

    std::vector<double> output(data.size(), 0.0);

    auto hann_window = Nodes::Generator::generate_window(window_size, Nodes::Generator::WindowType::HANNING);

    Eigen::FFT<double> fft;

    auto window_indices = std::views::iota(size_t { 0 }, num_windows);

    std::ranges::for_each(window_indices, [&](size_t win) {
        size_t start_idx = win * hop_size;
        auto window_data = data.subspan(start_idx,
            std::min(static_cast<size_t>(window_size), data.size() - start_idx));

        Eigen::VectorXd windowed = Eigen::VectorXd::Zero(static_cast<Eigen::Index>(window_size));
        const size_t actual_size = std::min(window_data.size(), hann_window.size());

        for (size_t j = 0; j < actual_size; ++j) {
            windowed(static_cast<Eigen::Index>(j)) = window_data[j] * hann_window[j];
        }

        Eigen::VectorXcd spectrum;
        fft.fwd(spectrum, windowed);

        std::forward<ProcessorFunc>(processor)(spectrum, win);

        Eigen::VectorXd result;
        fft.inv(result, spectrum);

        for (size_t i = 0; i < static_cast<size_t>(result.size()) && start_idx + i < output.size(); ++i) {
            output[start_idx + i] += result[static_cast<Eigen::Index>(i)];
        }
    });

    return output;
}

/**
 * @brief Windowing transformation using C++20 ranges (IN-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - WILL BE MODIFIED
 * @param window_type Type of window to apply
 * @param window_size Size of window (0 = full data size)
 * @return Windowed data
 */
template <OperationReadyData DataType>
DataType transform_window(DataType& input,
    Nodes::Generator::WindowType window_type,
    uint32_t window_size = 0)
{
    auto [target_data, structure_info] = OperationHelper::extract_structured_double(input);

    uint32_t size = window_size > 0 ? window_size : smallest_size(target_data);

    auto window = Nodes::Generator::generate_window(size, window_type);

    for (auto& span : target_data) {
        auto data_view = span | std::views::take(std::min(size, static_cast<uint32_t>(span.size())));
        auto window_view = window | std::views::take(data_view.size());

        std::ranges::transform(data_view, window_view, data_view.begin(),
            [](double sample, double win) { return sample * win; });
    }

    auto reconstructed_data = target_data
        | std::views::transform([](const auto& span) {
              return std::vector<double>(span.begin(), span.end());
          })
        | std::ranges::to<std::vector>();

    return OperationHelper::reconstruct_from_double<DataType>(reconstructed_data, structure_info);
}

/**
 * @brief Windowing transformation using C++20 ranges (OUT-OF-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - will NOT be modified
 * @param window_type Type of window to apply
 * @param window_size Size of window (0 = full data size)
 * @param working_buffer Buffer for operations (will be resized if needed)
 * @return Windowed data
 */
template <OperationReadyData DataType>
DataType transform_window(DataType& input,
    Nodes::Generator::WindowType window_type,
    uint32_t window_size,
    std::vector<std::vector<double>>& working_buffer)
{
    auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input, working_buffer);

    uint32_t size = window_size > 0 ? window_size : smallest_size(working_buffer);

    auto window = Nodes::Generator::generate_window(size, window_type);

    working_buffer.resize(target_data.size());
    for (size_t i = 0; i < target_data.size(); ++i) {
        auto& span = target_data[i];
        auto& buffer = working_buffer[i];
        buffer.resize(span.size());

        auto data_view = span | std::views::take(std::min(size, static_cast<uint32_t>(span.size())));
        auto window_view = window | std::views::take(data_view.size());

        std::ranges::transform(data_view, window_view, buffer.begin(),
            [](double sample, double win) { return sample * win; });

        if (span.size() > size) {
            std::copy(span.begin() + size, span.end(), buffer.begin() + size);
        }
    }

    return OperationHelper::reconstruct_from_double<DataType>(working_buffer, structure_info);
}

/**
 * @brief Spectral filtering using existing FFT infrastructure with C++20 ranges (IN-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - WILL BE MODIFIED
 * @param low_freq Low cutoff frequency (Hz)
 * @param high_freq High cutoff frequency (Hz)
 * @param sample_rate Sample rate (Hz)
 * @param window_size FFT window size
 * @param hop_size Hop size for overlap-add
 * @return Filtered data
 */
template <OperationReadyData DataType>
DataType transform_spectral_filter(DataType& input,
    double low_freq,
    double high_freq,
    double sample_rate = 48000.0,
    uint32_t window_size = 1024,
    uint32_t hop_size = 256)
{
    auto [target_data, structure_info] = OperationHelper::extract_structured_double(input);

    auto processor = [low_freq, high_freq, sample_rate](Eigen::VectorXcd& spectrum, size_t) {
        auto bin_indices = std::views::iota(0, static_cast<int>(spectrum.size()));
        std::ranges::for_each(bin_indices, [&](int bin) {
            double freq = (bin * sample_rate) / (2.0 * (double)spectrum.size());
            if (freq < low_freq || freq > high_freq) {
                spectrum[bin] = 0.0;
            }
        });
    };

    for (auto& span : target_data) {
        auto result = process_spectral_windows(span, window_size, hop_size, processor);
        std::ranges::copy(result, span.begin());
    }

    auto reconstructed_data = target_data
        | std::views::transform([](const auto& span) {
              return std::vector<double>(span.begin(), span.end());
          })
        | std::ranges::to<std::vector>();

    return OperationHelper::reconstruct_from_double<DataType>(reconstructed_data, structure_info);
}

/**
 * @brief Spectral filtering using existing FFT infrastructure with C++20 ranges (OUT-OF-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - will NOT be modified
 * @param low_freq Low cutoff frequency (Hz)
 * @param high_freq High cutoff frequency (Hz)
 * @param sample_rate Sample rate (Hz)
 * @param window_size FFT window size
 * @param hop_size Hop size for overlap-add
 * @param working_buffer Buffer for operations (will be resized if needed)
 * @return Filtered data
 */
template <OperationReadyData DataType>
DataType transform_spectral_filter(DataType& input,
    double low_freq,
    double high_freq,
    double sample_rate,
    uint32_t window_size,
    uint32_t hop_size,
    std::vector<std::vector<double>>& working_buffer)
{
    auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input, working_buffer);

    auto processor = [low_freq, high_freq, sample_rate](Eigen::VectorXcd& spectrum, size_t) {
        auto bin_indices = std::views::iota(0, static_cast<int>(spectrum.size()));
        std::ranges::for_each(bin_indices, [&](int bin) {
            double freq = (bin * sample_rate) / (2.0 * (double)spectrum.size());
            if (freq < low_freq || freq > high_freq) {
                spectrum[bin] = 0.0;
            }
        });
    };

    working_buffer.resize(target_data.size());
    for (size_t i = 0; i < target_data.size(); ++i) {
        working_buffer[i] = process_spectral_windows(target_data[i], window_size, hop_size, processor);
    }

    return OperationHelper::reconstruct_from_double<DataType>(working_buffer, structure_info);
}

/**
 * @brief Pitch shifting using existing FFT from AnalysisHelper with C++20 ranges (IN-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - WILL BE MODIFIED
 * @param semitones Pitch shift in semitones
 * @param window_size FFT window size
 * @param hop_size Hop size for overlap-add
 * @return Pitch-shifted data
 */
template <OperationReadyData DataType>
DataType transform_pitch_shift(DataType& input,
    double semitones,
    uint32_t window_size = 1024,
    uint32_t hop_size = 256)
{
    auto [target_data, structure_info] = OperationHelper::extract_structured_double(input);

    if (semitones == 0.0) {
        return input;
    }

    double pitch_ratio = std::pow(2.0, semitones / 12.0);

    auto processor = [pitch_ratio](Eigen::VectorXcd& spectrum, size_t) {
        Eigen::VectorXcd shifted_spectrum = Eigen::VectorXcd::Zero(spectrum.size());
        auto bin_indices = std::views::iota(0, static_cast<int>(spectrum.size()));

        std::ranges::for_each(bin_indices, [&](int bin) {
            int shifted_bin = static_cast<int>(bin * pitch_ratio);
            if (shifted_bin < shifted_spectrum.size()) {
                shifted_spectrum[shifted_bin] = spectrum[bin];
            }
        });

        spectrum = std::move(shifted_spectrum);
    };

    for (auto& span : target_data) {
        auto result = process_spectral_windows(span, window_size, hop_size, processor);
        std::ranges::copy(result, span.begin());
    }

    auto reconstructed_data = target_data
        | std::views::transform([](const auto& span) {
              return std::vector<double>(span.begin(), span.end());
          })
        | std::ranges::to<std::vector>();

    return OperationHelper::reconstruct_from_double<DataType>(reconstructed_data, structure_info);
}

/**
 * @brief Pitch shifting using existing FFT from AnalysisHelper with C++20 ranges (OUT-OF-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - will NOT be modified
 * @param semitones Pitch shift in semitones
 * @param window_size FFT window size
 * @param hop_size Hop size for overlap-add
 * @param working_buffer Buffer for operations (will be resized if needed)
 * @return Pitch-shifted data
 */
template <OperationReadyData DataType>
DataType transform_pitch_shift(DataType& input,
    double semitones,
    uint32_t window_size,
    uint32_t hop_size,
    std::vector<std::vector<double>>& working_buffer)
{
    auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input, working_buffer);

    if (semitones == 0.0) {
        return input;
    }

    double pitch_ratio = std::pow(2.0, semitones / 12.0);

    auto processor = [pitch_ratio](Eigen::VectorXcd& spectrum, size_t) {
        Eigen::VectorXcd shifted_spectrum = Eigen::VectorXcd::Zero(spectrum.size());
        auto bin_indices = std::views::iota(0, static_cast<int>(spectrum.size()));

        std::ranges::for_each(bin_indices, [&](int bin) {
            int shifted_bin = static_cast<int>(bin * pitch_ratio);
            if (shifted_bin < shifted_spectrum.size()) {
                shifted_spectrum[shifted_bin] = spectrum[bin];
            }
        });

        spectrum = std::move(shifted_spectrum);
    };

    working_buffer.resize(target_data.size());
    for (size_t i = 0; i < target_data.size(); ++i) {
        working_buffer[i] = process_spectral_windows(target_data[i], window_size, hop_size, processor);
    }

    return OperationHelper::reconstruct_from_double<DataType>(working_buffer, structure_info);
}

/**
 * @brief Spectral inversion (phase inversion in frequency domain) using C++20 ranges (IN-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - WILL BE MODIFIED
 * @param window_size FFT window size
 * @param hop_size Hop size for overlap-add
 * @return Spectrally inverted data
 */
template <OperationReadyData DataType>
DataType transform_spectral_invert(DataType& input,
    uint32_t window_size = 1024,
    uint32_t hop_size = 256)
{
    auto [target_data, structure_info] = OperationHelper::extract_structured_double(input);

    auto processor = [](Eigen::VectorXcd& spectrum, size_t) {
        std::ranges::transform(spectrum, spectrum.begin(),
            [](const std::complex<double>& bin) { return std::conj(bin); });
    };

    for (auto& span : target_data) {
        auto result = process_spectral_windows(span, window_size, hop_size, processor);
        std::ranges::copy(result, span.begin());
    }

    auto reconstructed_data = target_data
        | std::views::transform([](const auto& span) {
              return std::vector<double>(span.begin(), span.end());
          })
        | std::ranges::to<std::vector>();

    return OperationHelper::reconstruct_from_double<DataType>(reconstructed_data, structure_info);
}

/**
 * @brief Spectral inversion (phase inversion in frequency domain) using C++20 ranges (OUT-OF-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - will NOT be modified
 * @param window_size FFT window size
 * @param hop_size Hop size for overlap-add
 * @param working_buffer Buffer for operations (will be resized if needed)
 * @return Spectrally inverted data
 */
template <OperationReadyData DataType>
DataType transform_spectral_invert(DataType& input,
    uint32_t window_size,
    uint32_t hop_size,
    std::vector<std::vector<double>>& working_buffer)
{
    auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input, working_buffer);

    auto processor = [](Eigen::VectorXcd& spectrum, size_t) {
        std::ranges::transform(spectrum, spectrum.begin(),
            [](const std::complex<double>& bin) { return std::conj(bin); });
    };

    working_buffer.resize(target_data.size());
    for (size_t i = 0; i < target_data.size(); ++i) {
        working_buffer[i] = process_spectral_windows(target_data[i], window_size, hop_size, processor);
    }

    return OperationHelper::reconstruct_from_double<DataType>(working_buffer, structure_info);
}

}
