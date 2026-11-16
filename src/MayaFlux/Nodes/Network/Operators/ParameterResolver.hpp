#pragma once

namespace MayaFlux::Nodes::Network {

/** @brief How one source maps to a network's elements. */
enum class MappingMode : uint8_t {
    BROADCAST, ///< One source value applies to every network element
    ONE_TO_ONE ///< Source elements correspond to network elements
};

/** @brief Hash a parameter name once when installing a mapping. */
[[nodiscard]] constexpr uint64_t parameter_hash(std::string_view name) noexcept
{
    uint64_t hash = 14695981039346656037ULL;
    for (unsigned char character : name) {
        hash ^= character;
        hash *= 1099511628211ULL;
    }
    return hash;
}

/** @brief A cached hash paired with its original name for collision-safe matching. */
struct ParameterKey {
    std::string_view name;
    uint64_t hash;

    [[nodiscard]] constexpr bool matches(std::string_view candidate) const noexcept
    {
        return hash == parameter_hash(candidate) && name == candidate;
    }
};

[[nodiscard]] constexpr ParameterKey parameter_key(std::string_view name) noexcept
{
    return { name, parameter_hash(name) };
}

} // namespace MayaFlux::Nodes::Network
