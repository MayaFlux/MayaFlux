#pragma once

#include "ExtractionHelper.hpp"
#include "MayaFlux/Yantra/YantraUtils.hpp"

/**
 * @file UniversalExtractor.hpp
 * @brief Robust, extensible digital-first extraction framework for Maya Flux
 *
 * The UniversalExtractor system provides a modern, highly extensible foundation for feature extraction
 * in the Maya Flux ecosystem. It is designed for digital, data-driven workflows, supporting recursion,
 * analyzer integration, lazy evaluation, and grammar-based extraction. The framework is type-safe,
 * composable, and future-proof, enabling advanced extraction pipelines for multi-dimensional data.
 *
 * ## Key Features
 * - **Universal input/output:** Accepts and produces a wide range of data types via variants and concepts.
 * - **Analyzer integration:** Can delegate extraction to analyzers for advanced feature computation.
 * - **Recursion and feedback:** Supports recursive extraction and feedback via ExtractorNode trees.
 * - **Lazy evaluation:** Enables deferred computation for large or expensive extractions.
 * - **Grammar-based extraction:** Ready for rule-based, pattern-driven extraction workflows.
 * - **Type safety:** Uses C++20 concepts and std::variant for strict compile-time guarantees.
 * - **Parameterization:** Fully configurable at runtime via parameter maps.
 * - **Composable:** Designed for chaining, nesting, and integration with other Maya Flux components.
 *
 * ## Usage Overview
 * - Derive from UniversalExtractor to implement custom extraction logic.
 * - Override extract_impl methods for supported input types.
 * - Use set_analyzer() and set_use_analyzer() to enable analyzer delegation.
 * - Use ExtractorNode for recursive/lazy extraction patterns.
 * - Configure extraction via set_parameter() and get_parameter().
 */

namespace MayaFlux::Yantra {

/**
 * @class ConcreteExtractorNode
 * @brief Node that holds a concrete extraction result.
 *
 * Used for storing and retrieving a specific extraction result in the extraction tree.
 * Enables type-safe, value-based extraction nodes.
 *
 * @tparam T Type of the extraction result.
 */
template <typename T>
class ConcreteExtractorNode : public ExtractorNode {
public:
    /**
     * @brief Construct a ConcreteExtractorNode with a result.
     * @param result The extraction result to store.
     */
    ConcreteExtractorNode(T result)
        : m_result(std::move(result))
    {
    }

    /**
     * @brief Extract the stored result.
     * @return ExtractorOutput containing the stored result.
     */
    inline ExtractorOutput extract() override
    {
        return ExtractorOutput { m_result };
    }

    /**
     * @brief Get the type name of the stored result.
     * @return Demangled type name as a string.
     */
    inline std::string get_type_name() const override
    {
        return typeid(T).name();
    }

    /**
     * @brief Access the stored result.
     * @return Const reference to the result.
     */
    const T& get_result() const { return m_result; }

private:
    T m_result; ///< Stored extraction result.
};

/**
 * @class LazyExtractorNode
 * @brief Node that holds a function for lazy evaluation.
 *
 * Supports deferred computation of extraction results, caching the result after first evaluation.
 */
class LazyExtractorNode : public ExtractorNode {
public:
    /**
     * @brief Construct a LazyExtractorNode with an extraction function.
     * @param extractor_func Function that computes the extraction result.
     */
    LazyExtractorNode(std::function<ExtractorOutput()> extractor_func)
        : m_extractor_func(std::move(extractor_func))
    {
    }

    /**
     * @brief Extract the result, computing it if necessary.
     * @return Cached or newly computed ExtractorOutput.
     */
    inline ExtractorOutput extract() override
    {
        if (!m_cached_result.has_value()) {
            m_cached_result = m_extractor_func();
        }
        return *m_cached_result;
    }

    /**
     * @brief Get the type name of this node.
     * @return "LazyExtractorNode"
     */
    inline std::string get_type_name() const override
    {
        return "LazyExtractorNode";
    }

    /**
     * @brief Indicates that this node is lazy.
     * @return true
     */
    inline bool is_lazy() const override { return true; }

private:
    std::function<ExtractorOutput()> m_extractor_func; ///< Function for deferred extraction.
    std::optional<ExtractorOutput> m_cached_result; ///< Cached result after evaluation.
};

/**
 * @class RecursiveExtractorNode
 * @brief Node that can extract from other nodes recursively.
 *
 * Enables recursive extraction patterns, where the result depends on another node's output.
 */
class RecursiveExtractorNode : public ExtractorNode {
public:
    /**
     * @brief Construct a RecursiveExtractorNode.
     * @param extractor_func Function to perform extraction given input.
     * @param input_node Node providing input for the extraction.
     */
    RecursiveExtractorNode(std::function<ExtractorOutput(ExtractorInput)> extractor_func,
        std::shared_ptr<ExtractorNode> input_node)
        : m_extraction_func(std::move(extractor_func))
        , m_input_node(input_node)
    {
    }

    /**
     * @brief Extract the result recursively from the input node.
     * @return ExtractorOutput from recursive extraction.
     */
    ExtractorOutput extract() override;

    /**
     * @brief Get the type name of this node.
     * @return "RecursiveExtractorNode"
     */
    inline std::string get_type_name() const override
    {
        return "RecursiveExtractorNode";
    }

private:
    std::function<ExtractorOutput(ExtractorInput)> m_extraction_func; ///< Extraction function.
    std::shared_ptr<ExtractorNode> m_input_node; ///< Input node for recursion.
};

/**
 * @enum ExtractionStrategy
 * @brief Strategies for extraction execution.
 */
enum class ExtractionStrategy {
    IMMEDIATE, ///< Extract now (traditional, eager evaluation)
    LAZY, ///< Extract when accessed (deferred/lazy evaluation)
    RECURSIVE, ///< Extract based on previously extracted data (recursive)
    ANALYZER_DELEGATE ///< Delegate extraction to an analyzer
};

/**
 * @concept ExtractorInputType
 * @brief Concept for valid extractor input types.
 */
template <typename T>
concept ExtractorInputType = requires {
    std::holds_alternative<T>(std::declval<BaseExtractorInput>());
};

/**
 * @concept ExtractorOutputType
 * @brief Concept for valid extractor output types.
 */
template <typename T>
concept ExtractorOutputType = requires {
    std::holds_alternative<T>(std::declval<BaseExtractorOutput>());
};

/**
 * @class UniversalExtractor
 * @brief Modern, extensible extractor supporting analyzers, recursion, and advanced workflows.
 *
 * UniversalExtractor is the core base class for all feature extractors in Maya Flux.
 * It provides a unified interface for extraction, analyzer delegation, recursion, lazy evaluation,
 * and parameterization. Derived classes implement extraction logic for specific input types.
 *
 * ## Responsibilities
 * - Dispatch extraction based on input type.
 * - Integrate with analyzers for advanced feature computation.
 * - Support recursive and lazy extraction via ExtractorNode.
 * - Provide runtime configuration and method selection.
 * - Enable chaining and composition in extraction pipelines.
 */
class UniversalExtractor : public ComputeOperation<ExtractorInput, ExtractorOutput>, std::enable_shared_from_this<UniversalExtractor> {
public:
    /**
     * @brief Virtual destructor for safe polymorphic use.
     */
    virtual ~UniversalExtractor() = default;

    /**
     * @brief Main extraction method using ExtractorInput.
     *        Dispatches to appropriate extract_impl overload or analyzer if enabled.
     * @param input Extraction input (variant type).
     * @return ExtractorOutput containing the result.
     */
    ExtractorOutput apply_operation(ExtractorInput input) override;

    /**
     * @brief Type-safe extraction with specific input/output types.
     *        Sets extraction method and returns result of requested type.
     * @tparam InputT Input type (must satisfy ExtractorInputType).
     * @tparam OutputT Output type (must satisfy ExtractorOutputType).
     * @param input Input value.
     * @param method Extraction method name (optional).
     * @return Extracted result of type OutputT.
     * @throws std::runtime_error if result type does not match OutputT.
     */
    template <ExtractorInputType InputT, ExtractorOutputType OutputT>
    OutputT extract_typed(const InputT& input, const std::string& method = "default")
    {
        set_extraction_method(method);

        ExtractorInput extractor_input { input };
        auto result = apply_operation(extractor_input);

        if (auto* typed_result = std::get_if<OutputT>(&result.base_output)) {
            return *typed_result;
        }

        throw std::runtime_error("Extraction result type mismatch");
    }

    /**
     * @brief Perform extraction using a specified strategy.
     * @param input Extraction input.
     * @param strategy Extraction strategy (immediate, lazy, recursive, analyzer).
     * @return ExtractorOutput containing the result.
     */
    ExtractorOutput extract_with_strategy(ExtractorInput input, ExtractionStrategy strategy);

    /**
     * @brief Set the analyzer to use for delegation.
     * @param analyzer Shared pointer to a UniversalAnalyzer.
     */
    inline void set_analyzer(std::shared_ptr<UniversalAnalyzer> analyzer) { m_analyzer = analyzer; }

    /**
     * @brief Enable or disable analyzer delegation.
     * @param use True to enable, false to disable.
     */
    inline void set_use_analyzer(bool use) { m_use_analyzer = use; }

    /**
     * @brief Check if analyzer delegation is enabled and analyzer is set.
     * @return True if using analyzer, false otherwise.
     */
    inline bool uses_analyzer() const { return m_use_analyzer && m_analyzer != nullptr; }

    /**
     * @brief Get the list of available extraction methods for this extractor.
     * @return Vector of method names.
     */
    virtual std::vector<std::string> get_available_methods() const = 0;

    /**
     * @brief Get supported extraction methods for a specific input type.
     * @tparam T Input type.
     * @return Vector of method names.
     */
    template <ExtractorInputType T>
    std::vector<std::string> get_methods_for_type() const
    {
        return get_methods_for_type_impl(std::type_index(typeid(T)));
    }

    /**
     * @brief Set the extraction method to use.
     * @param method Method name.
     */
    inline void set_extraction_method(const std::string& method) { set_parameter("method", method); }

    /**
     * @brief Get the currently configured extraction method.
     * @return Method name as a string.
     */
    std::string get_extraction_method() const;

    /**
     * @brief Create a concrete extractor node for a result.
     * @tparam T Result type.
     * @param result Extraction result.
     * @return Shared pointer to ConcreteExtractorNode.
     */
    template <typename T>
    std::shared_ptr<ExtractorNode> create_node(T result)
    {
        return std::make_shared<ConcreteExtractorNode<T>>(std::move(result));
    }

    /**
     * @brief Create a lazy extractor node for deferred evaluation.
     * @param func Function to compute the extraction result.
     * @return Shared pointer to LazyExtractorNode.
     */
    inline std::shared_ptr<ExtractorNode> create_lazy_node(std::function<ExtractorOutput()> func)
    {
        return std::make_shared<LazyExtractorNode>(std::move(func));
    }

    /**
     * @brief Create a recursive extractor node.
     * @param input_node Input node to extract from recursively.
     * @return Shared pointer to RecursiveExtractorNode.
     */
    inline std::shared_ptr<ExtractorNode> create_recursive_node(std::shared_ptr<ExtractorNode> input_node)
    {
        auto extraction_func = [this](ExtractorInput input) -> ExtractorOutput {
            return this->apply_operation(input);
        };

        return std::make_shared<RecursiveExtractorNode>(extraction_func, input_node);
    }

protected:
    // ===== Virtual Implementation Methods =====

    /**
     * @brief Extract features from a Kakshya::DataVariant input.
     * @param data Input data variant.
     * @return ExtractorOutput containing the result.
     * @throws std::runtime_error if not implemented in derived class.
     */
    virtual ExtractorOutput extract_impl(const Kakshya::DataVariant& data)
    {
        throw std::runtime_error("DataVariant extraction not implemented");
    }

    /**
     * @brief Extract features from a SignalSourceContainer.
     * @param container Shared pointer to signal container.
     * @return ExtractorOutput containing the result.
     * @throws std::runtime_error if not implemented in derived class.
     */
    virtual ExtractorOutput extract_impl(std::shared_ptr<Kakshya::SignalSourceContainer> container)
    {
        throw std::runtime_error("Container extraction not implemented");
    }

    /**
     * @brief Extract features from a Region.
     * @param region Region of interest.
     * @return ExtractorOutput containing the result.
     * @throws std::runtime_error if not implemented in derived class.
     */
    virtual ExtractorOutput extract_impl(const Kakshya::Region& region)
    {
        throw std::runtime_error("Region extraction not implemented");
    }

    /**
     * @brief Extract features from a RegionGroup.
     * @param group Group of regions.
     * @return ExtractorOutput containing the result.
     * @throws std::runtime_error if not implemented in derived class.
     */
    virtual ExtractorOutput extract_impl(const Kakshya::RegionGroup& group)
    {
        throw std::runtime_error("RegionGroup extraction not implemented");
    }

    /**
     * @brief Extract features from a list of RegionSegments.
     * @param segments List of region segments.
     * @return ExtractorOutput containing the result.
     * @throws std::runtime_error if not implemented in derived class.
     */
    virtual ExtractorOutput extract_impl(const std::vector<Kakshya::RegionSegment>& segments)
    {
        throw std::runtime_error("RegionSegment extraction not implemented");
    }

    /**
     * @brief Extract features from an AnalyzerOutput.
     * @param analyzer_output Output from an analyzer.
     * @return ExtractorOutput containing the result.
     * @throws std::runtime_error if not implemented in derived class.
     */
    virtual ExtractorOutput extract_impl(const AnalyzerOutput& analyzer_output)
    {
        throw std::runtime_error("AnalyzerOutput extraction not implemented");
    }

    /**
     * @brief Get supported extraction methods for a given type.
     * @param type_info Type index of the input.
     * @return Vector of method names.
     */
    virtual std::vector<std::string> get_methods_for_type_impl(std::type_index type_info) const = 0;

    /**
     * @brief Create a lazy extraction result for deferred evaluation.
     * @param input Extraction input.
     * @return ExtractorOutput containing a lazy node.
     */
    virtual ExtractorOutput create_lazy_extraction(ExtractorInput input);

    /**
     * @brief Perform recursive extraction using input's recursive nodes.
     * @param input Extraction input.
     * @return ExtractorOutput with recursive results.
     */
    virtual ExtractorOutput extract_recursive(ExtractorInput input);

    /**
     * @brief Perform extraction with recursive inputs, combining results.
     * @param input Extraction input with recursive nodes.
     * @return Combined ExtractorOutput.
     */
    virtual ExtractorOutput extract_with_recursive_inputs(ExtractorInput input);

    /**
     * @brief Combine base and recursive extraction results.
     * @param base_result Result from base extraction.
     * @param recursive_results Results from recursive nodes.
     * @return Combined ExtractorOutput (default: base_result).
     */
    inline virtual ExtractorOutput combine_results(const ExtractorOutput& base_result,
        const std::vector<ExtractorOutput>& recursive_results)
    {
        // Default: just return base result
        // Derived classes can override for specific combination logic
        return base_result;
    }

    /**
     * @brief Determine if extraction should be delegated to an analyzer.
     * @tparam T Input type.
     * @param input Input value.
     * @return True if analyzer should be used, false otherwise.
     */
    inline bool should_use_analyzer(const auto& input) const
    {
        return m_use_analyzer && m_analyzer != nullptr;
    }

    /**
     * @brief Delegate extraction to the configured analyzer.
     * @tparam T Input type.
     * @param input Input value.
     * @return ExtractorOutput from analyzer.
     * @throws std::runtime_error if no analyzer is set.
     */
    ExtractorOutput extract_via_analyzer(const auto& input);

    /**
     * @brief Perform extraction using analyzer strategy.
     * @param input Extraction input.
     * @return ExtractorOutput from analyzer.
     */
    virtual ExtractorOutput extract_via_analyzer_strategy(ExtractorInput input);

    /**
     * @brief Convert an extractor input to an analyzer input.
     * @tparam T Input type.
     * @param input Input value.
     * @return AnalyzerInput for analyzer delegation.
     */
    AnalyzerInput convert_to_analyzer_input(const auto& input);

    /**
     * @brief Convert an AnalyzerOutput to an ExtractorOutput.
     * @param output AnalyzerOutput value.
     * @return ExtractorOutput for further extraction.
     */
    ExtractorOutput convert_from_analyzer_output(const AnalyzerOutput& output);

public:
    /**
     * @brief Set a named parameter for extraction configuration.
     * @param name Parameter name.
     * @param value Parameter value (std::any).
     */
    inline void set_parameter(const std::string& name, std::any value) override
    {
        m_parameters[name] = value;
    }

    /**
     * @brief Get the value of a named parameter.
     * @param name Parameter name.
     * @return Parameter value (std::any).
     */
    inline std::any get_parameter(const std::string& name) const override
    {
        return Yantra::safe_get_parameter(name, m_parameters);
    }

    /**
     * @brief Get all configured parameters.
     * @return Map of parameter names to values.
     */
    inline std::map<std::string, std::any> get_all_parameters() const override
    {
        return m_parameters;
    }

private:
    std::map<std::string, std::any> m_parameters; ///< Extraction parameters.
    std::shared_ptr<UniversalAnalyzer> m_analyzer; ///< Optional analyzer for delegation.
    bool m_use_analyzer = false; ///< Whether to delegate extraction to analyzer.
};

}
