#pragma once

#include "MayaFlux/Nodes/Generators/Polynomial.hpp"
#include "MayaFlux/Yantra/OperationSpec/OperationHelper.hpp"

/**
 * @file MathematicalTransforms.hpp
 * @brief Mathematical transformation functions leveraging existing ecosystem
 *
 * Provides mathematical transformation functions that can be used by any class.
 * Leverages existing infrastructure:
 * - Uses OperationHelper for data conversion/reconstruction
 * - Uses existing Generator::Polynomial for polynomial operations
 * - Uses StatisticalAnalyzer for normalization statistics
 * - Follows DataUtils in-place vs copy patterns
 * - Uses C++20 ranges/views for efficient processing
 *
 * Philosophy: **Function-based helpers** that compose existing capabilities.
 */

namespace MayaFlux::Yantra {

/**
 * @brief Linear transformation y = ax + b using C++20 ranges (IN-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - WILL BE MODIFIED
 * @param a Scale factor
 * @param b Offset factor
 * @return Transformed data
 */
template <OperationReadyData DataType>
DataType transform_linear(DataType& input, double a, double b)
{
    auto [target_data, structure_info] = OperationHelper::extract_structured_double(input);

    for (auto& span : target_data) {
        std::ranges::transform(span, span.begin(),
            [a, b](double x) { return a * x + b; });
    }

    auto reconstructed_data = target_data
        | std::views::transform([](const auto& span) {
              return std::vector<double>(span.begin(), span.end());
          })
        | std::ranges::to<std::vector>();

    return OperationHelper::reconstruct_from_double<DataType>(reconstructed_data, structure_info);
}

/**
 * @brief Linear transformation y = ax + b using C++20 ranges (OUT-OF-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - will NOT be modified
 * @param a Scale factor
 * @param b Offset factor
 * @param working_buffer Buffer for operations (will be resized if needed)
 * @return Transformed data
 */
template <OperationReadyData DataType>
DataType transform_linear(DataType& input, double a, double b, std::vector<std::vector<double>>& working_buffer)
{
    auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input, working_buffer);

    for (auto& span : target_data) {
        std::ranges::transform(span, span.begin(),
            [a, b](double x) { return a * x + b; });
    }

    return OperationHelper::reconstruct_from_double<DataType>(working_buffer, structure_info);
}

/**
 * @brief Power transformation y = x^exponent (IN-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - WILL BE MODIFIED
 * @param exponent Power exponent
 * @return Transformed data
 */
template <OperationReadyData DataType>
DataType transform_power(DataType& input, double exponent)
{
    auto [target_data, structure_info] = OperationHelper::extract_structured_double(input);

    for (auto& span : target_data) {
        std::ranges::transform(span, span.begin(),
            [exponent](double x) { return std::pow(x, exponent); });
    }

    auto reconstructed_data = target_data
        | std::views::transform([](const auto& span) {
              return std::vector<double>(span.begin(), span.end());
          })
        | std::ranges::to<std::vector>();

    return OperationHelper::reconstruct_from_double<DataType>(reconstructed_data, structure_info);
}

/**
 * @brief Power transformation y = x^exponent (OUT-OF-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - will NOT be modified
 * @param exponent Power exponent
 * @param working_buffer Buffer for operations (will be resized if needed)
 * @return Transformed data
 */
template <OperationReadyData DataType>
DataType transform_power(DataType& input, double exponent, std::vector<std::vector<double>>& working_buffer)
{
    auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input, working_buffer);

    for (auto& span : target_data) {
        std::ranges::transform(span, span.begin(),
            [exponent](double x) { return std::pow(x, exponent); });
    }

    return OperationHelper::reconstruct_from_double<DataType>(working_buffer, structure_info);
}

/**
 * @brief Polynomial transformation using existing Generator::Polynomial (IN-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - WILL BE MODIFIED
 * @param coefficients Polynomial coefficients [a0, a1, a2, ...]
 * @return Transformed data
 */
template <OperationReadyData DataType>
DataType transform_polynomial(DataType& input, const std::vector<double>& coefficients)
{
    auto [target_data, structure_info] = OperationHelper::extract_structured_double(input);

    auto polynomial_gen = std::make_shared<Nodes::Generator::Polynomial>(coefficients);

    for (auto& span : target_data) {
        std::ranges::transform(span, span.begin(),
            [&polynomial_gen](double x) {
                return polynomial_gen->process_sample(x);
            });
    }

    auto reconstructed_data = target_data
        | std::views::transform([](const auto& span) {
              return std::vector<double>(span.begin(), span.end());
          })
        | std::ranges::to<std::vector>();

    return OperationHelper::reconstruct_from_double<DataType>(reconstructed_data, structure_info);
}

/**
 * @brief Polynomial transformation using existing Generator::Polynomial (OUT-OF-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - will NOT be modified
 * @param coefficients Polynomial coefficients [a0, a1, a2, ...]
 * @param working_buffer Buffer for operations (will be resized if needed)
 * @return Transformed data
 */
template <OperationReadyData DataType>
DataType transform_polynomial(DataType& input, const std::vector<double>& coefficients, std::vector<std::vector<double>>& working_buffer)
{
    auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input, working_buffer);

    auto polynomial_gen = std::make_shared<Nodes::Generator::Polynomial>(coefficients);

    for (auto& span : target_data) {
        std::ranges::transform(span, span.begin(),
            [&polynomial_gen](double x) {
                return polynomial_gen->process_sample(x);
            });
    }

    return OperationHelper::reconstruct_from_double<DataType>(working_buffer, structure_info);
}

/**
 * @brief Exponential transformation y = a * base^(b * x) (IN-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - WILL BE MODIFIED
 * @param a Scale factor
 * @param b Exponential rate
 * @param base Exponential base (default: e for natural exponential)
 * @return Transformed data
 */
template <OperationReadyData DataType>
DataType transform_exponential(DataType& input, double a, double b, double base = std::numbers::e)
{
    auto [target_data, structure_info] = OperationHelper::extract_structured_double(input);

    for (auto& span : target_data) {
        std::ranges::transform(span, span.begin(),
            [a, b, base](double x) {
                if (base == std::numbers::e) {
                    return a * std::exp(b * x);
                }
                return a * std::pow(base, b * x);
            });
    }

    auto reconstructed_data = target_data
        | std::views::transform([](const auto& span) {
              return std::vector<double>(span.begin(), span.end());
          })
        | std::ranges::to<std::vector>();

    return OperationHelper::reconstruct_from_double<DataType>(reconstructed_data, structure_info);
}

/**
 * @brief Exponential transformation y = a * base^(b * x) (OUT-OF-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - will NOT be modified
 * @param a Scale factor
 * @param b Exponential rate
 * @param working_buffer Buffer for operations (will be resized if needed)
 * @param b Exponential rate
 * @return Transformed data
 */
template <OperationReadyData DataType>
DataType transform_exponential(DataType& input, double a, double b, std::vector<std::vector<double>>& working_buffer, double base = std::numbers::e)
{
    auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input, working_buffer);

    for (auto& span : target_data) {
        std::ranges::transform(span, span.begin(),
            [a, b, base](double x) {
                if (base == std::numbers::e) {
                    return a * std::exp(b * x);
                }
                return a * std::pow(base, b * x);
            });
    }

    return OperationHelper::reconstruct_from_double<DataType>(working_buffer, structure_info);
}

/**
 * @brief Logarithmic transformation y = a * log_base(b * x + c) (IN-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - WILL BE MODIFIED
 * @param a Scale factor
 * @param b Input scale factor
 * @param c Offset to ensure positive argument
 * @param base Logarithm base (default: e for natural log)
 * @return Transformed data
 */
template <OperationReadyData DataType>
DataType transform_logarithmic(DataType& input, double a, double b, double c = 1.0, double base = std::numbers::e)
{
    auto [target_data, structure_info] = OperationHelper::extract_structured_double(input);

    double log_base_factor = (base == std::numbers::e) ? 1.0 : (1.0 / std::log(base));

    for (auto& span : target_data) {
        std::ranges::transform(span, span.begin(),
            [a, b, c, log_base_factor](double x) {
                double arg = b * x + c;
                if (arg <= 0.0)
                    return 0.0;
                return a * std::log(arg) * log_base_factor;
            });
    }

    auto reconstructed_data = target_data
        | std::views::transform([](const auto& span) {
              return std::vector<double>(span.begin(), span.end());
          })
        | std::ranges::to<std::vector>();

    return OperationHelper::reconstruct_from_double<DataType>(reconstructed_data, structure_info);
}

/**
 * @brief Logarithmic transformation y = a * log_base(b * x + c) (OUT-OF-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - will NOT be modified
 * @param a Scale factor
 * @param b Input scale factor
 * @param c Offset to ensure positive argument
 * @param working_buffer Buffer for operations (will be resized if needed)
 * @return Transformed data
 */
template <OperationReadyData DataType>
DataType transform_logarithmic(DataType& input, double a, double b, double c, std::vector<std::vector<double>>& working_buffer, double base = std::numbers::e)
{
    auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input, working_buffer);

    double log_base_factor = (base == std::numbers::e) ? 1.0 : (1.0 / std::log(base));

    for (auto& span : target_data) {
        std::ranges::transform(span, span.begin(),
            [a, b, c, log_base_factor](double x) {
                double arg = b * x + c;
                if (arg <= 0.0)
                    return 0.0;
                return a * std::log(arg) * log_base_factor;
            });
    }

    return OperationHelper::reconstruct_from_double<DataType>(working_buffer, structure_info);
}

/**
 * @brief Trigonometric transformation using specified function (IN-PLACE)
 * @tparam DataType OperationReadyData type
 * @tparam TrigFunc Trigonometric function type
 * @param input Input data - WILL BE MODIFIED
 * @param trig_func Trigonometric function (sin, cos, tan, etc.)
 * @param frequency Frequency scaling factor
 * @param amplitude Amplitude scaling factor
 * @param phase Phase offset
 * @return Transformed data
 */
template <OperationReadyData DataType, typename TrigFunc>
    requires std::invocable<TrigFunc, double>
DataType transform_trigonometric(DataType& input,
    TrigFunc trig_func,
    double frequency = 1.0,
    double amplitude = 1.0,
    double phase = 0.0)
{
    auto [target_data, structure_info] = OperationHelper::extract_structured_double(input);

    for (auto& span : target_data) {
        std::ranges::transform(span, span.begin(),
            [trig_func, frequency, amplitude, phase](double x) {
                return amplitude * trig_func(frequency * x + phase);
            });
    }

    auto reconstructed_data = target_data
        | std::views::transform([](const auto& span) {
              return std::vector<double>(span.begin(), span.end());
          })
        | std::ranges::to<std::vector>();

    return OperationHelper::reconstruct_from_double<DataType>(reconstructed_data, structure_info);
}

/**
 * @brief Trigonometric transformation using specified function (OUT-OF-PLACE)
 * @tparam DataType OperationReadyData type
 * @tparam TrigFunc Trigonometric function type
 * @param input Input data - will NOT be modified
 * @param trig_func Trigonometric function (sin, cos, tan, etc.)
 * @param frequency Frequency scaling factor
 * @param amplitude Amplitude scaling factor
 * @param phase Phase offset
 * @param working_buffer Buffer for operations (will be resized if needed)
 * @return Transformed data
 */
template <OperationReadyData DataType, typename TrigFunc>
    requires std::invocable<TrigFunc, double>
DataType transform_trigonometric(DataType& input,
    TrigFunc trig_func,
    double frequency,
    double amplitude,
    double phase,
    std::vector<std::vector<double>>& working_buffer)
{
    auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input, working_buffer);

    for (auto& span : target_data) {
        std::ranges::transform(span, span.begin(),
            [trig_func, frequency, amplitude, phase](double x) {
                return amplitude * trig_func(frequency * x + phase);
            });
    }

    return OperationHelper::reconstruct_from_double<DataType>(working_buffer, structure_info);
}

/**
 * @brief Quantization transformation (bit reduction) using C++20 features (IN-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - WILL BE MODIFIED
 * @param bits Number of bits for quantization
 * @return Quantized data
 */
template <OperationReadyData DataType>
DataType transform_quantize(DataType& input, u_int8_t bits)
{
    auto [target_data, structure_info] = OperationHelper::extract_structured_double(input);

    const double levels = std::pow(2.0, bits) - 1.0;

    for (auto& span : target_data) {
        std::ranges::transform(span, span.begin(),
            [levels](double x) {
                double clamped = std::clamp(x, -1.0, 1.0);
                return std::round(clamped * levels) / levels;
            });
    }

    auto reconstructed_data = target_data
        | std::views::transform([](const auto& span) {
              return std::vector<double>(span.begin(), span.end());
          })
        | std::ranges::to<std::vector>();

    return OperationHelper::reconstruct_from_double<DataType>(reconstructed_data, structure_info);
}

/**
 * @brief Quantization transformation (bit reduction) using C++20 features (OUT-OF-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - will NOT be modified
 * @param bits Number of bits for quantization
 * @param working_buffer Buffer for operations (will be resized if needed)
 * @return Quantized data
 */
template <OperationReadyData DataType>
DataType transform_quantize(DataType& input, u_int8_t bits, std::vector<std::vector<double>>& working_buffer)
{
    auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input, working_buffer);

    const double levels = std::pow(2.0, bits) - 1.0;

    for (auto& span : target_data) {
        std::ranges::transform(span, span.begin(),
            [levels](double x) {
                double clamped = std::clamp(x, -1.0, 1.0);
                return std::round(clamped * levels) / levels;
            });
    }

    return OperationHelper::reconstruct_from_double<DataType>(working_buffer, structure_info);
}

/**
 * @brief Clamp transformation using C++20 ranges (IN-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - WILL BE MODIFIED
 * @param min_val Minimum value
 * @param max_val Maximum value
 * @return Clamped data
 */
template <OperationReadyData DataType>
DataType transform_clamp(DataType& input, double min_val, double max_val)
{
    auto [target_data, structure_info] = OperationHelper::extract_structured_double(input);

    for (auto& span : target_data) {
        std::ranges::transform(span, span.begin(),
            [min_val, max_val](double x) { return std::clamp(x, min_val, max_val); });
    }

    auto reconstructed_data = target_data
        | std::views::transform([](const auto& span) {
              return std::vector<double>(span.begin(), span.end());
          })
        | std::ranges::to<std::vector>();

    return OperationHelper::reconstruct_from_double<DataType>(reconstructed_data, structure_info);
}

/**
 * @brief Clamp transformation using C++20 ranges (OUT-OF-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - will NOT be modified
 * @param min_val Minimum value
 * @param max_val Maximum value
 * @param working_buffer Buffer for operations (will be resized if needed)
 * @return Clamped data
 */
template <OperationReadyData DataType>
DataType transform_clamp(DataType& input, double min_val, double max_val, std::vector<std::vector<double>>& working_buffer)
{
    auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input, working_buffer);

    for (auto& span : target_data) {
        std::ranges::transform(span, span.begin(),
            [min_val, max_val](double x) { return std::clamp(x, min_val, max_val); });
    }

    return OperationHelper::reconstruct_from_double<DataType>(working_buffer, structure_info);
}

/**
 * @brief Wrap transformation (modulo operation) using C++20 ranges (IN-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - WILL BE MODIFIED
 * @param wrap_range Wrap range [0, wrap_range)
 * @return Wrapped data
 */
template <OperationReadyData DataType>
DataType transform_wrap(DataType& input, double wrap_range)
{
    auto [target_data, structure_info] = OperationHelper::extract_structured_double(input);

    for (auto& span : target_data) {
        std::ranges::transform(span, span.begin(),
            [wrap_range](double x) {
                return x - wrap_range * std::floor(x / wrap_range);
            });
    }

    auto reconstructed_data = target_data
        | std::views::transform([](const auto& span) {
              return std::vector<double>(span.begin(), span.end());
          })
        | std::ranges::to<std::vector>();

    return OperationHelper::reconstruct_from_double<DataType>(reconstructed_data, structure_info);
}

/**
 * @brief Wrap transformation (modulo operation) using C++20 ranges (OUT-OF-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - will NOT be modified
 * @param wrap_range Wrap range [0, wrap_range)
 * @param working_buffer Buffer for operations (will be resized if needed)
 * @return Wrapped data
 */
template <OperationReadyData DataType>
DataType transform_wrap(DataType& input, double wrap_range, std::vector<std::vector<double>>& working_buffer)
{
    auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input, working_buffer);

    for (auto& span : target_data) {
        std::ranges::transform(span, span.begin(),
            [wrap_range](double x) {
                return x - wrap_range * std::floor(x / wrap_range);
            });
    }

    return OperationHelper::reconstruct_from_double<DataType>(working_buffer, structure_info);
}

/**
 * @brief Normalize transformation using existing infrastructure (IN-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - WILL BE MODIFIED
 * @param target_range Target range [min, max]
 * @return Normalized data
 */
template <OperationReadyData DataType>
DataType transform_normalize(DataType& input, const std::pair<double, double>& target_range = { -1.0, 1.0 })
{
    auto [target_data, structure_info] = OperationHelper::extract_structured_double(input);

    for (auto& span : target_data) {
        auto [min_it, max_it] = std::ranges::minmax_element(span);
        double current_min = *min_it;
        double current_max = *max_it;

        if (current_max == current_min)
            return input;

        double current_range = current_max - current_min;
        double target_min = target_range.first;
        double target_max = target_range.second;
        double target_span = target_max - target_min;

        std::ranges::transform(span, span.begin(),
            [current_min, current_range, target_span, target_min](double x) {
                return ((x - current_min) / current_range) * target_span + target_min;
            });
    }

    auto reconstructed_data = target_data
        | std::views::transform([](const auto& span) {
              return std::vector<double>(span.begin(), span.end());
          })
        | std::ranges::to<std::vector>();

    return OperationHelper::reconstruct_from_double<DataType>(reconstructed_data, structure_info);
}

/**
 * @brief Normalize transformation using existing infrastructure (OUT-OF-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - will NOT be modified
 * @param target_range Target range [min, max]
 * @param working_buffer Buffer for operations (will be resized if needed)
 * @return Normalized data
 */
template <OperationReadyData DataType>
DataType transform_normalize(DataType& input, const std::pair<double, double>& target_range, std::vector<std::vector<double>>& working_buffer)
{
    auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input, working_buffer);

    for (auto& span : target_data) {
        auto [min_it, max_it] = std::ranges::minmax_element(span);
        double current_min = *min_it;
        double current_max = *max_it;

        if (current_max == current_min)
            return input;

        double current_range = current_max - current_min;
        double target_min = target_range.first;
        double target_max = target_range.second;
        double target_span = target_max - target_min;

        std::ranges::transform(span, span.begin(),
            [current_min, current_range, target_span, target_min](double x) {
                return ((x - current_min) / current_range) * target_span + target_min;
            });
    }

    return OperationHelper::reconstruct_from_double<DataType>(working_buffer, structure_info);
}

inline void interpolate(std::span<double> input, std::vector<double>& output, u_int32_t target_size)
{
    auto indices = std::views::iota(size_t { 0 }, target_size);

    std::ranges::transform(indices, output.begin(),
        [&input, target_size](size_t i) {
            double pos = static_cast<double>(i) * double(input.size() - 1) / (target_size - 1);
            auto idx = static_cast<size_t>(pos);
            double frac = pos - (double)idx;

            if (idx + 1 < input.size()) {
                return input[idx] * (1.0 - frac) + input[idx + 1] * frac;
            }

            return input[idx];
        });
}

/**
 * @brief Linear interpolation between data points using C++20 ranges (IN-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - WILL BE MODIFIED/RESIZED
 * @param target_size Target size after interpolation
 * @return Interpolated data
 */
template <OperationReadyData DataType>
DataType interpolate_linear(DataType& input, size_t target_size)
{
    auto [data_span, structure_info] = OperationHelper::extract_structured_double(input);

    std::vector<std::vector<double>> interpolated;
    for (auto& span : data_span) {
        if (target_size != span.size()) {
            std::vector<double> sub_data(target_size);
            interpolate(span, std::ref(sub_data), target_size);
            interpolated.push_back(std::move(sub_data));
        } else {
            interpolated.emplace_back(span.begin(), span.end());
        }
    }

    input = OperationHelper::reconstruct_from_double<DataType>(interpolated, structure_info);
    return input;
}

/**
 * @brief Linear interpolation between data points using C++20 ranges (OUT-OF-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - will NOT be modified
 * @param target_size Target size after interpolation
 * @param working_buffer Buffer for operations (will be resized if needed)
 * @return Interpolated data
 */
template <OperationReadyData DataType>
DataType interpolate_linear(DataType& input, size_t target_size, std::vector<std::vector<double>>& working_buffer)
{
    auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input, working_buffer);

    for (size_t i = 0; i < target_data.size(); i++) {
        if (target_size != target_data[i].size()) {
            working_buffer[i].resize(target_size);
            interpolate(target_data[i], working_buffer[i], target_size);
        }
    }

    return OperationHelper::reconstruct_from_double<DataType>(working_buffer, structure_info);
}

/**
 * @brief Cubic interpolation between data points using C++20 ranges (IN-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - WILL BE MODIFIED/RESIZED
 * @param target_size Target size after interpolation
 * @return Interpolated data
 */
template <OperationReadyData DataType>
DataType interpolate_cubic(DataType& input, size_t target_size)
{
    auto [target_data, structure_info] = OperationHelper::extract_structured_double(input);

    bool needs_resize = false;
    for (const auto& span : target_data) {
        if (target_size != span.size()) {
            needs_resize = true;
            break;
        }
    }

    if (!needs_resize) {
        return input;
    }

    std::vector<std::vector<double>> result(target_data.size());

    for (size_t ch = 0; ch < target_data.size(); ++ch) {
        const auto& data_span = target_data[ch];
        auto& interpolated = result[ch];
        interpolated.resize(target_size);

        if (target_size <= 1 || data_span.size() <= 1) {
            std::fill(interpolated.begin(), interpolated.end(),
                data_span.empty() ? 0.0 : data_span[0]);
            continue;
        }

        auto indices = std::views::iota(size_t { 0 }, target_size);
        std::ranges::transform(indices, interpolated.begin(),
            [&data_span, target_size](size_t i) {
                double pos = static_cast<double>(i) * (data_span.size() - 1) / (target_size - 1);
                int idx = static_cast<int>(pos);
                double frac = pos - idx;

                auto get_sample = [&data_span](int i) {
                    return data_span[std::clamp(i, 0, static_cast<int>(data_span.size() - 1))];
                };

                double y0 = get_sample(idx - 1);
                double y1 = get_sample(idx);
                double y2 = get_sample(idx + 1);
                double y3 = get_sample(idx + 2);

                double a = -0.5 * y0 + 1.5 * y1 - 1.5 * y2 + 0.5 * y3;
                double b = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3;
                double c = -0.5 * y0 + 0.5 * y2;
                double d = y1;

                return ((a * frac + b) * frac + c) * frac + d;
            });
    }

    return OperationHelper::reconstruct_from_double<DataType>(result, structure_info);
}

/**
 * @brief Cubic interpolation between data points using C++20 ranges (OUT-OF-PLACE)
 * @tparam DataType OperationReadyData type
 * @param input Input data - will NOT be modified
 * @param target_size Target size after interpolation
 * @param working_buffer Buffer for operations (will be resized if needed)
 * @return Interpolated data
 */
template <OperationReadyData DataType>
DataType interpolate_cubic(DataType& input, size_t target_size, std::vector<std::vector<double>>& working_buffer)
{
    auto [target_data, structure_info] = OperationHelper::setup_operation_buffer(input, working_buffer);

    bool needs_resize = false;
    for (const auto& span : target_data) {
        if (target_size != span.size()) {
            needs_resize = true;
            break;
        }
    }

    if (!needs_resize) {
        return OperationHelper::reconstruct_from_double<DataType>(working_buffer, structure_info);
    }

    for (size_t ch = 0; ch < target_data.size(); ++ch) {
        const auto& data_span = target_data[ch];
        working_buffer[ch].resize(target_size);

        if (target_size <= 1 || data_span.size() <= 1) {
            std::fill(working_buffer[ch].begin(), working_buffer[ch].end(),
                data_span.empty() ? 0.0 : data_span[0]);
            continue;
        }

        auto indices = std::views::iota(size_t { 0 }, target_size);
        std::ranges::transform(indices, working_buffer[ch].begin(),
            [&data_span, target_size](size_t i) {
                double pos = static_cast<double>(i) * (data_span.size() - 1) / (target_size - 1);
                int idx = static_cast<int>(pos);
                double frac = pos - idx;

                auto get_sample = [&data_span](int i) {
                    return data_span[std::clamp(i, 0, static_cast<int>(data_span.size() - 1))];
                };

                double y0 = get_sample(idx - 1);
                double y1 = get_sample(idx);
                double y2 = get_sample(idx + 1);
                double y3 = get_sample(idx + 2);

                double a = -0.5 * y0 + 1.5 * y1 - 1.5 * y2 + 0.5 * y3;
                double b = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3;
                double c = -0.5 * y0 + 0.5 * y2;
                double d = y1;

                return ((a * frac + b) * frac + c) * frac + d;
            });
    }

    return OperationHelper::reconstruct_from_double<DataType>(working_buffer, structure_info);
}

} // namespace MayaFlux::Yantra
