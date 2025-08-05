#pragma once

#include "Data/StructureIntrospection.hpp"

namespace MayaFlux::Yantra {

/**
 * @struct IO
 * @brief Input/Output container for computation pipeline data flow with structure preservation.
 * @tparam T Data type that satisfies ComputeData concept
 *
 * Encapsulates data with associated structure information and metadata for pipeline operations.
 * The structure (dimensions and modality) is automatically inferred at construction time,
 * ensuring that all data flowing through operations carries its dimensional context.
 *
 * Key Features:
 * - Automatic structure inference from data type and content
 * - Dimension and modality preservation through pipeline operations
 * - Flexible metadata storage for operation-specific information
 * - Move semantics for efficiency with large data
 */
template <ComputeData T = Kakshya::DataVariant>
struct IO {
    T data; ///< The actual computation data
    std::vector<Kakshya::DataDimension> dimensions; ///< Data dimensional structure
    Kakshya::DataModality modality {}; ///< Data modality (audio, image, spectral, etc.)
    std::unordered_map<std::string, std::any> metadata; ///< Associated metadata

    IO() = default; ///< Default constructor

    /**
     * @brief Construct from data by copy with automatic structure inference
     * @param d Data to copy into the container
     *
     * Automatically infers dimensions and modality from the data type and content.
     * For containers, uses their existing dimensional information.
     * For other types, creates appropriate dimensional structures.
     */
    IO(const T& d)
        : data(d)
    {
        auto [dims, mod] = infer_structure(d);
        dimensions = std::move(dims);
        modality = mod;
    }

    /**
     * @brief Construct from data by move with automatic structure inference
     * @param d Data to move into the container
     *
     * Automatically infers dimensions and modality before moving the data.
     * More efficient for large data structures.
     */
    IO(T&& d)
        : data(std::move(d))
    {
        // Note: We infer from data after it's moved, which should be fine
        // since inference typically only needs type info and basic properties
        auto [dims, mod] = infer_structure(data);
        dimensions = std::move(dims);
        modality = mod;
    }

    /**
     * @brief Construct with explicit structure information
     * @param d Data to store
     * @param dims Explicit dimensional structure
     * @param mod Explicit data modality
     *
     * Use this constructor when you want to override the automatic inference
     * or when you have more accurate structure information than can be inferred.
     */
    IO(const T& d, std::vector<Kakshya::DataDimension> dims, Kakshya::DataModality mod)
        : data(d)
        , dimensions(std::move(dims))
        , modality(mod)
    {
    }

    /**
     * @brief Construct with move and explicit structure information
     * @param d Data to move
     * @param dims Explicit dimensional structure
     * @param mod Explicit data modality
     */
    IO(T&& d, std::vector<Kakshya::DataDimension> dims, Kakshya::DataModality mod)
        : data(std::move(d))
        , dimensions(std::move(dims))
        , modality(mod)
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

    /**
     * @brief Get data dimensionality (number of dimensions)
     * @return Number of dimensions in the data structure
     */
    [[nodiscard]] size_t get_dimensionality() const { return dimensions.size(); }

    /**
     * @brief Check if data has specific modality
     * @param target_modality Modality to check for
     * @return True if data matches the target modality
     */
    [[nodiscard]] bool has_modality(Kakshya::DataModality target_modality) const
    {
        return modality == target_modality;
    }

    /**
     * @brief Get total number of elements across all dimensions
     * @return Product of all dimension sizes
     */
    [[nodiscard]] uint64_t get_total_elements() const
    {
        uint64_t total = 1;
        for (const auto& dim : dimensions) {
            total *= dim.size;
        }
        return total;
    }

    /**
     * @brief Find dimension by semantic role
     * @param role Dimension role to search for
     * @return Index of dimension with that role, or -1 if not found
     */
    [[nodiscard]] int find_dimension_by_role(Kakshya::DataDimension::Role role) const
    {
        for (size_t i = 0; i < dimensions.size(); ++i) {
            if (dimensions[i].role == role) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    /**
     * @brief Check if data is suitable for a specific type of processing
     * @param required_modality Required data modality
     * @param min_dimensions Minimum number of dimensions required
     * @return True if data meets the requirements
     */
    [[nodiscard]] bool is_suitable_for_processing(Kakshya::DataModality required_modality,
        size_t min_dimensions = 1) const
    {
        return (modality == required_modality || required_modality == Kakshya::DataModality::UNKNOWN) && dimensions.size() >= min_dimensions;
    }

    /**
     * @brief Update structure information (use carefully!)
     * @param new_dims New dimensional structure
     * @param new_modality New data modality
     *
     * This allows operations to update structure info when they transform
     * the data in ways that change its dimensional characteristics.
     */
    void update_structure(std::vector<Kakshya::DataDimension> new_dims,
        Kakshya::DataModality new_modality)
    {
        dimensions = std::move(new_dims);
        modality = new_modality;
    }

    /**
     * @brief Add or update metadata entry
     * @param key Metadata key
     * @param value Metadata value
     */
    template <typename ValueType>
    void set_metadata(const std::string& key, ValueType&& value)
    {
        metadata[key] = std::forward<ValueType>(value);
    }

    /**
     * @brief Get metadata entry with type safety
     * @tparam ValueType Expected value type
     * @param key Metadata key
     * @return Optional value if found and correct type
     */
    template <typename ValueType>
    std::optional<ValueType> get_metadata(const std::string& key) const
    {
        auto it = metadata.find(key);
        if (it != metadata.end()) {
            try {
                return std::any_cast<ValueType>(it->second);
            } catch (const std::bad_any_cast&) {
                return std::nullopt;
            }
        }
        return std::nullopt;
    }
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

using DataIO = IO<Kakshya::DataVariant>; ///< IO for universal data variant
using ContainerIO = IO<std::shared_ptr<Kakshya::SignalSourceContainer>>; ///< IO for signal containers
using RegionIO = IO<Kakshya::Region>; ///< IO for single regions
using RegionGroupIO = IO<Kakshya::RegionGroup>; ///< IO for region groups
using SegmentIO = IO<std::vector<Kakshya::RegionSegment>>; ///< IO for region segments

}
