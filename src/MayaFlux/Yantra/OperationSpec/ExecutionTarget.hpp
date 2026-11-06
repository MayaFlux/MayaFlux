#pragma once

/**
 * @file ExecutionTarget.hpp
 * @brief Where and on what an operation can run, as sets of pairs that compose.
 *
 * @experimental The shape of Target and its use in resolution are not settled.
 */

namespace MayaFlux::Yantra {

/**
 * @enum Device
 * @brief The silicon the work runs on. A set: values combine with `|`.
 */
enum class Device : uint8_t {
    NONE = 0,
    CPU = 1U << 0,
    GPU = 1U << 1,
    NPU = 1U << 2,
    CUSTOM_0 = 1U << 6,
    CUSTOM_1 = 1U << 7,
};

/**
 * @enum Runtime
 * @brief The software stack that executes the work. A set: values combine with `|`.
 */
enum class Runtime : uint8_t {
    NONE = 0,
    NATIVE = 1U << 0, ///< Compiled C++ in this library
    VULKAN = 1U << 1, ///< Vulkan compute shaders
    ONNX = 1U << 2, ///< ONNX runtime
    CUSTOM_0 = 1U << 6,
    CUSTOM_1 = 1U << 7,
};

template <typename E>
concept FlagEnum = std::same_as<E, Device> || std::same_as<E, Runtime>;

template <FlagEnum E>
[[nodiscard]] constexpr E operator|(E a, E b) noexcept
{
    return static_cast<E>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

template <FlagEnum E>
[[nodiscard]] constexpr E operator&(E a, E b) noexcept
{
    return static_cast<E>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

/** @brief True when `set` holds every member of `members`. */
template <FlagEnum E>
[[nodiscard]] constexpr bool contains(E set, E members) noexcept
{
    return (set & members) == members;
}

/** @brief True when the two sets share a member. */
template <FlagEnum E>
[[nodiscard]] constexpr bool intersects(E a, E b) noexcept
{
    return (a & b) != E::NONE;
}

/**
 * @struct Target
 * @brief A set of (device, runtime) pairs. Two targets meet when they share a pair.
 *
 * Built from a device set and a runtime set, which gives every combination of
 * the two (CPU|GPU with ONNX is CPU+ONNX and GPU+ONNX). Joining with `|` is a
 * union of pairs, so CPU+Native joined with GPU+Vulkan names exactly those two
 * and does not admit GPU+Native.
 */
struct Target {
    constexpr Target() noexcept = default;

    /** @brief Every pairing of a device in @p device_set with a runtime in @p runtime_set. */
    constexpr Target(Device device_set, Runtime runtime_set) noexcept
    {
        for (unsigned device = 0; device < 8; ++device) {
            if (((static_cast<unsigned>(device_set) >> device) & 1U) != 0U) {
                m_pairs |= uint64_t { static_cast<uint8_t>(runtime_set) } << (device * 8U);
            }
        }
    }

    [[nodiscard]] constexpr Target operator|(Target other) const noexcept
    {
        Target joined;
        joined.m_pairs = m_pairs | other.m_pairs;
        return joined;
    }

    /** @brief True when the two targets share a (device, runtime) pair. */
    [[nodiscard]] constexpr bool meets(Target other) const noexcept
    {
        return (m_pairs & other.m_pairs) != 0U;
    }

    /** @brief True when the pair (@p device, @p runtime) is in the set. Both must be single members. */
    [[nodiscard]] constexpr bool has(Device device, Runtime runtime) const noexcept
    {
        return meets(Target { device, runtime });
    }

    [[nodiscard]] constexpr bool empty() const noexcept { return m_pairs == 0U; }

    /** @brief Devices that appear in any pair. */
    [[nodiscard]] constexpr Device devices() const noexcept
    {
        uint8_t result = 0;
        for (unsigned device = 0; device < 8; ++device) {
            if (((m_pairs >> (device * 8U)) & 0xFFU) != 0U) {
                result |= static_cast<uint8_t>(1U << device);
            }
        }
        return static_cast<Device>(result);
    }

    /** @brief Runtimes that appear in any pair. */
    [[nodiscard]] constexpr Runtime runtimes() const noexcept
    {
        uint8_t result = 0;
        for (unsigned device = 0; device < 8; ++device) {
            result |= static_cast<uint8_t>((m_pairs >> (device * 8U)) & 0xFFU);
        }
        return static_cast<Runtime>(result);
    }

    [[nodiscard]] constexpr bool operator==(const Target&) const noexcept = default;

private:
    /** One byte per device bit; each byte is the runtime set paired with that device. */
    uint64_t m_pairs { 0 };
};

namespace targets {
    inline constexpr Target cpu { Device::CPU, Runtime::NATIVE };
    inline constexpr Target vulkan { Device::GPU, Runtime::VULKAN };
    inline constexpr Target onnx_cpu { Device::CPU, Runtime::ONNX };
    inline constexpr Target onnx_gpu { Device::GPU, Runtime::ONNX };
}

/**
 * @brief The target an operation class declares through `static constexpr Target target`,
 *        or the CPU when it declares none.
 */
template <typename Op>
inline constexpr Target target_of_v = [] {
    if constexpr (requires { { Op::target } -> std::convertible_to<Target>; }) {
        return Target { Op::target };
    } else {
        return targets::cpu;
    }
}();

} // namespace MayaFlux::Yantra
