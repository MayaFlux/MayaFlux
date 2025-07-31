#pragma once

#include "MayaFlux/Kakshya/SignalSourceContainer.hpp"

namespace MayaFlux::Yantra {

// clang-format off

/**
 * @concept ComputeData
 * @brief Universal concept for types that can be used as data in compute operations.
 *
 * Defines the valid types that can flow through the computation pipeline.
 * Supports both direct type matching and constructible conversion patterns.
 *
 * Following types are considered valid:
 * - Kakshya::DataVariant: Universal data container
 * - std::shared_ptr<Kakshya::SignalSourceContainer>: Shared signal sources
 * - Kakshya::Region: Spatial/temporal markers
 * - Kakshya::RegionGroup: Collections of regions
 * - std::vector<Kakshya::RegionSegment>: Cached region data (with values)
 * - Any type constructible from Kakshya::DataVariant
 */
template <typename T>
concept ComputeData =
    std::same_as<T, Kakshya::DataVariant> ||
    std::same_as<T, std::shared_ptr<Kakshya::SignalSourceContainer>> ||
    std::same_as<T, Kakshya::Region> ||
    std::same_as<T, Kakshya::RegionGroup> ||
    std::same_as<T, std::vector<Kakshya::RegionSegment>> ||
    std::constructible_from<Kakshya::DataVariant, T>;
// clang-format on

/**
 * @struct IO
 * @brief Input/Output container for computation pipeline data flow.
 * @tparam T Data type that satisfies ComputeData concept
 *
 * Encapsulates data with associated metadata for pipeline operations.
 * Provides convenient access operators and move semantics for efficiency.
 */
template <ComputeData T = Kakshya::DataVariant>
struct IO {
    T data; ///< The actual computation data
    std::unordered_map<std::string, std::any> metadata; ///< Associated metadata

    IO() = default;

    /**
     * @brief Construct from data by copy
     * @param d Data to copy into the container
     */
    IO(const T& d)
        : data(d)
    {
    }

    /**
     * @brief Construct from data by move
     * @param d Data to move into the container
     */
    IO(T&& d)
        : data(std::move(d))
    {
    }

    /**
     * @brief Access underlying data (const)
     * @return Const reference to the data
     */
    const T& operator*() const { return data; }

    /**
     * @brief Access underlying data (mutable)
     * @return Mutable reference to the data
     */
    T& operator*() { return data; }
};

/**
 * @class OpUnit
 * @brief Abstract base class for operation units in recursive processing graphs.
 * @tparam T Data type that satisfies ComputeData concept
 *
 * Represents a single computational node that can process data and maintain
 * dependencies on other operation units. Forms the building blocks of
 * computation pipelines with automatic dependency resolution.
 */
template <ComputeData T = Kakshya::DataVariant>
class OpUnit {
public:
    virtual ~OpUnit() = default;

    /**
     * @brief Execute the operation on input data
     * @param input Input data container with metadata
     * @return Processed output data container
     */
    virtual IO<T> execute(const IO<T>& input) = 0;

    /**
     * @brief Get the name/identifier of this operation
     * @return String name of the operation
     */
    [[nodiscard]] virtual std::string get_name() const = 0;

    /**
     * @brief Add a dependency operation unit
     * @param dep Shared pointer to the dependency operation
     *
     * Dependencies are executed before this operation in the pipeline.
     */
    void add_dependency(std::shared_ptr<OpUnit<T>> dep)
    {
        dependencies.push_back(std::move(dep));
    }

    /**
     * @brief Get all dependency operations
     * @return Const reference to the dependencies vector
     */
    const auto& get_dependencies() const { return dependencies; }

protected:
    std::vector<std::shared_ptr<OpUnit<T>>> dependencies; ///< Operation dependencies
};

// === Type Aliases for Common Use Cases ===

using DataIO = IO<Kakshya::DataVariant>; ///< IO for universal data variant
using ContainerIO = IO<std::shared_ptr<Kakshya::SignalSourceContainer>>; ///< IO for signal containers
using RegionIO = IO<Kakshya::Region>; ///< IO for single regions
using RegionGroupIO = IO<Kakshya::RegionGroup>; ///< IO for region groups
using SegmentIO = IO<std::vector<Kakshya::RegionSegment>>; ///< IO for region segments

}
