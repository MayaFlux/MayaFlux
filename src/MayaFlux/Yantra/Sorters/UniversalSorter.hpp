#pragma once

#include "MayaFlux/Yantra/Analyzers/UniversalAnalyzer.hpp"
#include "SorterHelpers.hpp"

/**
 * @file UniversalSorter.hpp
 * @brief Modern, digital-first universal sorting framework for Maya Flux
 *
 * This header defines the core sorting abstractions for the Maya Flux ecosystem,
 * enabling robust, type-safe, and extensible sorting pipelines for multi-dimensional
 * data. Unlike traditional sorting which operates on simple containers, this system
 * embraces the digital paradigm completely with analyzer delegation and YantraUtils integration.
 *
 * Key Digital-First Principles:
 * - **Data is everything:** Sort any data type - audio, video, metadata, analysis results
 * - **Analyzer delegation:** Delegate complex sort key extraction to specialized analyzers
 * - **Multi-dimensional sorting:** N-dimensional sort keys, not just single values
 * - **Grammar-based sorting:** Define sort rules like parsing grammars
 * - **Lazy evaluation:** Future integration with Vruta/Kriya coroutine systems
 * - **Algorithmic sorting:** Complex mathematical operations, not simple comparisons
 * - **Temporal awareness:** Sort based on time-series patterns, predictions
 * - **Cross-modal sorting:** Sort audio by visual features, video by audio, etc.
 */

namespace MayaFlux::Yantra {

/**
 * @class SortingGrammar
 * @brief Grammar-based sorting rules for complex sorting logic
 */
class SortingGrammar {
public:
    enum class SortingContext {
        TEMPORAL,
        SPECTRAL,
        SPATIAL,
        SEMANTIC,
        STATISTICAL,
        CROSS_MODAL
    };

    struct Rule {
        std::string name;
        std::function<bool(const SorterInput&)> matcher; ///< Check if rule applies
        std::function<SorterOutput(const SorterInput&)> sorter; ///< Apply sorting
        std::vector<std::string> dependencies; ///< Required previous sorts
        SortingContext context;
        int priority = 0;
        SortingAlgorithm algorithm = SortingAlgorithm::STANDARD;
    };

    void add_rule(const Rule& rule);
    std::optional<SorterOutput> sort_by_rule(const std::string& rule_name, const SorterInput& input) const;
    std::vector<SorterOutput> sort_all_matching(const SorterInput& input) const;
    std::vector<std::string> get_available_rules() const;

private:
    std::vector<Rule> m_rules;
};

/**
 * @class UniversalSorter
 * @brief Modern, concept-based universal sorter for Maya Flux with analyzer delegation
 *
 * Provides a unified, extensible interface for all sorting operations in the Maya Flux
 * ecosystem. Supports type-safe dispatch, parameterization, analyzer delegation, and
 * output granularity control using YantraUtils for maximum code reuse.
 *
 * Key Responsibilities:
 * - Dispatches to type-specific implementations via std::variant visitation
 * - Delegates complex sort operations to analyzers when beneficial
 * - Uses YantraUtils for all common sorting operations (eliminates code duplication)
 * - Supports runtime method discovery and introspection
 * - Enables dynamic configuration via parameter maps
 * - Integrates with ComputeMatrix and processing chains for composable workflows
 * - Optional flow enforcement for pipeline ordering
 *
 * Derived sorters implement type-specific sort_impl overloads for supported input types.
 */
class UniversalSorter : public ComputeOperation<SorterInput, SorterOutput> {
public:
    virtual ~UniversalSorter() = default;

    /**
     * @brief Main sorting method - dispatches to type-specific implementations or analyzer
     */
    SorterOutput apply_operation(SorterInput input) override;

    /**
     * @brief Type-safe sorting with explicit input/output types
     */
    template <SorterInputType InputT, SorterOutputType OutputT>
    OutputT sort_typed(InputT input, const std::string& method = "default")
    {
        auto old_method = get_sorting_method();
        set_parameter("method", method);

        SorterOutput result = apply_operation(SorterInput { input });

        set_parameter("method", old_method);

        if (std::holds_alternative<OutputT>(result)) {
            return std::get<OutputT>(result);
        }

        throw std::runtime_error("Type mismatch in sort_typed result");
    }

    /**
     * @brief Sort with specific algorithm
     */
    virtual SorterOutput sort_with_algorithm(SorterInput input, SortingAlgorithm algorithm);

    /**
     * @brief Multi-key sorting with weights
     */
    virtual SorterOutput sort_multi_key(SorterInput input, const std::vector<SortKey>& keys);

    /**
     * @brief Chunked sorting for large datasets (alternative to coroutines)
     * @note Future: Will integrate with Vruta/Kriya coroutine systems for true lazy evaluation
     */
    virtual std::vector<SorterOutput> sort_chunked(SorterInput input, size_t chunk_size = 1024);

    /**
     * @brief Grammar-based sorting
     */
    virtual SorterOutput sort_by_grammar(SorterInput input, const std::string& rule_name);

    /**
     * @brief Get available sorting methods for this sorter
     */
    virtual std::vector<std::string> get_available_methods() const = 0;

    /**
     * @brief Get available methods for a specific input type
     */
    std::vector<std::string> get_methods_for_type(std::type_index type_info) const;

    // ===== Analyzer Delegation Interface =====

    /**
     * @brief Set the analyzer to use for delegation
     * @param analyzer Shared pointer to a UniversalAnalyzer
     */
    inline void set_analyzer(std::shared_ptr<UniversalAnalyzer> analyzer) { m_analyzer = analyzer; }

    /**
     * @brief Enable or disable analyzer delegation
     * @param use True to enable, false to disable
     */
    inline void set_use_analyzer(bool use) { m_use_analyzer = use; }

    /**
     * @brief Check if analyzer delegation is enabled and analyzer is set
     * @return True if using analyzer, false otherwise
     */
    inline bool uses_analyzer() const { return m_use_analyzer && m_analyzer != nullptr; }

    /**
     * @brief Get the configured analyzer
     * @return Shared pointer to analyzer (may be nullptr)
     */
    std::shared_ptr<UniversalAnalyzer> get_analyzer() const { return m_analyzer; }

    // ===== Configuration Interface =====

    /**
     * @brief Set sorting granularity
     */
    void set_granularity(SortingGranularity granularity) { m_granularity = granularity; }
    SortingGranularity get_granularity() const { return m_granularity; }

    /**
     * @brief Set default sorting algorithm
     */
    void set_algorithm(SortingAlgorithm algorithm) { m_algorithm = algorithm; }
    SortingAlgorithm get_algorithm() const { return m_algorithm; }

    /**
     * @brief Set sort direction
     */
    void set_direction(SortDirection direction) { m_direction = direction; }
    SortDirection get_direction() const { return m_direction; }

    /**
     * @brief Enable/disable flow enforcement
     */
    void set_flow_enforcement(bool enabled) { m_flow_enforcement = enabled; }
    bool get_flow_enforcement() const { return m_flow_enforcement; }

    /**
     * @brief Add grammar rules for complex sorting
     */
    void add_grammar_rule(const SortingGrammar::Rule& rule) { m_grammar.add_rule(rule); }

    /**
     * @brief Parameter management
     */
    void set_parameter(const std::string& name, std::any value) override;
    std::any get_parameter(const std::string& name) const override;
    std::map<std::string, std::any> get_all_parameters() const override;

protected:
    /**
     * @brief Type-specific sorting implementations (to be overridden by derived classes)
     * Most of these now delegate to YantraUtils or analyzers for maximum code reuse
     */
    virtual SorterOutput sort_impl(const Kakshya::DataVariant& data);
    virtual SorterOutput sort_impl(std::shared_ptr<Kakshya::SignalSourceContainer> container);
    virtual SorterOutput sort_impl(const Kakshya::Region& region);
    virtual SorterOutput sort_impl(const Kakshya::RegionGroup& group);
    virtual SorterOutput sort_impl(const std::vector<Kakshya::RegionSegment>& segments);
    virtual SorterOutput sort_impl(const AnalyzerOutput& output);
    virtual SorterOutput sort_impl(const Eigen::MatrixXd& matrix);
    virtual SorterOutput sort_impl(const Eigen::VectorXd& vector);
    virtual SorterOutput sort_impl(const std::vector<std::any>& data);

    /**
     * @brief Get available methods for specific type (to be overridden)
     */
    virtual std::vector<std::string> get_methods_for_type_impl(std::type_index type_info) const;

    /**
     * @brief Format output based on granularity setting
     */
    SorterOutput format_output_based_on_granularity(const SorterOutput& raw_output) const;

    /**
     * @brief Check flow enforcement constraints
     */
    virtual bool check_flow_constraints(const SorterInput& input) const;

    /**
     * @brief Get current sorting method from parameters
     */
    std::string get_sorting_method() const;

    // ===== Analyzer Delegation Methods =====

    /**
     * @brief Determine if sorting should be delegated to an analyzer
     * @param input Input value
     * @return True if analyzer should be used, false otherwise
     */
    bool should_use_analyzer(const auto& input) const;

    /**
     * @brief Delegate sorting to the configured analyzer
     * @param input Input value
     * @return SorterOutput from analyzer
     * @throws std::runtime_error if no analyzer is set
     */
    SorterOutput sort_via_analyzer(const auto& input);

    /**
     * @brief Convert sorter input to analyzer input
     * @param input Input value
     * @return AnalyzerInput for analyzer delegation
     */
    AnalyzerInput convert_to_analyzer_input(const auto& input);

    /**
     * @brief Convert AnalyzerOutput to SorterOutput
     * @param output AnalyzerOutput value
     * @return SorterOutput for further sorting
     */
    SorterOutput convert_from_analyzer_output(const AnalyzerOutput& output);

    /**
     * @brief Determine if a sorting method requires analyzer delegation
     * @param method Sorting method name
     * @return True if method should use analyzer
     */
    bool requires_analyzer_delegation(const std::string& method) const;

private:
    // Core configuration
    SortingGranularity m_granularity = SortingGranularity::SORTED_VALUES;
    SortingAlgorithm m_algorithm = SortingAlgorithm::STANDARD;
    SortDirection m_direction = SortDirection::ASCENDING;
    bool m_flow_enforcement = false; ///< Optional flow enforcement
    SortingGrammar m_grammar; ///< Grammar-based sorting rules
    std::map<std::string, std::any> m_parameters;

    // Analyzer delegation
    std::shared_ptr<UniversalAnalyzer> m_analyzer; ///< For complex sort key extraction
    bool m_use_analyzer = false; ///< Whether to delegate sorting to analyzer

    /**
     * @brief Visitor for type-safe dispatch
     */
    struct SorterVisitor {
        UniversalSorter* sorter;

        template <typename T>
        SorterOutput operator()(const T& data)
        {
            return sorter->sort_impl(data);
        }
    };
};

/**
 * @class TypedSorterWrapper
 * @brief Strongly-typed sorter wrapper for ComputeMatrix pipelines
 */
template <SorterInputType InputT, SorterOutputType OutputT>
class TypedSorterWrapper : public ComputeOperation<InputT, OutputT> {
public:
    TypedSorterWrapper(std::shared_ptr<UniversalSorter> sorter, const std::string& method = "default")
        : m_sorter(sorter)
        , m_method(method)
    {
    }

    OutputT apply_operation(InputT input) override
    {
        return m_sorter->sort_typed<InputT, OutputT>(input, m_method);
    }

    void set_parameter(const std::string& name, std::any value) override
    {
        m_sorter->set_parameter(name, value);
    }

    std::any get_parameter(const std::string& name) const override
    {
        return m_sorter->get_parameter(name);
    }

private:
    std::shared_ptr<UniversalSorter> m_sorter;
    std::string m_method;
};

/**
 * @brief Creates strongly-typed sorter wrappers for ComputeMatrix pipelines
 */
template <SorterInputType InputT, SorterOutputType OutputT>
std::shared_ptr<ComputeOperation<InputT, OutputT>>
create_typed_sorter(std::shared_ptr<UniversalSorter> sorter, const std::string& method = "default")
{
    return std::make_shared<TypedSorterWrapper<InputT, OutputT>>(sorter, method);
}

/**
 * @brief Creates a sorter with analyzer delegation enabled
 * @tparam SorterType The type of sorter to create
 * @param analyzer Shared pointer to the UniversalAnalyzer to use for delegation
 * @return Shared pointer to the created sorter with analyzer configured
 */
template <typename SorterType>
std::shared_ptr<SorterType> create_sorter_with_analyzer(std::shared_ptr<UniversalAnalyzer> analyzer)
{
    auto sorter = std::make_shared<SorterType>();
    sorter->set_analyzer(analyzer);
    sorter->set_use_analyzer(true);
    return sorter;
}

/**
 * @brief Registers sorter operations with a ComputeMatrix
 */
// void register_sorter_operations(std::shared_ptr<ComputeMatrix> matrix);

using DataToSortedData = TypedSorterWrapper<Kakshya::DataVariant, Kakshya::DataVariant>;
using SegmentsToSortedSegments = TypedSorterWrapper<std::vector<Kakshya::RegionSegment>, std::vector<Kakshya::RegionSegment>>;
using ValuesToSortedValues = TypedSorterWrapper<std::vector<double>, std::vector<double>>;
using MatrixToSortedMatrix = TypedSorterWrapper<Eigen::MatrixXd, Eigen::MatrixXd>;

} // namespace MayaFlux::Yantra
