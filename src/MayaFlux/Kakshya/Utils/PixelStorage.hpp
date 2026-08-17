#pragma once

#include "MayaFlux/Kakshya/NDData/NDData.hpp"
#include "MayaFlux/Portal/Graphics/GraphicsUtils.hpp"

namespace MayaFlux::Kakshya {

/**
 * @brief Byte width of the DataVariant element backing a given ImageFormat.
 *
 * Returns 1 for uint8 storage, 2 for uint16, 4 for float. Defaults to 1
 * for any format without an explicit mapping.
 *
 * Size determines the alternative unambiguously only because DataVariant's
 * pixel alternatives are uint8, uint16, and float: 4 means float, not
 * uint32. Half-float formats return 2, since the uint16 bits are the
 * IEEE-754 binary16 encoding rather than a UNORM value; consumers needing
 * the decoded magnitude must check is_float_format().
 *
 * @param format Pixel format.
 * @return Element size in bytes.
 */
[[nodiscard]] MAYAFLUX_API size_t storage_element_size(Portal::Graphics::ImageFormat format);

/**
 * @brief True when the format's numeric interpretation is floating point.
 *
 * Distinguishes R16F from R16: both occupy U16 storage, but only the
 * former holds half-float bits rather than a normalised integer.
 *
 * @param format Pixel format.
 */
[[nodiscard]] MAYAFLUX_API bool is_float_format(Portal::Graphics::ImageFormat format);

/**
 * @brief Allocate a zeroed DataVariant of the alternative backing a format.
 * @param format        Pixel format determining the element type.
 * @param element_count Number of elements, not bytes. Typically
 *                      width * height * channel_count.
 * @return Zero-filled variant of the matching alternative.
 */
[[nodiscard]] MAYAFLUX_API DataVariant make_empty_storage(
    Portal::Graphics::ImageFormat format, size_t element_count);

/**
 * @brief Read one element as a normalised double.
 *
 * uint8 divided by 255. uint16 divided by 65535 unless the format is
 * half-float, in which case the raw bits are returned uninterpreted.
 * float returned unchanged.
 *
 * @param v          Source variant.
 * @param format     Pixel format governing the uint16 interpretation.
 * @param elem_index Element index. Returns 0.0 when out of range.
 */
[[nodiscard]] MAYAFLUX_API double read_normalized_at(
    const DataVariant& v, Portal::Graphics::ImageFormat format, size_t elem_index);

/**
 * @brief Read one element as a normalised double, with optional range remapping.
 *
 * Same as read_normalized_at, but if @p range is provided, the value is
 * linearly remapped from [range.min, range.max] to [0.0, 1.0]. Values
 * outside the range are clamped to 0.0 or 1.0.
 *
 * @param v          Source variant.
 * @param format     Pixel format governing the uint16 interpretation.
 * @param range      Optional value range for remapping.
 * @param elem_index Element index. Returns 0.0 when out of range.
 */
[[nodiscard]]
MAYAFLUX_API double read_normalized_at(const DataVariant& v,
    Portal::Graphics::ImageFormat format,
    const std::optional<DataDimension::ValueRange>& range,
    size_t elem_index);

/**
 * @brief Write one element from a normalised double.
 *
 * Inverse of read_normalized_at, with clamping on the integer paths.
 * Out-of-range indices are ignored.
 *
 * @param v          Destination variant.
 * @param format     Pixel format governing the uint16 interpretation.
 * @param elem_index Element index.
 * @param value      Normalised value.
 */
MAYAFLUX_API void write_normalized_at(
    DataVariant& v, Portal::Graphics::ImageFormat format, size_t elem_index, double value);

/**
 * @brief Write one element from a normalised double, with optional range remapping.
 *
 * Inverse of read_normalized_at, with clamping on the integer paths.
 * If @p range is provided, the value is linearly remapped from [0.0, 1.0]
 * to [range.min, range.max] before writing. Out-of-range indices are ignored.
 *
 * @param v          Destination variant.
 * @param format     Pixel format governing the uint16 interpretation.
 * @param range      Optional value range for remapping.
 * @param elem_index Element index.
 * @param value      Normalised value.
 */
MAYAFLUX_API void write_normalized_at(DataVariant& v,
    Portal::Graphics::ImageFormat format,
    const std::optional<DataDimension::ValueRange>& range,
    size_t elem_index,
    double value);

} // namespace MayaFlux::Kakshya
