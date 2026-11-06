#pragma once

#include "MayaFlux/Transitive/Reflect/EnumReflect.hpp"
#include "MayaFlux/Yantra/ComputeOperation.hpp"

#include "SortingHelper.hpp"

/**
 * @file Sorter.hpp
 * @brief Modern, digital-first sorting framework for Maya Flux
 *
 * The Sorter system provides a clean, extensible foundation for data sorting
 * in the Maya Flux ecosystem. Unlike traditional sorting which operates on simple containers,
 * this embraces the digital paradigm: data-driven workflows, composability, and type safety.
 *
 * ## Core Philosophy
 * A sorter **organizes ComputeData** through digital-first approaches:
 * 1. **Algorithmic sorting:** Advanced mathematical operations, not simple comparisons
 * 2. **Multi-dimensional sorting:** N-dimensional sort keys, not just single values
 * 3. **Temporal sorting:** Sort based on time-series patterns, predictions
 * 4. **Cross-modal sorting:** Sort audio by visual features, video by audio, etc.
 * 5. **Computational sorting:** Leverage digital capabilities beyond analog metaphors
 *
 * ## Key Features
 * - **Flexible input/output:** Template-based I/O types defined at instantiation
 * - **Type-safe sorting:** C++20 concepts and compile-time guarantees
 * - **Sorting strategies:** Algorithmic, pattern-based, predictive
 * - **Composable operations:** Integrates with ComputeMatrix execution modes
 * - **Digital-first design:** Embraces computational possibilities beyond analog metaphors
 *
 * ## Usage Examples
 * ```cpp
 * // Sort DataVariant containing vectors
 * auto sorter = std::make_shared<MySorter<Kakshya::DataVariant>>();
 *
 * // Sort regions by custom criteria
 * auto region_sorter = std::make_shared<MySorter<
 *     std::vector<Kakshya::Region>,
 *     std::vector<Kakshya::Region>>>();
 *
 * // Sort with mathematical transformation
 * auto matrix_sorter = std::make_shared<MySorter<
 *     Eigen::MatrixXd,
 *     Eigen::MatrixXd>>();
 * ```
 */

namespace MayaFlux::Yantra {

/**
 * @enum SortingType
 * @brief Categories of sorting operations for discovery and organization
 */
enum class SortingType : uint8_t {
    STANDARD, ///< Traditional comparison-based sorting
    ALGORITHMIC, ///< Mathematical/computational sorting algorithms
    PATTERN_BASED, ///< Sort based on pattern recognition
    TEMPORAL, ///< Time-series aware sorting
    SPATIAL, ///< Multi-dimensional spatial sorting
    PREDICTIVE, ///< ML/AI-based predictive sorting
    CROSS_MODAL, ///< Sort one modality by features of another
    RECURSIVE, ///< Recursive/hierarchical sorting
    CUSTOM ///< User-defined sorting types
};

/**
 * @enum SortingStrategy
 * @brief Sorting execution strategies
 */
enum class SortingStrategy : uint8_t {
    IN_PLACE, ///< Sort data in-place (modifies input)
    COPY_SORT, ///< Create sorted copy (preserves input)
    INDEX_ONLY, ///< Generate sort indices only
    PARTIAL_SORT, ///< Sort only top-K elements
    LAZY_SORT, ///< Lazy evaluation sorting (future: coroutines)
    CHUNKED_SORT, ///< Sort in chunks for large datasets
    PARALLEL_SORT ///< Parallel/concurrent sorting
};

/**
 * @struct SorterConfig
 * @brief Parent of every sorter's config struct.
 *
 * Holds no settings. Each sorter's config struct adds the fields that sorter reads,
 * and is stored whole, keyed by type, in the operation's facets.
 */
struct SorterConfig : OperationConfig {
    static constexpr auto describe_facets()
    {
        return std::tuple<> {};
    }
};

/**
 * @class Sorter
 * @brief Template-flexible sorter base with instance-defined I/O types
 *
 * The Sorter provides a clean, concept-based foundation for all sorting
 * operations. I/O types are defined at instantiation time, providing maximum flexibility
 * while maintaining type safety through C++20 concepts.
 *
 * Unlike traditional sorters that only handle simple containers, this embraces the
 * digital paradigm with analyzer delegation, cross-modal sorting, and computational
 * approaches that go beyond analog metaphors.
 */
template <ComputeData InputType = std::vector<Kakshya::DataVariant>, ComputeData OutputType = InputType>
class Sorter : public ComputeOperation<InputType, OutputType> {
public:
    using input_type = Datum<InputType>;
    using output_type = Datum<OutputType>;
    using base_type = ComputeOperation<InputType, OutputType>;

    virtual ~Sorter() = default;

    /**
     * @brief Gets the sorting type category for this sorter
     * @return SortingType enum value
     */
    [[nodiscard]] virtual SortingType get_sorting_type() const = 0;

    /**
     * @brief Gets human-readable name for this sorter
     * @return String identifier for the sorter
     */
    [[nodiscard]] std::string get_name() const override
    {
        return get_sorter_name();
    }

    [[nodiscard]] OperationType get_operation_type() const override
    {
        return OperationType::SORTER;
    }

    [[nodiscard]] ConfigRefs configs() const override
    {
        return this->stored_configs(Supports<SorterConfig> {});
    }

protected:
    /**
     * @brief Core operation implementation - called by ComputeOperation interface
     * @param input Input data with metadata
     * @return Output data with metadata
     */
    output_type operation_function(const input_type& input) override
    {
        return sort_implementation(input);
    }

    /**
     * @brief Pure virtual sorting implementation - derived classes implement this
     * @param input Input data with metadata
     * @return Sorted output
     */
    virtual output_type sort_implementation(const input_type& input) = 0;

    /**
     * @brief Get sorter-specific name (derived classes override this)
     * @return Sorter name string
     */
    [[nodiscard]] virtual std::string get_sorter_name() const { return "Sorter"; }

    /**
     * @brief Input validation (override for custom validation logic)
     */
    virtual bool validate_sorting_input(const input_type& /*input*/) const
    {
        // Default: accept any input that satisfies ComputeData concept
        return true;
    }
};

/// Sorter that takes DataVariant and produces DataVariant
template <ComputeData OutputType = std::vector<Kakshya::DataVariant>>
using DataSorter = Sorter<std::vector<Kakshya::DataVariant>, OutputType>;

/// Sorter for signal container processing
template <ComputeData OutputType = std::shared_ptr<Kakshya::SignalSourceContainer>>
using ContainerSorter = Sorter<std::shared_ptr<Kakshya::SignalSourceContainer>, OutputType>;

/// Sorter for region-based sorting
template <ComputeData OutputType = Kakshya::Region>
using RegionSorter = Sorter<Kakshya::Region, OutputType>;

/// Sorter for region group processing
template <ComputeData OutputType = Kakshya::RegionGroup>
using RegionGroupSorter = Sorter<Kakshya::RegionGroup, OutputType>;

/// Sorter for segment processing
template <ComputeData OutputType = std::vector<Kakshya::RegionSegment>>
using SegmentSorter = Sorter<std::vector<Kakshya::RegionSegment>, OutputType>;

/// Sorter that produces Eigen matrices
template <ComputeData InputType = std::vector<Kakshya::DataVariant>>
using MatrixSorter = Sorter<InputType, Eigen::MatrixXd>;

/// Sorter that produces Eigen vectors
template <ComputeData InputType = std::vector<Kakshya::DataVariant>>
using VectorSorter = Sorter<InputType, Eigen::VectorXd>;

/// Sorter for vector containers
template <typename T, ComputeData OutputType = std::vector<std::vector<T>>>
using VectorContainerSorter = Sorter<std::vector<std::vector<T>>, OutputType>;

/// Sorter for indices generation
template <ComputeData InputType = std::vector<Kakshya::DataVariant>>
using IndexSorter = Sorter<InputType, std::vector<std::vector<size_t>>>;

// ============================================================================
// UNIVERSAL COMPUTE DATA FUNCTIONS
// ============================================================================

} // namespace MayaFlux::Yantra
