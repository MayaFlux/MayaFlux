#pragma once

#include "Tensor.hpp"

#include <expected>

namespace MayaFlux::Kakshya {

/**
 * @brief Empty when a tensor can be converted to a type, otherwise the reason it cannot.
 *
 * Whole-byte integers (8 to 64 bits, signed and unsigned), bool, IEEE half,
 * bfloat16, float and double, and complex of float and double are supported.
 * Complex and real types do not convert to each other. Other types (float8,
 * float6, float4, sub-byte integers and multi-lane elements) are refused.
 *
 * Quantization is only held by integer storage of at most 32 bits. A source
 * with quantization is dequantized; @p quantization asks for the result to be
 * quantized and needs an integer target.
 *
 * @param source Layout of the tensor to convert.
 * @param target Element type of the result.
 * @param quantization Quantization the result should carry, if any.
 */
[[nodiscard]] MAYAFLUX_API std::string diagnose_conversion(
    const TensorSpec& source, TensorDType target,
    const std::optional<TensorQuantization>& quantization = std::nullopt);

/**
 * @brief Convert the elements of a tensor to another element type.
 *
 * The result is a dense C-order tensor with the source's name and axes. A
 * strided source is read through its strides.
 *
 * Integer to integer conversion is exact and saturates. A float becomes an
 * integer by rounding to nearest, ties to even, with NaN as zero and
 * saturation at the range. A float narrows by rounding to nearest, ties to
 * even, and overflows to infinity. Anything non-zero becomes true in a bool.
 *
 * Values are real numbers when a quantization is involved: a source with
 * quantization is read as scale * (stored - zero_point), and a requested
 * quantization stores clamp(round(value / scale) + zero_point), with NaN
 * stored as the zero point. A source quantization is not carried to the
 * result unless it is requested again.
 *
 * Usage:
 * @code
 * auto weights = convert_tensor(*bf16_weights.access(), TensorDType::of<float>());
 * auto packed = convert_tensor(*activations.access(), TensorDType::of<int8_t>(),
 *     TensorQuantization { .scale = { 0.05 }, .zero_point = { 3 }, .axis = std::nullopt });
 * @endcode
 *
 * @param source Tensor to read.
 * @param target Element type of the result.
 * @param quantization Quantization the result should carry, if any.
 * @return The converted tensor, or the reason it could not be converted.
 */
[[nodiscard]] MAYAFLUX_API std::expected<TensorData, std::string> convert_tensor(
    const TensorAccess& source, TensorDType target,
    const std::optional<TensorQuantization>& quantization = std::nullopt);

}
