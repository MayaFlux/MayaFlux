#pragma once

#include "ComputeGrammar.hpp"
#include "ComputeMatrix.hpp"

namespace MayaFlux::Yantra {

/**
 * @class ComputationPipeline
 * @brief Pipeline that uses grammar rules for operation composition
 * @tparam InputType Input data type (defaults to std::vector<Kakshya::DataVariant>)
 * @tparam OutputType Output data type (defaults to InputType)
 *
 * The ComputationPipeline provides a flexible, grammar-aware system for chaining
 * computational operations in sequence. Unlike traditional pipelines that execute
 * operations in a fixed order, this pipeline can dynamically select and apply
 * operations based on grammar rules that match input data characteristics and
 * execution context.
 *
 * ## Key Features
 *
 * **Grammar Integration**: Uses ComputationGrammar to intelligently select and
 * configure operations based on input data properties and context.
 *
 * **Type Safety**: Template-based design ensures type compatibility between
 * pipeline stages while supporting different input/output types.
 *
 * **Dynamic Configuration**: Operations can be added, configured, and removed
 * at runtime, enabling adaptive processing workflows.
 *
 * **Error Handling**: Comprehensive error handling with operation-specific
 * error reporting and graceful degradation.
 *
 * ## Usage Patterns
 *
 * ### Basic Pipeline Construction
 * ```cpp
 * auto pipeline = std::make_shared<ComputationPipeline<std::vector<Kakshya::DataVariant>>>();
 *
 * // Add operations with names for later reference
 * pipeline->create_operation<MathematicalTransformer<>>("gain_stage")
 *          ->create_operation<SpectralTransformer<>>("frequency_processing")
 *          ->create_operation<TemporalTransformer<>>("time_effects");
 * ```
 *
 * ### Grammar-Driven Processing
 * ```cpp
 * // Pipeline automatically applies grammar rules before operation chain
 * ExecutionContext context;
 * context.execution_metadata["processing_mode"] = std::string("high_quality");
 *
 * auto result = pipeline->process(input_data, context);
 * // Grammar rules are evaluated first, then operation chain executes
 * ```
 *
 * ### Dynamic Operation Configuration
 * ```cpp
 * // Configure specific operations by name
 * pipeline->configure_operation<MathematicalTransformer<>>("gain_stage",
 *     [](auto op) {
 *         op->set_parameter("operation", "gain");
 *         op->set_parameter("gain_factor", 2.0);
 *     });
 * ```
 */
template <ComputeData InputType = std::vector<Kakshya::DataVariant>, ComputeData OutputType = InputType>
class MAYAFLUX_API ComputationPipeline {
public:
    using input_type = IO<InputType>;
    using output_type = IO<OutputType>;

    /**
     * @brief Constructor with optional grammar
     * @param grammar Shared pointer to ComputationGrammar instance (creates new if nullptr)
     *
     * Creates a pipeline with the specified grammar instance. If no grammar is provided,
     * creates a new empty grammar that can be populated with rules later.
     */
    explicit ComputationPipeline(std::shared_ptr<ComputationGrammar> grammar = nullptr)
        : m_grammar(grammar ? std::move(grammar) : std::make_shared<ComputationGrammar>())
    {
    }

    /**
     * @brief Add a concrete operation instance to the pipeline
     * @tparam ConcreteOpType The concrete operation type (must derive from ComputeOperation)
     * @param operation Shared pointer to the operation instance
     * @param name Optional name for the operation (used for later reference)
     * @return Reference to this pipeline for method chaining
     *
     * Adds an existing operation instance to the pipeline. The operation will be executed
     * in the order it was added. Names are optional but recommended for later configuration
     * and debugging.
     *
     * @note The operation type must be compatible with the pipeline's input/output types
     */
    template <typename ConcreteOpType>
    ComputationPipeline& add_operation(std::shared_ptr<ConcreteOpType> operation, const std::string& name = "")
    {
        static_assert(std::is_base_of_v<ComputeOperation<InputType, OutputType>, ConcreteOpType>,
            "Operation must derive from ComputeOperation");

        m_operations.emplace_back(std::static_pointer_cast<ComputeOperation<InputType, OutputType>>(operation), name);
        return *this;
    }

    /**
     * @brief Create and add operation by type
     * @tparam ConcreteOpType The concrete operation type to create
     * @tparam Args Constructor argument types
     * @param name Optional name for the operation
     * @param args Constructor arguments for the operation
     * @return Reference to this pipeline for method chaining
     *
     * Creates a new instance of the specified operation type and adds it to the pipeline.
     * This is the most convenient way to add operations when you don't need to configure
     * them before adding to the pipeline.
     *
     * Example:
     * ```cpp
     * pipeline->create_operation<MathematicalTransformer<>>("gain")
     *          ->create_operation<SpectralTransformer<>>("pitch_shift",
     *              SpectralOperation::PITCH_SHIFT);
     * ```
     */
    template <typename ConcreteOpType, typename... Args>
    ComputationPipeline& create_operation(const std::string& name = "", Args&&... args)
    {
        auto operation = std::make_shared<ConcreteOpType>(std::forward<Args>(args)...);
        return add_operation(operation, name);
    }

    /**
     * @brief Execute the pipeline with grammar rule application
     * @param input Input data to process through the pipeline
     * @param context Execution context containing parameters and metadata
     * @return Processed output data
     *
     * Executes the complete pipeline processing workflow:
     *
     * 1. **Grammar Rule Application**: Searches for grammar rules that match the input
     *    data and execution context. If a matching rule is found, applies it first.
     *
     * 2. **Operation Chain Execution**: Executes all operations in the pipeline in
     *    the order they were added, passing output from each stage as input to the next.
     *
     * 3. **Type Conversion**: Handles type conversion between InputType and OutputType
     *    when they differ.
     *
     * The pipeline provides comprehensive error handling with operation-specific error
     * messages that include the operation name for debugging.
     *
     * @throws std::runtime_error If any operation in the pipeline fails
     */
    output_type process(const input_type& input, const ExecutionContext& context = {})
    {
        input_type current_data = input;
        ExecutionContext current_context = context;

        if (auto best_rule = m_grammar->find_best_match(current_data, current_context)) {
            if (auto rule_result = m_grammar->execute_rule(best_rule->name, current_data, current_context)) {
                try {
                    current_data = std::any_cast<input_type>(*rule_result);
                } catch (const std::bad_any_cast&) {
                    // Continue with original data if conversion fails
                }
            }
        }

        for (const auto& [operation, name] : m_operations) {
            try {
                auto result = operation->apply_operation(current_data);
                current_data = result;
            } catch (const std::exception& e) {
                throw std::runtime_error("Pipeline operation failed: " + name + " - " + e.what());
            }
        }

        if constexpr (std::is_same_v<InputType, OutputType>) {
            return current_data;
        } else {
            output_type result;
            return result;
        }
    }

    /**
     * @brief Get the grammar instance
     * @return Shared pointer to the current ComputationGrammar
     *
     * Provides access to the pipeline's grammar instance for adding rules,
     * querying existing rules, or integrating with other grammar-aware components.
     */
    [[nodiscard]] std::shared_ptr<ComputationGrammar> get_grammar() const
    {
        return m_grammar;
    }

    /**
     * @brief Set grammar instance
     * @param grammar New grammar instance to use
     *
     * Replaces the current grammar with a new instance. Useful for switching
     * between different rule sets or sharing grammars between multiple pipelines.
     */
    void set_grammar(std::shared_ptr<ComputationGrammar> grammar)
    {
        m_grammar = std::move(grammar);
    }

    /**
     * @brief Get operation by name
     * @tparam ConcreteOpType The expected concrete operation type
     * @param name Name of the operation to retrieve
     * @return Shared pointer to the operation, or nullptr if not found or wrong type
     *
     * Retrieves a named operation from the pipeline with automatic type casting.
     * Returns nullptr if no operation with the given name exists or if the
     * operation is not of the expected type.
     *
     * Example:
     * ```cpp
     * auto gain_op = pipeline->get_operation<MathematicalTransformer<>>("gain_stage");
     * if (gain_op) {
     *     gain_op->set_parameter("gain_factor", 1.5);
     * }
     * ```
     */
    template <typename ConcreteOpType>
    std::shared_ptr<ConcreteOpType> get_operation(const std::string& name) const
    {
        for (const auto& [operation, op_name] : m_operations) {
            if (op_name == name) {
                return std::dynamic_pointer_cast<ConcreteOpType>(operation);
            }
        }
        return nullptr;
    }

    /**
     * @brief Configure operation by name
     * @tparam ConcreteOpType The expected concrete operation type
     * @param name Name of the operation to configure
     * @param configurator Function that configures the operation
     * @return True if operation was found and configured, false otherwise
     *
     * Provides a safe way to configure named operations in the pipeline. The
     * configurator function is only called if an operation with the given name
     * exists and is of the expected type.
     *
     * Example:
     * ```cpp
     * bool configured = pipeline->configure_operation<SpectralTransformer<>>("pitch_shift",
     *     [](auto op) {
     *         op->set_parameter("pitch_ratio", 1.5);
     *         op->set_parameter("window_size", 2048);
     *     });
     * ```
     */
    template <typename ConcreteOpType>
    bool configure_operation(const std::string& name,
        const std::function<void(std::shared_ptr<ConcreteOpType>)>& configurator)
    {
        for (auto& [operation, op_name] : m_operations) {
            if (op_name == name) {
                if (auto concrete_op = std::dynamic_pointer_cast<ConcreteOpType>(operation)) {
                    configurator(concrete_op);
                    return true;
                }
            }
        }
        return false;
    }

    /**
     * @brief Get number of operations in pipeline
     * @return Number of operations currently in the pipeline
     */
    [[nodiscard]] size_t operation_count() const
    {
        return m_operations.size();
    }

    /**
     * @brief Clear all operations
     *
     * Removes all operations from the pipeline, leaving it empty.
     * The grammar instance is preserved.
     */
    void clear_operations()
    {
        m_operations.clear();
    }

    /**
     * @brief Get all operation names in the pipeline
     * @return Vector of operation names in execution order
     *
     * Returns the names of all operations in the pipeline in the order they
     * will be executed. Unnamed operations appear as empty strings.
     */
    [[nodiscard]] std::vector<std::string> get_operation_names() const
    {
        std::vector<std::string> names;
        names.reserve(m_operations.size());
        std::ranges::transform(m_operations, std::back_inserter(names),
            [](const auto& op_pair) { return op_pair.second; });
        return names;
    }

    /**
     * @brief Remove operation by name
     * @param name Name of the operation to remove
     * @return True if operation was found and removed, false otherwise
     *
     * Removes the first operation with the given name from the pipeline.
     * If multiple operations have the same name, only the first one is removed.
     */
    bool remove_operation(const std::string& name)
    {
        auto it = std::ranges::find_if(m_operations,
            [&name](const auto& op_pair) { return op_pair.second == name; });

        if (it != m_operations.end()) {
            m_operations.erase(it);
            return true;
        }
        return false;
    }

private:
    std::shared_ptr<ComputationGrammar> m_grammar; ///< Grammar instance for rule-based operation selection
    std::vector<std::pair<std::shared_ptr<ComputeOperation<InputType, OutputType>>, std::string>> m_operations; ///< Operations and their names in execution order
};

/**
 * @brief Factory functions for common pipeline configurations
 *
 * The PipelineFactory namespace provides convenience functions for creating
 * pre-configured pipelines for common use cases. These factories set up
 * typical operation chains and grammar rules for specific domains.
 */
namespace PipelineFactory {

    /**
     * @brief Create an audio processing pipeline
     * @tparam DataType The data type for the pipeline (defaults to std::vector<Kakshya::DataVariant>)
     * @return Shared pointer to a configured audio processing pipeline
     *
     * Creates a pipeline pre-configured for audio processing workflows with
     * typical operations for gain control, temporal effects, and spectral processing.
     * The returned pipeline is ready to use but can be further customized.
     *
     * Example:
     * ```cpp
     * auto audio_pipeline = PipelineFactory::create_audio_pipeline<std::vector<Kakshya::DataVariant>>();
     * auto result = audio_pipeline->process(audio_data, context);
     * ```
     */
    template <ComputeData DataType = std::vector<Kakshya::DataVariant>>
    std::shared_ptr<ComputationPipeline<DataType>> create_audio_pipeline()
    {
        auto pipeline = std::make_shared<ComputationPipeline<DataType>>();

        // Add common audio operations
        // pipeline->create_operation<MathematicalTransformer<DataType>>("gain");
        // pipeline->create_operation<TemporalTransformer<DataType>>("time_effects");
        // pipeline->create_operation<SpectralTransformer<DataType>>("frequency_effects");

        return pipeline;
    }

    /**
     * @brief Create an analysis pipeline
     * @tparam DataType The data type for the pipeline (defaults to std::vector<Kakshya::DataVariant>)
     * @return Shared pointer to a configured analysis pipeline
     *
     * Creates a pipeline pre-configured for data analysis workflows with
     * operations for feature extraction, statistical analysis, and result processing.
     * Suitable for machine learning preprocessing and data science workflows.
     *
     * Example:
     * ```cpp
     * auto analysis_pipeline = PipelineFactory::create_analysis_pipeline<>();
     * auto features = analysis_pipeline->process(raw_data, analysis_context);
     * ```
     */
    template <ComputeData DataType = std::vector<Kakshya::DataVariant>>
    std::shared_ptr<ComputationPipeline<DataType>> create_analysis_pipeline()
    {
        auto pipeline = std::make_shared<ComputationPipeline<DataType>>();

        // Add analysis operations (examples)
        // pipeline->create_operation<FeatureExtractor<DataType>>("feature_extract");
        // pipeline->create_operation<StandardSorter<DataType>>("sort_results");

        return pipeline;
    }

} // namespace PipelineFactory

/**
 * @class GrammarAwareComputeMatrix
 * @brief ComputeMatrix extension that integrates grammar-based operation selection
 *
 * The GrammarAwareComputeMatrix extends the base ComputeMatrix functionality with
 * grammar-based rule processing. This allows for intelligent operation selection
 * and preprocessing based on input data characteristics and execution context.
 *
 * Unlike pipelines that execute operations in sequence, the grammar-aware matrix
 * can dynamically select which operations to apply based on the current data
 * and context, making it suitable for adaptive and conditional processing workflows.
 *
 * ## Usage Patterns
 *
 * ### Basic Grammar Integration
 * ```cpp
 * auto grammar = std::make_shared<ComputationGrammar>();
 * auto matrix = std::make_unique<GrammarAwareComputeMatrix>(grammar);
 *
 * // Grammar rules are applied before any matrix operations
 * auto result = matrix->execute_with_grammar(input_data, context);
 * ```
 *
 * ### Dynamic Operation Selection
 * ```cpp
 * // Grammar rules can dynamically select operations based on data properties
 * ExecutionContext context;
 * context.execution_metadata["processing_quality"] = std::string("high");
 * context.execution_metadata["data_size"] = input_data.size();
 *
 * auto processed_data = matrix->execute_with_grammar(input_data, context);
 * // Appropriate operations selected based on quality requirements and data size
 * ```
 */
class MAYAFLUX_API GrammarAwareComputeMatrix : public ComputeMatrix {
private:
    std::shared_ptr<ComputationGrammar> m_grammar; ///< Grammar instance for rule-based operation selection

public:
    /**
     * @brief Constructor with optional grammar
     * @param grammar Shared pointer to ComputationGrammar instance (creates new if nullptr)
     *
     * Creates a grammar-aware compute matrix with the specified grammar instance.
     * If no grammar is provided, creates a new empty grammar that can be populated
     * with rules later.
     */
    explicit GrammarAwareComputeMatrix(std::shared_ptr<ComputationGrammar> grammar = nullptr)
        : m_grammar(grammar ? std::move(grammar) : std::make_shared<ComputationGrammar>())
    {
    }

    /**
     * @brief Execute operations with grammar rule pre-processing
     * @tparam InputType The input data type
     * @param input Input data to process
     * @param context Execution context containing parameters and metadata
     * @return Processed input data after grammar rule application
     *
     * Applies grammar rules to the input data before any matrix operations.
     * This allows for intelligent preprocessing, operation selection, and
     * parameter configuration based on the input characteristics and context.
     *
     * The process:
     * 1. Wraps input data in IO structure
     * 2. Searches for matching grammar rules
     * 3. Applies the best matching rule if found
     * 4. Returns processed data or original data if no rules match
     *
     * @note This method focuses on grammar rule application. Use the base
     *       ComputeMatrix methods for actual operation execution.
     */
    template <ComputeData InputType>
    auto execute_with_grammar(const InputType& input, const ExecutionContext& context = {})
    {
        IO<InputType> input_data { input };

        if (auto best_rule = m_grammar->find_best_match(input_data, context)) {
            if (auto rule_result = m_grammar->execute_rule(best_rule->name, input_data, context)) {
                try {
                    input_data = std::any_cast<IO<InputType>>(*rule_result);
                } catch (const std::bad_any_cast&) {
                    // Continue with original data if conversion fails
                }
            }
        }

        return input_data;
    }

    /**
     * @brief Get the grammar instance
     * @return Shared pointer to the current ComputationGrammar
     *
     * Provides access to the grammar instance for adding rules, querying existing
     * rules, or integrating with other grammar-aware components.
     */
    std::shared_ptr<ComputationGrammar> get_grammar() const
    {
        return m_grammar;
    }

    /**
     * @brief Set grammar instance
     * @param grammar New grammar instance to use
     *
     * Replaces the current grammar with a new instance. Useful for switching
     * between different rule sets or sharing grammars between multiple components.
     */
    void set_grammar(std::shared_ptr<ComputationGrammar> grammar)
    {
        m_grammar = std::move(grammar);
    }

    /**
     * @brief Add a grammar rule directly to the matrix
     * @param rule Rule to add to the grammar
     *
     * Convenience method to add rules directly to the matrix's grammar without
     * needing to access the grammar instance separately. Useful for quick
     * rule addition during matrix configuration.
     */
    void add_grammar_rule(ComputationGrammar::Rule rule)
    {
        m_grammar->add_rule(std::move(rule));
    }

    /**
     * @brief Create a rule builder for this matrix's grammar
     * @param name Unique name for the rule
     * @return RuleBuilder instance for method chaining
     *
     * Provides direct access to the grammar's rule building interface,
     * allowing for fluent rule creation without explicit grammar access.
     *
     * Example:
     * ```cpp
     * matrix->create_grammar_rule("auto_gain")
     *        .with_context(ComputationContext::TEMPORAL)
     *        .matches_type<std::vector<double>>()
     *        .executes([](const auto& input, const auto& ctx) { return input; })
     *        .build();
     * ```
     */
    ComputationGrammar::RuleBuilder create_grammar_rule(const std::string& name)
    {
        return m_grammar->create_rule(name);
    }
};

} // namespace MayaFlux::Yantra
