#pragma once

#include "NDData.hpp"
#include "TensorDType.hpp"

namespace MayaFlux::Kakshya {

/**
 * @enum TensorDeviceType
 * @brief Where tensor memory lives. Values equal the DLPack device types.
 */
enum class TensorDeviceType : int32_t {
    CPU = 1,
    CUDA = 2,
    CUDA_HOST = 3,
    OPENCL = 4,
    VULKAN = 7,
    METAL = 8,
    VPI = 9,
    ROCM = 10,
    ROCM_HOST = 11,
    EXT_DEV = 12,
    CUDA_MANAGED = 13,
    ONEAPI = 14,
    WEBGPU = 15,
    HEXAGON = 16,
    MAIA = 17,
    TRN = 18,
    TPU = 19,
    TPU_HOST = 20,
    ASCEND = 21
};

/**
 * @struct TensorDevice
 * @brief Device type and ordinal holding a tensor's memory.
 */
struct MAYAFLUX_API TensorDevice {
    TensorDeviceType type { TensorDeviceType::CPU };
    int32_t id {};

    constexpr bool operator==(const TensorDevice&) const noexcept = default;

    /** @brief Whether the host can read this memory directly. */
    [[nodiscard]] constexpr bool is_host_accessible() const noexcept
    {
        switch (type) {
        case TensorDeviceType::CPU:
        case TensorDeviceType::CUDA_HOST:
        case TensorDeviceType::CUDA_MANAGED:
        case TensorDeviceType::ROCM_HOST:
        case TensorDeviceType::TPU_HOST:
            return true;
        default:
            return false;
        }
    }
};

/**
 * @struct TensorQuantization
 * @brief Affine quantization: real = scale * (stored - zero_point).
 *
 * Per tensor when axis is empty (one scale), otherwise per slice along axis
 * (one scale per index of that axis). zero_point is empty for symmetric
 * quantization or matches scale in length.
 */
struct MAYAFLUX_API TensorQuantization {
    std::vector<double> scale;
    std::vector<int64_t> zero_point;
    std::optional<size_t> axis;
};

/**
 * @struct TensorSpec
 * @brief Complete description of a tensor: type, shape, layout, meaning.
 *
 * Follows the DLPack model. Strides are in elements and may be explicit,
 * negative or broadcast (zero), so views, Fortran order and transposes are
 * described without copying; empty strides mean C order. byte_offset locates
 * element zero inside the storage. axes optionally gives each dimension a
 * role and name, for example batch, channel and spatial axes.
 *
 * Usage:
 * @code
 * auto spec = TensorSpec::contiguous(TensorDType::of<float>(), { 1, 3, 224, 224 }, "input");
 * if (auto why = spec.diagnose(); !why.empty()) { }
 * @endcode
 */
struct MAYAFLUX_API TensorSpec {
    std::string name;
    TensorDType dtype;
    std::vector<uint64_t> shape;
    std::vector<int64_t> strides;
    uint64_t byte_offset {};
    std::vector<DataDimension> axes;
    std::optional<TensorQuantization> quantization;
    TensorDevice device;

    /** @brief C-ordered spec with no offset, axes or quantization. */
    [[nodiscard]] static TensorSpec contiguous(
        TensorDType dtype, std::vector<uint64_t> shape, std::string name = {});

    /** @brief Number of dimensions. */
    [[nodiscard]] size_t rank() const noexcept { return shape.size(); }

    /** @brief Product of the shape, or std::nullopt on overflow. */
    [[nodiscard]] std::optional<uint64_t> element_count() const noexcept;

    /** @brief Whether elements are laid out densely in C order. */
    [[nodiscard]] bool is_contiguous() const noexcept;

    /** @brief Element strides, with C order filled in when none are stored. */
    [[nodiscard]] std::vector<int64_t> resolved_strides() const;

    /**
     * @brief Storage bytes [first, last) the spec addresses, from storage start.
     * @return Range, or std::nullopt for overflow or a negative start.
     */
    [[nodiscard]] std::optional<std::pair<uint64_t, uint64_t>> byte_range() const noexcept;

    /** @brief Empty when the spec is self-consistent, otherwise the reason it is not. */
    [[nodiscard]] std::string diagnose() const;

    /** @brief Whether diagnose() reports no problem. */
    [[nodiscard]] bool is_valid() const { return diagnose().empty(); }

    /** @brief One DataDimension per axis, with size and stride taken from shape and strides. */
    [[nodiscard]] std::vector<DataDimension> dimensions() const;
};

}
