#pragma once

#include "MayaFlux/Nodes/Filters/FIR.hpp"
#include "MayaFlux/Yantra/OperationSpec/OperationHelper.hpp"
#include <unsupported/Eigen/FFT>

namespace MayaFlux::Yantra {

/**
 * @brief Common FFT convolution helper to eliminate code duplication
 * @param data_span Input data
 * @param kernel Convolution kernel
 * @param operation Function to apply in frequency domain
 * @return Convolved result
 */
template <typename OperationFunc>
std::vector<double> fft_convolve_helper(
    std::span<const double> data_span,
    std::span<const double> kernel,
    OperationFunc&& operation,
    bool return_full_size = true)
{
    if (data_span.empty() || kernel.empty()) {
        if (return_full_size && !data_span.empty() && !kernel.empty()) {
            return std::vector<double>(data_span.size() + kernel.size() - 1, 0.0);
        } else {
            return std::vector<double>(data_span.size(), 0.0);
        }
    }

    size_t conv_size = data_span.size() + kernel.size() - 1;
    /*
    size_t fft_size = 1;
    while (fft_size < conv_size)
        fft_size *= 2;

    if (fft_size < 2) {
        fft_size = 2;
    }
    */

    size_t fft_size = std::max(256uz, conv_size);
    fft_size = 1 << (32 - __builtin_clz(fft_size - 1));

    Eigen::VectorXd padded_input = Eigen::VectorXd::Zero(fft_size);
    Eigen::VectorXd padded_kernel = Eigen::VectorXd::Zero(fft_size);

    std::ranges::copy(data_span, padded_input.begin());
    std::ranges::copy(kernel, padded_kernel.begin());

    Eigen::FFT<double> fft;
    Eigen::VectorXcd input_fft, kernel_fft;
    fft.fwd(input_fft, padded_input);
    fft.fwd(kernel_fft, padded_kernel);

    Eigen::VectorXcd result_fft(fft_size);
    operation(input_fft, kernel_fft, result_fft);

    // new
    double scale_factor = 1.0 / fft_size;
    std::ranges::transform(result_fft, result_fft.begin(),
        [scale_factor](std::complex<double>& c) {
            return c * scale_factor;
        });

    Eigen::VectorXd time_result;
    fft.inv(time_result, result_fft);

    if (return_full_size) {
        std::vector<double> result(conv_size);
        std::ranges::copy(time_result | std::views::take(conv_size), result.begin());
        return result;
    } else {
        std::vector<double> result(data_span.size());
        std::ranges::copy(time_result | std::views::take(data_span.size()), result.begin());
        return result;
    }
}

/**
 * @brief Direct convolution using existing FIR filter infrastructure (IN-PLACE)
 * This leverages the existing, well-tested filter implementation
 * @tparam DataType OperationReadyData type
 * @param input Input data - WILL BE MODIFIED
 * @param impulse_response Impulse response for convolution
 * @return Convolved data
 */
template <OperationReadyData DataType>
DataType transform_convolve_with_fir(DataType& input, const std::vector<double>& impulse_response)
{
    auto [target_data, structure_info] = OperationHelper::extract_structured_double(input);

    for (auto& span : target_data) {
        auto fir_filter = std::make_shared<Nodes::Filters::FIR>(nullptr, impulse_response);
        std::vector<double> output;
        output.reserve(span.size());
        for (double sample : span) {
            output.push_back(fir_filter->process_sample(sample));
        }
        std::copy(output.begin(), output.end(), span.begin());
    }

    auto reconstructed_data = target_data
        | std::views::transform([](const auto& span) {
              return std::vector<double>(span.begin(), span.end());
          })
        | std::ranges::to<std::vector>();

    return OperationHelper::reconstruct_from_double<DataType>(reconstructed_data, structure_info);
}

/**
 * @brief Direct convolution using existing FIR filter infrastructure (OUT-OF-PLACE)
 * This leverages the existing, well-tested filter implementation
 * @tparam DataType OperationReadyData type
 * @param input Input data - will NOT be modified
 * @param impulse_response Impulse response for convolution
 * @param working_buffer Buffer for operations (will be resized if needed)
 * @return Convolved data
 */
template <OperationReadyData DataType>
DataType transform_convolve_with_fir(DataType& input, const std::vector<double>& impulse_response, std::vector<std::vector<double>>& working_buffer)
{
    auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input, working_buffer);

    working_buffer.clear();
    working_buffer.resize(target_data.size());

    for (size_t i = 0; i < target_data.size(); ++i) {
        auto fir_filter = std::make_shared<Nodes::Filters::FIR>(nullptr, impulse_response);
        working_buffer[i].reserve(target_data[i].size());
        for (double sample : target_data[i]) {
            working_buffer[i].push_back(fir_filter->process_sample(sample));
        }
    }

    return OperationHelper::reconstruct_from_double<DataType>(working_buffer, structure_info);
}

/**
 * @brief Convolution transformation using existing infrastructure with C++20 ranges (IN-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - WILL BE MODIFIED
 * @param impulse_response Impulse response for convolution
 * @return Convolved data
 */
template <OperationReadyData DataType>
DataType transform_convolve(DataType& input, const std::vector<double>& impulse_response)
{
    if (impulse_response.size() == 1) {
        if (std::abs(impulse_response[0] - 1.0) < 1e-15) {
            return input;
        } else {
            auto [target_data, structure_info] = OperationHelper::extract_structured_double(input);

            std::vector<std::vector<double>> reconstructed_data(target_data.size());

            for (size_t i = 0; i < target_data.size(); ++i) {
                std::ranges::transform(target_data[i], reconstructed_data[i].begin(),
                    [gain = impulse_response[0]](double x) { return x * gain; });
            }
            return OperationHelper::reconstruct_from_double<DataType>(reconstructed_data, structure_info);
        }
    }

    auto [target_data, structure_info] = OperationHelper::extract_structured_double(input);

    auto convolution_op = [](const Eigen::VectorXcd& input_fft,
                              const Eigen::VectorXcd& kernel_fft,
                              Eigen::VectorXcd& result_fft) {
        std::ranges::transform(input_fft, kernel_fft, result_fft.begin(),
            [](const std::complex<double>& a, const std::complex<double>& b) {
                return a * b;
            });
    };

    for (auto& span : target_data) {
        auto result = fft_convolve_helper(span, std::span<const double>(impulse_response), convolution_op, false);
        std::copy(result.begin(), result.end(), span.begin());
    }

    auto reconstructed_data = target_data
        | std::views::transform([](const auto& span) {
              return std::vector<double>(span.begin(), span.end());
          })
        | std::ranges::to<std::vector>();

    return OperationHelper::reconstruct_from_double<DataType>(reconstructed_data, structure_info);
}

/**
 * @brief Convolution transformation using existing infrastructure with C++20 ranges (OUT-OF-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - will NOT be modified
 * @param impulse_response Impulse response for convolution
 * @param working_buffer Buffer for operations (will be resized if needed)
 * @return Convolved data
 */
template <OperationReadyData DataType>
DataType transform_convolve(DataType& input, const std::vector<double>& impulse_response, std::vector<std::vector<double>>& working_buffer)
{
    if (impulse_response.size() == 1) {
        if (std::abs(impulse_response[0] - 1.0) < 1e-15) {
            return input;
        } else {
            auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input, working_buffer);
            for (size_t i = 0; i < target_data.size(); ++i) {
                std::ranges::transform(target_data[i], working_buffer[i].begin(),
                    [gain = impulse_response[0]](double x) { return x * gain; });
            }
            return OperationHelper::reconstruct_from_double<DataType>(working_buffer, structure_info);
        }
    }

    auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input, working_buffer);

    working_buffer.clear();
    working_buffer.resize(target_data.size());

    auto convolution_op = [](const Eigen::VectorXcd& input_fft,
                              const Eigen::VectorXcd& kernel_fft,
                              Eigen::VectorXcd& result_fft) {
        std::ranges::transform(input_fft, kernel_fft, result_fft.begin(),
            [](const std::complex<double>& a, const std::complex<double>& b) {
                return a * b;
            });
    };

    for (size_t i = 0; i < target_data.size(); ++i) {
        working_buffer[i] = fft_convolve_helper(target_data[i], std::span<const double>(impulse_response), convolution_op, false);
    }

    return OperationHelper::reconstruct_from_double<DataType>(working_buffer, structure_info);
}

/**
 * @brief Cross-correlation using FFT (convolution with time-reversed impulse) (IN-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - WILL BE MODIFIED
 * @param template_signal Template signal for correlation
 * @param normalize Whether to normalize the result
 * @return Cross-correlated data
 */
template <OperationReadyData DataType>
DataType transform_cross_correlate(DataType& input, const std::vector<double>& template_signal, bool normalize = true)
{
    auto [target_data, structure_info] = OperationHelper::extract_structured_double(input);

    std::vector<double> reversed_template(template_signal.rbegin(), template_signal.rend());

    auto correlation_op = [](const Eigen::VectorXcd& input_fft,
                              const Eigen::VectorXcd& kernel_fft,
                              Eigen::VectorXcd& result_fft) {
        std::ranges::transform(input_fft, kernel_fft, result_fft.begin(),
            [](const std::complex<double>& a, const std::complex<double>& b) {
                return a * std::conj(b);
            });
    };

    for (auto& span : target_data) {
        auto result = fft_convolve_helper(span, reversed_template, correlation_op);

        if (normalize && !result.empty()) {
            auto [min_it, max_it] = std::ranges::minmax_element(result);
            double max_abs = std::max(std::abs(*min_it), std::abs(*max_it));
            if (max_abs > 0.0) {
                std::ranges::transform(result, result.begin(),
                    [max_abs](double val) { return val / max_abs; });
            }
        }

        std::copy(result.begin(), result.begin() + std::min(result.size(), span.size()), span.begin());
    }

    auto reconstructed_data = target_data
        | std::views::transform([](const auto& span) {
              return std::vector<double>(span.begin(), span.end());
          })
        | std::ranges::to<std::vector>();

    return OperationHelper::reconstruct_from_double<DataType>(reconstructed_data, structure_info);
}

/**
 * @brief Cross-correlation using FFT (convolution with time-reversed impulse) (OUT-OF-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - will NOT be modified
 * @param template_signal Template signal for correlation
 * @param normalize Whether to normalize the result
 * @param working_buffer Buffer for operations (will be resized if needed)
 * @return Cross-correlated data
 */
template <OperationReadyData DataType>
DataType transform_cross_correlate(DataType& input, const std::vector<double>& template_signal, bool normalize, std::vector<std::vector<double>>& working_buffer)
{
    auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input, working_buffer);

    std::vector<double> reversed_template(template_signal.rbegin(), template_signal.rend());

    auto correlation_op = [](const Eigen::VectorXcd& input_fft,
                              const Eigen::VectorXcd& kernel_fft,
                              Eigen::VectorXcd& result_fft) {
        std::ranges::transform(input_fft, kernel_fft, result_fft.begin(),
            [](const std::complex<double>& a, const std::complex<double>& b) {
                return a * std::conj(b);
            });
    };

    for (size_t i = 0; i < target_data.size(); ++i) {
        auto result = fft_convolve_helper(target_data[i], reversed_template, correlation_op);

        if (normalize && !result.empty()) {
            auto [min_it, max_it] = std::ranges::minmax_element(result);
            double max_abs = std::max(std::abs(*min_it), std::abs(*max_it));
            if (max_abs > 0.0) {
                std::ranges::transform(result, result.begin(),
                    [max_abs](double val) { return val / max_abs; });
            }
        }

        working_buffer[i].resize(result.size());
        working_buffer[i] = std::move(result);
    }

    return OperationHelper::reconstruct_from_double<DataType>(working_buffer, structure_info);
}

/**
 * @brief Auto-correlation using FFT (IN-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - will be modified
 * @param normalize Whether to normalize the result
 * @return Auto-correlated data
 */
template <OperationReadyData DataType>
DataType transform_auto_correlate_fft(DataType& input, bool normalize = true)
{
    auto [target_data, structure_info] = OperationHelper::extract_structured_double(input);

    // For auto-correlation using FFT:
    // R(k) = IFFT(FFT(x) * conj(FFT(x)))

    auto correlation_op = [](const Eigen::VectorXcd& input_fft,
                              const Eigen::VectorXcd& /* unused */,
                              Eigen::VectorXcd& result_fft) {
        std::ranges::transform(input_fft, input_fft, result_fft.begin(),
            [](const std::complex<double>& a, const std::complex<double>& b) {
                return a * std::conj(b);
            });
    };

    for (auto& span : target_data) {
        std::vector<double> signal_copy(span.begin(), span.end());
        auto result = fft_convolve_helper(span, signal_copy, correlation_op);

        if (normalize && !result.empty()) {
            double max_val = *std::max_element(result.begin(), result.end());
            if (max_val > 0.0) {
                std::ranges::transform(result, result.begin(),
                    [max_val](double val) { return val / max_val; });
            }
        }

        std::copy(result.begin(), result.begin() + std::min(result.size(), span.size()), span.begin());
    }

    auto reconstructed_data = target_data
        | std::views::transform([](const auto& span) {
              return std::vector<double>(span.begin(), span.end());
          })
        | std::ranges::to<std::vector>();

    return OperationHelper::reconstruct_from_double<DataType>(reconstructed_data, structure_info);
}

/**
 * @brief Auto-correlation using FFT (OUT-OF-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - will NOT be modified
 * @param working_buffer Buffer for operations (will be resized if needed)
 * @param normalize Whether to normalize the result
 * @return Auto-correlated data
 */
template <OperationReadyData DataType>
DataType transform_auto_correlate_fft(DataType& input, std::vector<std::vector<double>>& working_buffer, bool normalize = true)
{
    auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input, working_buffer);

    auto correlation_op = [](const Eigen::VectorXcd& input_fft,
                              const Eigen::VectorXcd&,
                              Eigen::VectorXcd& result_fft) {
        std::ranges::transform(input_fft, input_fft, result_fft.begin(),
            [](const std::complex<double>& a, const std::complex<double>& b) {
                return a * std::conj(b);
            });
    };

    for (size_t i = 0; i < target_data.size(); ++i) {
        std::vector<double> signal_copy(target_data[i].begin(), target_data[i].end());
        auto result = fft_convolve_helper(target_data[i], signal_copy, correlation_op);

        if (normalize && !result.empty()) {
            double max_val = *std::max_element(result.begin(), result.end());
            if (max_val > 0.0) {
                std::ranges::transform(result, result.begin(),
                    [max_val](double val) { return val / max_val; });
            }
        }

        working_buffer[i].resize(result.size());
        working_buffer[i] = std::move(result);
    }

    return OperationHelper::reconstruct_from_double<DataType>(working_buffer, structure_info);
}

/**
 * @brief Matched filter using cross-correlation for signal detection (IN-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - WILL BE MODIFIED
 * @param reference_signal Reference signal for matching
 * @return Matched filter output
 */
template <OperationReadyData DataType>
DataType transform_matched_filter(DataType& input, const std::vector<double>& reference_signal)
{
    return transform_cross_correlate(input, reference_signal, true);
}

/**
 * @brief Matched filter using cross-correlation for signal detection (OUT-OF-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - will NOT be modified
 * @param reference_signal Reference signal for matching
 * @param working_buffer Buffer for operations (will be resized if needed)
 * @return Matched filter output
 */
template <OperationReadyData DataType>
DataType transform_matched_filter(DataType& input, const std::vector<double>& reference_signal, std::vector<std::vector<double>>& working_buffer)
{
    return transform_cross_correlate(input, reference_signal, true, working_buffer);
}

/**
 * @brief Deconvolution using frequency domain division (IN-PLACE)
 * Useful for removing known impulse responses
 * @tparam DataType OperationReadyData type
 * @param input Input data - WILL BE MODIFIED
 * @param impulse_to_remove Impulse response to remove
 * @param regularization Regularization factor for numerical stability
 * @return Deconvolved data
 */
template <OperationReadyData DataType>
DataType transform_deconvolve(DataType& input, const std::vector<double>& impulse_to_remove, double regularization = 1e-6)
{
    auto [target_data, structure_info] = OperationHelper::extract_structured_double(input);

    auto deconvolution_op = [regularization](const Eigen::VectorXcd& input_fft,
                                const Eigen::VectorXcd& kernel_fft,
                                Eigen::VectorXcd& result_fft) {
        std::ranges::transform(input_fft, kernel_fft, result_fft.begin(),
            [regularization](const std::complex<double>& signal, const std::complex<double>& kernel) {
                double magnitude_sq = std::norm(kernel);
                if (magnitude_sq < regularization) {
                    return std::complex<double>(0.0, 0.0);
                }
                return signal * std::conj(kernel) / (magnitude_sq + regularization);
            });
    };

    for (auto& span : target_data) {
        auto result = fft_convolve_helper(span, impulse_to_remove, deconvolution_op);
        std::copy(result.begin(), result.begin() + std::min(result.size(), span.size()), span.begin());
    }

    auto reconstructed_data = target_data
        | std::views::transform([](const auto& span) {
              return std::vector<double>(span.begin(), span.end());
          })
        | std::ranges::to<std::vector>();

    return OperationHelper::reconstruct_from_double<DataType>(reconstructed_data, structure_info);
}

/**
 * @brief Deconvolution using frequency domain division (OUT-OF-PLACE)
 * Useful for removing known impulse responses
 * @tparam DataType OperationReadyData type
 * @param input Input data - will NOT be modified
 * @param impulse_to_remove Impulse response to remove
 * @param regularization Regularization factor for numerical stability
 * @param working_buffer Buffer for operations (will be resized if needed)
 * @return Deconvolved data
 */
template <OperationReadyData DataType>
DataType transform_deconvolve(DataType& input, const std::vector<double>& impulse_to_remove, double regularization, std::vector<std::vector<double>>& working_buffer)
{
    auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input, working_buffer);

    auto deconvolution_op = [regularization](const Eigen::VectorXcd& input_fft,
                                const Eigen::VectorXcd& kernel_fft,
                                Eigen::VectorXcd& result_fft) {
        std::ranges::transform(input_fft, kernel_fft, result_fft.begin(),
            [regularization](const std::complex<double>& signal, const std::complex<double>& kernel) {
                double magnitude_sq = std::norm(kernel);
                if (magnitude_sq < regularization) {
                    return std::complex<double>(0.0, 0.0);
                }
                return signal * std::conj(kernel) / (magnitude_sq + regularization);
            });
    };

    working_buffer.clear();
    working_buffer.resize(target_data.size());

    for (size_t i = 0; i < target_data.size(); ++i) {
        working_buffer[i] = fft_convolve_helper(target_data[i], impulse_to_remove, deconvolution_op);
    }

    return OperationHelper::reconstruct_from_double<DataType>(working_buffer, structure_info);
}

}
