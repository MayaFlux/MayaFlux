#pragma once

#include "MayaFlux/Nodes/Filters/FIR.hpp"
#include "MayaFlux/Yantra/OperationHelper.hpp"
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
    OperationFunc&& operation)
{
    size_t conv_size = data_span.size() + kernel.size() - 1;
    size_t fft_size = 1;
    while (fft_size < conv_size)
        fft_size *= 2;

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

    Eigen::VectorXd time_result;
    fft.inv(time_result, result_fft);

    std::vector<double> result(data_span.size());
    std::ranges::copy(time_result | std::views::take(data_span.size()), result.begin());

    return result;
}

/**
 * @brief Direct convolution using existing FIR filter infrastructure
 * This leverages the existing, well-tested filter implementation
 * @tparam DataType ComputeData type
 * @param input Input data
 * @param impulse_response Impulse response for convolution
 * @param in_place Whether to modify input or create copy
 * @return Convolved data
 */
template <ComputeData DataType>
DataType transform_convolve_with_fir(DataType& input,
    const std::vector<double>& impulse_response,
    bool in_place = false)
{
    auto [data_span, structure_info] = OperationHelper::extract_structured_double(input);

    std::vector<double> working_data;
    std::span<double> target_data;

    if (in_place) {
        target_data = data_span;
        working_data.assign(data_span.begin(), data_span.end());
    } else {
        working_data.assign(data_span.begin(), data_span.end());
        target_data = std::span(working_data);
    }

    auto fir_filter = std::make_shared<Nodes::Filters::FIR>(nullptr, impulse_response);

    std::vector<double> output;
    output.reserve(working_data.size());

    for (double sample : working_data) {
        output.push_back(fir_filter->process_sample(sample));
    }

    if (in_place) {
        size_t copy_size = std::min(output.size(), target_data.size());
        std::copy(output.begin(), output.begin() + copy_size, target_data.begin());
        return input;
    }

    return OperationHelper::convert_result_to_output_type<DataType>(output, structure_info);
}

/**
 * @brief Convolution transformation using existing infrastructure with C++20 ranges
 * @tparam DataType ComputeData type
 * @param input Input data
 * @param impulse_response Impulse response for convolution
 * @param in_place Whether to modify input or create copy
 * @return Convolved data
 */
template <ComputeData DataType>
DataType transform_convolve(DataType& input,
    const std::vector<double>& impulse_response,
    bool in_place = false)
{
    auto [data_span, structure_info] = OperationHelper::extract_structured_double(input);

    std::vector<double> working_data;
    std::span<double> target_data;

    if (in_place) {
        target_data = data_span;
        working_data.assign(data_span.begin(), data_span.end());
    } else {
        working_data.assign(data_span.begin(), data_span.end());
        target_data = std::span(working_data);
    }

    auto convolution_op = [](const Eigen::VectorXcd& input_fft,
                              const Eigen::VectorXcd& kernel_fft,
                              Eigen::VectorXcd& result_fft) {
        std::ranges::transform(input_fft, kernel_fft, result_fft.begin(),
            [](const std::complex<double>& a, const std::complex<double>& b) {
                return a * b;
            });
    };

    auto result = fft_convolve_helper(working_data, impulse_response, convolution_op);

    if (in_place) {
        std::copy(result.begin(), result.end(), target_data.begin());
        return input;
    } else {
        return OperationHelper::convert_result_to_output_type<DataType>(result, structure_info);
    }
}

/**
 * @brief Cross-correlation using FFT (convolution with time-reversed impulse)
 * @tparam DataType ComputeData type
 * @param input Input data
 * @param template_signal Template signal for correlation
 * @param normalize Whether to normalize the result
 * @param in_place Whether to modify input or create copy
 * @return Cross-correlated data
 */
template <ComputeData DataType>
DataType transform_cross_correlate(DataType& input,
    const std::vector<double>& template_signal,
    bool normalize = true,
    bool in_place = false)
{
    auto [data_span, structure_info] = OperationHelper::extract_structured_double(input);

    std::vector<double> working_data;
    std::span<double> target_data;

    if (in_place) {
        target_data = data_span;
        working_data.assign(data_span.begin(), data_span.end());
    } else {
        working_data.assign(data_span.begin(), data_span.end());
        target_data = std::span(working_data);
    }

    std::vector<double> reversed_template(template_signal.rbegin(), template_signal.rend());

    auto correlation_op = [](const Eigen::VectorXcd& input_fft,
                              const Eigen::VectorXcd& kernel_fft,
                              Eigen::VectorXcd& result_fft) {
        std::ranges::transform(input_fft, kernel_fft, result_fft.begin(),
            [](const std::complex<double>& a, const std::complex<double>& b) {
                return a * std::conj(b);
            });
    };

    auto result = fft_convolve_helper(working_data, reversed_template, correlation_op);

    if (normalize && !result.empty()) {
        auto [min_it, max_it] = std::ranges::minmax_element(result);
        double max_abs = std::max(std::abs(*min_it), std::abs(*max_it));
        if (max_abs > 0.0) {
            std::ranges::transform(result, result.begin(),
                [max_abs](double val) { return val / max_abs; });
        }
    }

    if (in_place) {
        std::copy(result.begin(), result.end(), target_data.begin());
        return input;
    }

    return OperationHelper::convert_result_to_output_type<DataType>(result, structure_info);
}

/**
 * @brief Matched filter using cross-correlation for signal detection
 * @tparam DataType ComputeData type
 * @param input Input data
 * @param reference_signal Reference signal for matching
 * @param in_place Whether to modify input or create copy
 * @return Matched filter output
 */
template <ComputeData DataType>
DataType transform_matched_filter(DataType& input,
    const std::vector<double>& reference_signal,
    bool in_place = false)
{
    return transform_cross_correlate(input, reference_signal, true, in_place);
}

/**
 * @brief Deconvolution using frequency domain division (experimental)
 * Useful for removing known impulse responses
 * @tparam DataType ComputeData type
 * @param input Input data
 * @param impulse_to_remove Impulse response to remove
 * @param regularization Regularization factor for numerical stability
 * @param in_place Whether to modify input or create copy
 * @return Deconvolved data
 */
template <ComputeData DataType>
DataType transform_deconvolve(DataType& input,
    const std::vector<double>& impulse_to_remove,
    double regularization = 1e-6,
    bool in_place = false)
{
    auto [data_span, structure_info] = OperationHelper::extract_structured_double(input);

    std::vector<double> working_data;
    std::span<double> target_data;

    if (in_place) {
        target_data = data_span;
        working_data.assign(data_span.begin(), data_span.end());
    } else {
        working_data.assign(data_span.begin(), data_span.end());
        target_data = std::span(working_data);
    }

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

    auto result = fft_convolve_helper(working_data, impulse_to_remove, deconvolution_op);

    if (in_place) {
        std::copy(result.begin(), result.end(), target_data.begin());
        return input;
    }
    return OperationHelper::convert_result_to_output_type<DataType>(result, structure_info);
}

}
