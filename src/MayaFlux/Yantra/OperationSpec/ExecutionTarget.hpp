#pragma once

/**
 * @file ExecutionTarget.hpp
 * @brief Where and on what an operation can run, as sets that compose.
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
 * @brief A device set and a runtime set. Two targets meet when both axes intersect.
 *
 * Composes with `|` per axis. The result can admit a pair neither input named
 * (CPU+Native joined with GPU+Vulkan admits GPU+Native).
 */
struct Target {
    Device devices { Device::NONE };
    Runtime runtimes { Runtime::NONE };

    [[nodiscard]] constexpr Target operator|(Target other) const noexcept
    {
        return { devices | other.devices, runtimes | other.runtimes };
    }

    /** @brief True when both axes share a member. */
    [[nodiscard]] constexpr bool meets(Target other) const noexcept
    {
        return intersects(devices, other.devices) && intersects(runtimes, other.runtimes);
    }

    [[nodiscard]] constexpr bool empty() const noexcept
    {
        return devices == Device::NONE || runtimes == Runtime::NONE;
    }

    [[nodiscard]] constexpr bool operator==(const Target&) const noexcept = default;
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
