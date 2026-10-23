#pragma once

#include <glm/glm.hpp>

namespace MayaFlux::Kakshya {

/**
 * @enum GpuDataFormat
 * @brief GPU data formats with explicit precision levels
 */
enum class GpuDataFormat : uint8_t {
    FLOAT32, ///< 32-bit float (standard GPU)
    VEC2_F32, ///< glm::vec2 (32-bit components)
    VEC3_F32, ///< glm::vec3 (32-bit components), not a sampled image format
    VEC4_F32, ///< glm::vec4 (32-bit components)

    FLOAT64, ///< 64-bit double (audio precision)
    VEC2_F64, ///< glm::dvec2 (64-bit components)
    VEC3_F64, ///< glm::dvec3 (64-bit components)
    VEC4_F64, ///< glm::dvec4 (64-bit components)

    INT32,
    UINT32,

    UINT8, ///< uint8_t, R8 / RGBA8 texel data
    UINT16, ///< uint16_t, R16F raw half-float storage, packed formats

    MAT4_F32, ///< glm::mat4 (column-major), not a sampled image format
};

/**
 * @brief Byte size of one element of a GpuDataFormat.
 *
 * Single authoritative source for format sizing. Used by TextureAccess,
 * ShaderSpec, and any other system that needs to derive byte counts from
 * GpuDataFormat without duplicating the switch.
 *
 * @param fmt GpuDataFormat value.
 * @return Byte size of one element, or 0 for unrecognised formats.
 */
MAYAFLUX_API size_t gpu_data_format_bytes(GpuDataFormat fmt) noexcept;

/**
 * @brief Base alignment in bytes of one element, following std430 rules.
 *
 * Differs from gpu_data_format_bytes for three-component vectors, which
 * align as four.
 *
 * @param fmt GpuDataFormat value.
 * @return Alignment in bytes, or 0 for unrecognised formats.
 */
MAYAFLUX_API size_t gpu_data_format_align(GpuDataFormat fmt) noexcept;

/**
 * @brief Number of scalar components in one element of a GpuDataFormat.
 *
 * @param fmt GpuDataFormat value.
 * @return 1 for scalars, 2 to 4 for vectors, 16 for MAT4_F32, 0 for unrecognised formats.
 */
MAYAFLUX_API size_t gpu_data_format_components(GpuDataFormat fmt) noexcept;

/**
 * @brief Maps a C++ type to its GpuDataFormat.
 *
 * Specialised for float, double, int32_t, uint32_t, uint8_t, uint16_t,
 * glm::vec2/3/4, glm::dvec2/3/4 and glm::mat4. Unspecialised types have no
 * `value` member.
 */
template <typename T>
struct gpu_data_format_of { };

template <>
struct gpu_data_format_of<float> {
    static constexpr GpuDataFormat value = GpuDataFormat::FLOAT32;
};

template <>
struct gpu_data_format_of<glm::vec2> {
    static constexpr GpuDataFormat value = GpuDataFormat::VEC2_F32;
};

template <>
struct gpu_data_format_of<glm::vec3> {
    static constexpr GpuDataFormat value = GpuDataFormat::VEC3_F32;
};

template <>
struct gpu_data_format_of<glm::vec4> {
    static constexpr GpuDataFormat value = GpuDataFormat::VEC4_F32;
};

template <>
struct gpu_data_format_of<double> {
    static constexpr GpuDataFormat value = GpuDataFormat::FLOAT64;
};

template <>
struct gpu_data_format_of<glm::dvec2> {
    static constexpr GpuDataFormat value = GpuDataFormat::VEC2_F64;
};

template <>
struct gpu_data_format_of<glm::dvec3> {
    static constexpr GpuDataFormat value = GpuDataFormat::VEC3_F64;
};

template <>
struct gpu_data_format_of<glm::dvec4> {
    static constexpr GpuDataFormat value = GpuDataFormat::VEC4_F64;
};

template <>
struct gpu_data_format_of<int32_t> {
    static constexpr GpuDataFormat value = GpuDataFormat::INT32;
};

template <>
struct gpu_data_format_of<uint32_t> {
    static constexpr GpuDataFormat value = GpuDataFormat::UINT32;
};

template <>
struct gpu_data_format_of<uint8_t> {
    static constexpr GpuDataFormat value = GpuDataFormat::UINT8;
};

template <>
struct gpu_data_format_of<uint16_t> {
    static constexpr GpuDataFormat value = GpuDataFormat::UINT16;
};

template <>
struct gpu_data_format_of<glm::mat4> {
    static constexpr GpuDataFormat value = GpuDataFormat::MAT4_F32;
};

/**
 * @concept GpuFormatElement
 * @brief A type with a GpuDataFormat mapping.
 *
 * Wider than DataVariantElement: includes int32_t and the double precision
 * glm types, which DataVariant cannot hold.
 */
template <typename T>
concept GpuFormatElement = requires {
    { gpu_data_format_of<std::remove_cvref_t<T>>::value } -> std::convertible_to<GpuDataFormat>;
};

/**
 * @brief The GpuDataFormat of a GpuFormatElement type.
 */
template <GpuFormatElement T>
inline constexpr GpuDataFormat gpu_data_format_v = gpu_data_format_of<std::remove_cvref_t<T>>::value;

/**
 * @brief Writes scalar components into out as one element of dst.
 *
 * Each component is converted to the component type of dst. Integer
 * destinations saturate and map NaN to zero.
 *
 * @param dst Destination format.
 * @param components Source components, in element order (column-major for MAT4_F32).
 * @param out Destination bytes, at least gpu_data_format_bytes(dst).
 * @return False, with out untouched, if the component count does not equal
 *         gpu_data_format_components(dst) or out is too small.
 */
MAYAFLUX_API bool pack_gpu_components(
    GpuDataFormat dst,
    std::span<const double> components,
    std::span<std::byte> out) noexcept;

/**
 * @brief Writes a typed value into out as one element of dst.
 *
 * The value's component count must equal that of dst, so a glm::vec3 packs
 * into VEC3_F32 or VEC3_F64 but not into FLOAT32.
 *
 * @param dst Destination format.
 * @param value Source value.
 * @param out Destination bytes, at least gpu_data_format_bytes(dst).
 * @return False if the component counts differ or out is too small.
 */
template <GpuFormatElement T>
[[nodiscard]] bool pack_gpu_value(
    GpuDataFormat dst,
    const T& value,
    std::span<std::byte> out) noexcept
{
    using V = std::remove_cvref_t<T>;

    std::array<double, 16> components {};
    size_t count = 0;

    if constexpr (ArithmeticData<V>) {
        components[count++] = static_cast<double>(value);
    } else if constexpr (GlmMatrixType<V>) {
        for (int c = 0; c < 4; ++c) {
            for (int r = 0; r < 4; ++r) {
                components[count++] = static_cast<double>(value[c][r]);
            }
        }
    } else {
        for (int i = 0; i < static_cast<int>(glm_component_count<V>()); ++i) {
            components[count++] = static_cast<double>(value[i]);
        }
    }

    return pack_gpu_components(dst, std::span<const double>(components.data(), count), out);
}

} // namespace MayaFlux::Kakshya
