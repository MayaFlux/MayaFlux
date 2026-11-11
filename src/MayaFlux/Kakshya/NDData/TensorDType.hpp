#pragma once

#include "GpuDataFormat.hpp"

namespace MayaFlux::Kakshya {

/**
 * @enum TensorDTypeKind
 * @brief Element family of a tensor. Values equal the DLPack type codes.
 */
enum class TensorDTypeKind : uint8_t {
    INT = 0,
    UINT = 1,
    FLOAT = 2,
    OPAQUE_HANDLE = 3,
    BFLOAT = 4,
    COMPLEX = 5,
    BOOL = 6,
    FLOAT8_E3M4 = 7,
    FLOAT8_E4M3 = 8,
    FLOAT8_E4M3B11FNUZ = 9,
    FLOAT8_E4M3FN = 10,
    FLOAT8_E4M3FNUZ = 11,
    FLOAT8_E5M2 = 12,
    FLOAT8_E5M2FNUZ = 13,
    FLOAT8_E8M0FNU = 14,
    FLOAT6_E2M3FN = 15,
    FLOAT6_E3M2FN = 16,
    FLOAT4_E2M1FN = 17,
    BCOMPLEX = 18
};

namespace detail {

    template <typename T>
    struct is_std_complex : std::false_type { };

    template <typename T>
    struct is_std_complex<std::complex<T>> : std::bool_constant<std::is_floating_point_v<T>> { };

}

/** @brief A C++ scalar or std::complex that maps to a TensorDType. */
template <typename T>
concept TensorElement = (std::is_arithmetic_v<T>
                            && (!std::is_floating_point_v<T> || sizeof(T) == 4 || sizeof(T) == 8))
    || detail::is_std_complex<T>::value;

/**
 * @struct TensorDType
 * @brief Element type as kind, bit width and lane count.
 *
 * Mirrors the DLPack data type, so half, bfloat16, float8, int4 and every
 * integer width are first class and nothing is widened. Sub-byte types are
 * packed; their storage is rounded up to whole bytes for the tensor.
 *
 * Usage:
 * @code
 * auto f = TensorDType::of<float>();
 * auto h = TensorDType::float16();
 * bool same = f.is<float>();
 * @endcode
 */
struct MAYAFLUX_API TensorDType {
    TensorDTypeKind kind { TensorDTypeKind::FLOAT };
    uint8_t bits { 32 };
    uint16_t lanes { 1 };

    constexpr bool operator==(const TensorDType&) const noexcept = default;

    /** @brief Type for a C++ scalar or complex element. */
    template <TensorElement T>
    [[nodiscard]] static constexpr TensorDType of() noexcept
    {
        constexpr auto width = static_cast<uint8_t>(sizeof(T) * 8);
        if constexpr (std::same_as<T, bool>) {
            return { .kind = TensorDTypeKind::BOOL, .bits = 8, .lanes = 1 };
        } else if constexpr (std::is_floating_point_v<T>) {
            return { .kind = TensorDTypeKind::FLOAT, .bits = width, .lanes = 1 };
        } else if constexpr (std::is_integral_v<T>) {
            return { .kind = std::is_signed_v<T> ? TensorDTypeKind::INT : TensorDTypeKind::UINT, .bits = width, .lanes = 1 };
        } else {
            return { .kind = TensorDTypeKind::COMPLEX, .bits = width, .lanes = 1 };
        }
    }

    /** @brief IEEE half precision. */
    [[nodiscard]] static constexpr TensorDType float16() noexcept { return { .kind = TensorDTypeKind::FLOAT, .bits = 16, .lanes = 1 }; }
    /** @brief Brain floating point, 16 bits. */
    [[nodiscard]] static constexpr TensorDType bfloat16() noexcept { return { .kind = TensorDTypeKind::BFLOAT, .bits = 16, .lanes = 1 }; }

    /** @brief Whether this type is exactly the C++ element type T. */
    template <TensorElement T>
    [[nodiscard]] constexpr bool is() const noexcept { return *this == of<T>(); }

    /** @brief Bits of one element, lanes included. */
    [[nodiscard]] constexpr size_t element_bits() const noexcept { return static_cast<size_t>(bits) * lanes; }
    /** @brief Whether one element is not a whole number of bytes. */
    [[nodiscard]] constexpr bool is_sub_byte() const noexcept { return element_bits() % 8 != 0; }
    /** @brief Bytes of one element, zero for a sub-byte type. */
    [[nodiscard]] constexpr size_t element_bytes() const noexcept { return is_sub_byte() ? 0 : element_bits() / 8; }

    /**
     * @brief Packed bytes needed for @p count elements.
     * @return Byte count, or std::nullopt on overflow or a zero-width type.
     */
    [[nodiscard]] std::optional<size_t> storage_bytes(uint64_t count) const noexcept;

    /** @brief Short readable name such as f32, bf16, i64, u8. */
    [[nodiscard]] std::string name() const;

    /**
     * @brief GpuDataFormat holding exactly this element, when one exists.
     *
     * Half precision maps to UINT16, the raw half-float storage format.
     */
    [[nodiscard]] std::optional<GpuDataFormat> gpu_format() const noexcept;
};

}
