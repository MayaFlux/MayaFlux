#pragma once

#include "MayaFlux/Kakshya/NDData/Composite.hpp"
#include "MayaFlux/Kakshya/Source/CompositeContainer.hpp"
#include "MayaFlux/Kakshya/Source/TensorContainer.hpp"
#include "MayaFlux/Kakshya/Utils/DataUtils.hpp"
#include "StructureIntrospection.hpp"

namespace MayaFlux::Yantra {

/**
 * @concept SourceContainer
 * @brief A container a Datum can carry: a SignalSourceContainer, a CompositeContainer,
 *        a TensorContainer, or a type derived from one of them.
 */
template <typename C>
concept SourceContainer = std::derived_from<C, Kakshya::SignalSourceContainer>
    || std::derived_from<C, Kakshya::CompositeContainer>
    || std::derived_from<C, Kakshya::TensorContainer>;

/**
 * @brief The container family a SourceContainer belongs to.
 *
 * Every SignalSourceContainer child maps to SignalSourceContainer; the other
 * two families map to themselves.
 */
template <SourceContainer C>
using container_family_t = std::conditional_t<std::derived_from<C, Kakshya::SignalSourceContainer>,
    Kakshya::SignalSourceContainer,
    std::conditional_t<std::derived_from<C, Kakshya::CompositeContainer>,
        Kakshya::CompositeContainer,
        Kakshya::TensorContainer>>;

/**
 * @brief Facet key under which a Datum carries a container of family @p F.
 */
template <SourceContainer F>
    requires std::same_as<F, container_family_t<F>>
const Facet<std::shared_ptr<F>>& container_key()
{
    static const Facet<std::shared_ptr<F>> key;
    return key;
}

/**
 * @struct Datum
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
 * - Facets carry described result structs through a chain, read by type
 * - The container a Datum refers to rides in a facet, one key per container family
 * - Move semantics for efficiency with large data
 */
template <ComputeData T = std::vector<Kakshya::DataVariant>>
struct Datum {
    T data; ///< The actual computation data
    std::vector<Kakshya::DataDimension> dimensions; ///< Data dimensional structure
    Kakshya::DataModality modality {}; ///< Data modality (audio, image, spectral, etc.)
    std::unordered_map<std::string, std::any> metadata; ///< Associated metadata
    Facets facets; ///< Typed results that travel with the data, read by type
    bool b_container_processing {};

    /** @brief Construct an empty datum when its data type has a valid default. */
    Datum() requires std::default_initializable<T> = default;

    /**
     * @brief Construct from data by copy with automatic structure inference
     * @param d Data to copy into the container
     *
     * Automatically infers dimensions and modality from the data type and content.
     * For containers, uses their existing dimensional information.
     * For other types, creates appropriate dimensional structures.
     */
    Datum(const T& d)
        : data(d)
    {
        auto [dims, mod] = infer_structure(d);
        dimensions = std::move(dims);
        modality = mod;
    }

    /**
     * @brief Construct from data by copy with container reference and automatic structure inference
     * @param d Data to copy into the container
     * @param cont Shared pointer to the container the data refers to
     *
     * This constructor is useful when the data is part of a larger container
     * and we want to maintain a reference to that container for context.
     * The container is stored in the facet of its family.
     */
    template <SourceContainer C>
    Datum(const T& d, const std::shared_ptr<C>& cont)
        : data(d)
    {
        set_container(cont);
        auto [dims, mod] = infer_structure(data, cont);
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
    Datum(T&& d)
        : data(std::move(d))
    {
        // Note: We infer from data after it's moved, which should be fine
        // since inference typically only needs type info and basic properties
        auto [dims, mod] = infer_structure(data);
        dimensions = std::move(dims);
        modality = mod;
    }

    /**
     * @brief Construct from data by move with automatic structure inference
     * @param d Data to move into the container
     * @param cont Shared pointer to the container the data refers to
     *
     * Automatically infers dimensions and modality before moving the data.
     * More efficient for large data structures.
     */
    template <SourceContainer C>
    Datum(T&& d, const std::shared_ptr<C>& cont)
        : data(std::move(d))
    {
        set_container(cont);
        auto [dims, mod] = infer_structure(data, cont);
        dimensions = std::move(dims);
        modality = mod;
    }

    /**
     * @brief Construct with move and explicit structure information
     * @param d Data to move
     * @param dims Explicit dimensional structure
     * @param mod Explicit data modality
     */
    Datum(T&& d, std::vector<Kakshya::DataDimension> dims, Kakshya::DataModality mod)
        : data(std::move(d))
        , dimensions(std::move(dims))
        , modality(mod)
    {
    }

    /**
     * @brief Construct with move, a container and explicit structure information
     * @param d Data to move
     * @param cont Shared pointer to the container the data refers to
     * @param dims Explicit dimensional structure
     * @param mod Explicit data modality
     */
    template <SourceContainer C>
    Datum(T&& d, const std::shared_ptr<C>& cont, std::vector<Kakshya::DataDimension> dims, Kakshya::DataModality mod)
        : data(std::move(d))
        , dimensions(std::move(dims))
        , modality(mod)
    {
        set_container(cont);
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
     * @brief Set or update the associated container of one family
     * @tparam C A container type; derived signal containers share the signal family
     * @param cont Shared pointer to the new container; null removes the family's container
     */
    template <SourceContainer C>
    void set_container(const std::shared_ptr<C>& cont)
    {
        using Family = container_family_t<C>;
        if (cont) {
            facets.set(container_key<Family>(), std::shared_ptr<Family>(cont));
        } else {
            facets.erase(container_key<Family>());
        }
    }

    /** @brief Remove the signal container. */
    void set_container(std::nullptr_t)
    {
        reset_container();
    }

    /**
     * @brief Remove the container of one family
     * @tparam C Family to clear; the signal family by default
     */
    template <SourceContainer C = Kakshya::SignalSourceContainer>
    void reset_container()
    {
        facets.erase(container_key<container_family_t<C>>());
    }

    /**
     * @brief The associated container of one family
     * @tparam C Family to read; the signal family by default
     * @return The container, or null when none is set
     */
    template <SourceContainer C = Kakshya::SignalSourceContainer>
    [[nodiscard]] std::shared_ptr<container_family_t<C>> get_container() const
    {
        return facets.get(container_key<container_family_t<C>>());
    }

    /**
     * @brief Check if a container of any family is associated
     * @return True if a signal, composite or tensor container is set
     */
    [[nodiscard]] bool has_container() const
    {
        return get_container<Kakshya::SignalSourceContainer>()
            || get_container<Kakshya::CompositeContainer>()
            || get_container<Kakshya::TensorContainer>();
    }

    /**
     * @brief Check if a container of one family is associated
     * @tparam C Family to check
     */
    template <SourceContainer C>
    [[nodiscard]] bool has_container() const
    {
        return get_container<C>() != nullptr;
    }

    /**
     * @brief Check if processing is needed (for container types)
     * @return True if processing is required before data extraction
     */
    [[nodiscard]] bool needs_processig() const
    {
        if constexpr (std::is_same_v<T, std::shared_ptr<Kakshya::SignalSourceContainer>>) {
            return b_container_processing;
        }
        return false;
    }

    /**
     * @brief Set whether processing is needed (for container types)
     * @param val True if processing should be done before data extraction
     */
    void set_container_processing(bool val)
    {
        if constexpr (std::is_same_v<T, std::shared_ptr<Kakshya::SignalSourceContainer>>) {
            b_container_processing = val;
        } else {
            b_container_processing = false;
        }
    }

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
        return Kakshya::find_dimension_by_role(dimensions, role);
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
        if constexpr (RequiresContainer<T>) {
            if (!has_container()) {
                return false;
            }
        }
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
        if (it == metadata.end()) {
            return std::nullopt;
        }

        return safe_any_cast<ValueType>(it->second);
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
template <ComputeData T = std::vector<Kakshya::DataVariant>>
class OpUnit {
public:
    virtual ~OpUnit() = default;

    /**
     * @brief Execute the operation on input data
     * @param input Input data container with metadata
     * @return Processed output data container
     */
    virtual Datum<T> execute(const Datum<T>& input) = 0;

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

/// Helper to detect if a type is an Datum
template <typename>
struct is_IO : std::false_type { };
/// Specialization for Datum types
template <typename T>
struct is_IO<Datum<T>> : std::true_type { };

/**
 * @brief Convert a vector of raw values into a vector of Datum wrappers
 * @tparam T ComputeData type
 * @param inputs Raw input vector to wrap
 * @return Vector of Datum containers with inferred structure
 */
template <ComputeData T>
std::vector<Datum<T>> as_io_batch(const std::vector<T>& inputs)
{
    std::vector<Datum<T>> wrapped;
    wrapped.reserve(inputs.size());
    for (const auto& i : inputs)
        wrapped.emplace_back(i);
    return wrapped;
}

/**
 * @brief Convert a vector of raw values into a vector of Datum wrappers via move
 * @tparam T ComputeData type
 * @param inputs Raw input vector to consume
 * @return Vector of Datum containers with inferred structure
 */
template <ComputeData T>
std::vector<Datum<T>> as_io_batch(std::vector<T>&& inputs)
{
    std::vector<Datum<T>> wrapped;
    wrapped.reserve(inputs.size());
    for (auto& i : inputs)
        wrapped.emplace_back(std::move(i));
    return wrapped;
}

/**
 * @concept OperationReadyData
 * @brief Concept to constrain types suitable for operation units
 *
 * Ensures that types used in operation units are either:
 * - MultiVariant (data variant)
 * - CompositeData (schema-bearing records or their owning container)
 * - TensorContainerData (a tensor container)
 * - RegionLike (regions or region groups)
 * - EigenMatrixLike (Eigen matrices/vectors)
 * - Datum (input/output containers with structure)
 */
template <typename T>
concept OperationReadyData = CompositeData<T> || TensorContainerData<T> || MultiVariant<T> || RegionLike<T> || EigenMatrixLike<T> || is_IO<T>::value;

using DataIO = Datum<std::vector<Kakshya::DataVariant>>; ///< IO for data variant
using ContainerIO = Datum<std::shared_ptr<Kakshya::SignalSourceContainer>>; ///< IO for signal containers
using CompositeIO = Datum<Kakshya::CompositeArray>; ///< IO for owning Composite records
using CompositeContainerIO = Datum<std::shared_ptr<Kakshya::CompositeContainer>>; ///< IO for Composite containers
using TensorContainerIO = Datum<std::shared_ptr<Kakshya::TensorContainer>>; ///< IO for Tensor containers
using RegionIO = Datum<Kakshya::Region>; ///< IO for single regions
using RegionGroupIO = Datum<Kakshya::RegionGroup>; ///< IO for region groups
using SegmentIO = Datum<std::vector<Kakshya::RegionSegment>>; ///< IO for region segments

}
