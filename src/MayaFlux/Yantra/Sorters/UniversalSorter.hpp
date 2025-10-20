#pragma once

#include "MayaFlux/EnumUtils.hpp"
#include "MayaFlux/Yantra/ComputeOperation.hpp"

/**
 * @file UniversalSorter.hpp
 * @brief Modern, digital-first universal sorting framework for Maya Flux
 *
 * The UniversalSorter system provides a clean, extensible foundation for data sorting
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
 * - **Universal input/output:** Template-based I/O types defined at instantiation
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
 * @enum SortingDirection
 * @brief Basic sort direction for simple comparisons
 */
enum class SortingDirection : uint8_t {
    ASCENDING, ///< Smallest to largest
    DESCENDING, ///< Largest to smallest
    CUSTOM, ///< Use custom comparator function
    BIDIRECTIONAL ///< Sort with both directions (for special algorithms)
};

/**
 * @enum SortingGranularity
 * @brief Output granularity control for sorting results
 */
enum class SortingGranularity : uint8_t {
    RAW_DATA, ///< Direct sorted data
    ATTRIBUTED_INDICES, ///< Sort indices with metadata
    ORGANIZED_GROUPS, ///< Hierarchically organized sorted data
    DETAILED_ANALYSIS ///< Sorting analysis with statistics
};

/**
 * @struct SortKey
 * @brief Multi-dimensional sort key specification for complex sorting
 */
struct MAYAFLUX_API SortKey {
    std::string name;
    std::function<double(const std::any&)> extractor; ///< Extract sort value from data
    SortingDirection direction = SortingDirection::ASCENDING;
    double weight = 1.0; ///< Weight for multi-key sorting
    bool normalize = false; ///< Normalize values before sorting

    SortKey(std::string n, std::function<double(const std::any&)> e)
        : name(std::move(n))
        , extractor(std::move(e))
    {
    }
};

/**
 * @class UniversalSorter
 * @brief Template-flexible sorter base with instance-defined I/O types
 *
 * The UniversalSorter provides a clean, concept-based foundation for all sorting
 * operations. I/O types are defined at instantiation time, providing maximum flexibility
 * while maintaining type safety through C++20 concepts.
 *
 * Unlike traditional sorters that only handle simple containers, this embraces the
 * digital paradigm with analyzer delegation, cross-modal sorting, and computational
 * approaches that go beyond analog metaphors.
 */
template <ComputeData InputType = std::vector<Kakshya::DataVariant>, ComputeData OutputType = InputType>
class MAYAFLUX_API UniversalSorter : public ComputeOperation<InputType, OutputType> {
public:
    using input_type = IO<InputType>;
    using output_type = IO<OutputType>;
    using base_type = ComputeOperation<InputType, OutputType>;

    virtual ~UniversalSorter() = default;

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

    /**
     * @brief Type-safe parameter management with sorting-specific defaults
     */
    void set_parameter(const std::string& name, std::any value) override
    {
        if (name == "strategy") {
            auto strategy_result = safe_any_cast<SortingStrategy>(value);
            if (strategy_result) {
                m_strategy = *strategy_result.value;
                return;
            }
            auto str_result = safe_any_cast<std::string>(value);
            if (str_result) {
                auto strategy_enum = Utils::string_to_enum_case_insensitive<SortingStrategy>(*str_result.value);
                if (strategy_enum) {
                    m_strategy = *strategy_enum;
                    return;
                }
            }
        }
        if (name == "direction") {
            auto direction_result = safe_any_cast<SortingDirection>(value);
            if (direction_result) {
                m_direction = *direction_result.value;
                return;
            }
            auto str_result = safe_any_cast<std::string>(value);
            if (str_result) {
                auto direction_enum = Utils::string_to_enum_case_insensitive<SortingDirection>(*str_result.value);
                if (direction_enum) {
                    m_direction = *direction_enum;
                    return;
                }
            }
        }
        if (name == "granularity") {
            auto granularity_result = safe_any_cast<SortingGranularity>(value);
            if (granularity_result) {
                m_granularity = *granularity_result.value;
                return;
            }
            auto str_result = safe_any_cast<std::string>(value);
            if (str_result) {
                auto granularity_enum = Utils::string_to_enum_case_insensitive<SortingGranularity>(*str_result.value);
                if (granularity_enum) {
                    m_granularity = *granularity_enum;
                    return;
                }
            }
        }
        set_sorting_parameter(name, std::move(value));
    }

    [[nodiscard]] std::any get_parameter(const std::string& name) const override
    {
        if (name == "strategy") {
            return m_strategy;
        }
        if (name == "direction") {
            return m_direction;
        }
        if (name == "granularity") {
            return m_granularity;
        }
        return get_sorting_parameter(name);
    }

    [[nodiscard]] std::map<std::string, std::any> get_all_parameters() const override
    {
        auto params = get_all_sorting_parameters();
        params["strategy"] = m_strategy;
        params["direction"] = m_direction;
        params["granularity"] = m_granularity;
        return params;
    }

    /**
     * @brief Type-safe parameter access with defaults
     * @tparam T Parameter type
     * @param name Parameter name
     * @param default_value Default if not found/wrong type
     * @return Parameter value or default
     */
    template <typename T>
    T get_parameter_or_default(const std::string& name, const T& default_value) const
    {
        auto param = get_sorting_parameter(name);
        return safe_any_cast_or_default<T>(param, default_value);
    }

    /**
     * @brief Configure sorting strategy
     */
    void set_strategy(SortingStrategy strategy) { m_strategy = strategy; }
    SortingStrategy get_strategy() const { return m_strategy; }

    /**
     * @brief Configure sorting direction
     */
    void set_direction(SortingDirection direction) { m_direction = direction; }
    SortingDirection get_direction() const { return m_direction; }

    /**
     * @brief Configure output granularity
     */
    void set_granularity(SortingGranularity granularity) { m_granularity = granularity; }
    SortingGranularity get_granularity() const { return m_granularity; }

    /**
     * @brief Add multi-key sorting capability
     * @param keys Vector of sort keys for complex sorting
     */
    void set_sort_keys(const std::vector<SortKey>& keys) { m_sort_keys = keys; }
    const std::vector<SortKey>& get_sort_keys() const { return m_sort_keys; }

    /**
     * @brief Configure custom comparator for CUSTOM direction
     * @param comparator Custom comparison function
     */
    template <typename T>
    void set_custom_comparator(std::function<bool(const T&, const T&)> comparator)
    {
        m_custom_comparator = [comparator](const std::any& a, const std::any& b) -> bool {
            auto val_a_result = safe_any_cast<T>(a);
            auto val_b_result = safe_any_cast<T>(b);
            if (val_a_result && val_b_result) {
                return comparator(*val_a_result, *val_b_result);
            }
            return false;
        };
    }

protected:
    /**
     * @brief Core operation implementation - called by ComputeOperation interface
     * @param input Input data with metadata
     * @return Output data with metadata
     */
    output_type operation_function(const input_type& input) override
    {
        auto raw_result = sort_implementation(input);
        return apply_granularity_formatting(raw_result);
    }

    /**
     * @brief Pure virtual sorting implementation - derived classes implement this
     * @param input Input data with metadata
     * @return Raw sorting output before granularity processing
     */
    virtual output_type sort_implementation(const input_type& input) = 0;

    /**
     * @brief Get sorter-specific name (derived classes override this)
     * @return Sorter name string
     */
    [[nodiscard]] virtual std::string get_sorter_name() const { return "UniversalSorter"; }

    /**
     * @brief Sorting-specific parameter handling (override for custom parameters)
     */
    virtual void set_sorting_parameter(const std::string& name, std::any value)
    {
        m_parameters[name] = std::move(value);
    }

    [[nodiscard]] virtual std::any get_sorting_parameter(const std::string& name) const
    {
        auto it = m_parameters.find(name);
        return (it != m_parameters.end()) ? it->second : std::any {};
    }

    [[nodiscard]] virtual std::map<std::string, std::any> get_all_sorting_parameters() const
    {
        return m_parameters;
    }

    /**
     * @brief Input validation (override for custom validation logic)
     */
    virtual bool validate_sorting_input(const input_type& /*input*/) const
    {
        // Default: accept any input that satisfies ComputeData concept
        return true;
    }

    /**
     * @brief Apply granularity-based output formatting
     * @param raw_output Raw sorting results
     * @return Formatted output based on granularity setting
     */
    virtual output_type apply_granularity_formatting(const output_type& raw_output)
    {
        switch (m_granularity) {
        case SortingGranularity::RAW_DATA:
            return raw_output;

        case SortingGranularity::ATTRIBUTED_INDICES:
            return add_sorting_metadata(raw_output);

        case SortingGranularity::ORGANIZED_GROUPS:
            return organize_into_groups(raw_output);

        case SortingGranularity::DETAILED_ANALYSIS:
            return create_sorting_analysis(raw_output);

        default:
            return raw_output;
        }
    }

    /**
     * @brief Add sorting metadata to results (override for custom attribution)
     */
    virtual output_type add_sorting_metadata(const output_type& raw_output)
    {
        output_type attributed = raw_output;
        attributed.metadata["sorting_type"] = static_cast<int>(get_sorting_type());
        attributed.metadata["sorter_name"] = get_sorter_name();
        attributed.metadata["strategy"] = static_cast<int>(m_strategy);
        attributed.metadata["direction"] = static_cast<int>(m_direction);
        attributed.metadata["granularity"] = static_cast<int>(m_granularity);
        return attributed;
    }

    /**
     * @brief Organize results into hierarchical groups (override for custom grouping)
     */
    virtual output_type organize_into_groups(const output_type& raw_output)
    {
        // Default implementation: just add grouping metadata
        return add_sorting_metadata(raw_output);
    }

    /**
     * @brief Create detailed sorting analysis (override for custom analysis)
     */
    virtual output_type create_sorting_analysis(const output_type& raw_output)
    {
        // Default implementation: add analysis metadata
        auto analysis = add_sorting_metadata(raw_output);
        analysis.metadata["is_analysis"] = true;
        analysis.metadata["sort_keys_count"] = m_sort_keys.size();
        return analysis;
    }

    /**
     * @brief Helper to check if custom comparator is available
     */
    bool has_custom_comparator() const { return m_custom_comparator != nullptr; }

    /**
     * @brief Apply custom comparator if available
     */
    bool apply_custom_comparator(const std::any& a, const std::any& b) const
    {
        if (m_custom_comparator) {
            return m_custom_comparator(a, b);
        }
        return false;
    }

    /**
     * @brief Apply multi-key sorting if keys are configured
     */
    virtual bool should_use_multi_key_sorting() const
    {
        return !m_sort_keys.empty();
    }

    /**
     * @brief Current sorting operation storage for complex operations
     */
    mutable std::any m_current_sorting;

    template <typename SortingResultType>
    void store_current_sorting(SortingResultType&& result) const
    {
        m_current_sorting = std::forward<SortingResultType>(result);
    }

    output_type m_current_output;

private:
    SortingStrategy m_strategy = SortingStrategy::COPY_SORT;
    SortingDirection m_direction = SortingDirection::ASCENDING;
    SortingGranularity m_granularity = SortingGranularity::RAW_DATA;
    std::map<std::string, std::any> m_parameters;

    std::vector<SortKey> m_sort_keys;
    std::function<bool(const std::any&, const std::any&)> m_custom_comparator;
};

/// Sorter that takes DataVariant and produces DataVariant
template <ComputeData OutputType = std::vector<Kakshya::DataVariant>>
using DataSorter = UniversalSorter<std::vector<Kakshya::DataVariant>, OutputType>;

/// Sorter for signal container processing
template <ComputeData OutputType = std::shared_ptr<Kakshya::SignalSourceContainer>>
using ContainerSorter = UniversalSorter<std::shared_ptr<Kakshya::SignalSourceContainer>, OutputType>;

/// Sorter for region-based sorting
template <ComputeData OutputType = Kakshya::Region>
using RegionSorter = UniversalSorter<Kakshya::Region, OutputType>;

/// Sorter for region group processing
template <ComputeData OutputType = Kakshya::RegionGroup>
using RegionGroupSorter = UniversalSorter<Kakshya::RegionGroup, OutputType>;

/// Sorter for segment processing
template <ComputeData OutputType = std::vector<Kakshya::RegionSegment>>
using SegmentSorter = UniversalSorter<std::vector<Kakshya::RegionSegment>, OutputType>;

/// Sorter that produces Eigen matrices
template <ComputeData InputType = std::vector<Kakshya::DataVariant>>
using MatrixSorter = UniversalSorter<InputType, Eigen::MatrixXd>;

/// Sorter that produces Eigen vectors
template <ComputeData InputType = std::vector<Kakshya::DataVariant>>
using VectorSorter = UniversalSorter<InputType, Eigen::VectorXd>;

/// Sorter for vector containers
template <typename T, ComputeData OutputType = std::vector<std::vector<T>>>
using VectorContainerSorter = UniversalSorter<std::vector<std::vector<T>>, OutputType>;

/// Sorter for indices generation
template <ComputeData InputType = std::vector<Kakshya::DataVariant>>
using IndexSorter = UniversalSorter<InputType, std::vector<std::vector<size_t>>>;

} // namespace MayaFlux::Yantra
