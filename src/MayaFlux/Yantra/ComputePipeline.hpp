#pragma once

#include "ComputeGrammar.hpp"
#include "ComputeMatrix.hpp"

namespace MayaFlux::Yantra {

/**
 * @class ComputationPipeline
 * @brief Pipeline that uses grammar rules for operation composition
 */
template <ComputeData InputType = Kakshya::DataVariant, ComputeData OutputType = InputType>
class ComputationPipeline {
public:
    using input_type = IO<InputType>;
    using output_type = IO<OutputType>;

    /**
     * @brief Constructor with optional grammar
     */
    explicit ComputationPipeline(std::shared_ptr<ComputationGrammar> grammar = nullptr)
        : m_grammar(grammar ? std::move(grammar) : std::make_shared<ComputationGrammar>())
    {
    }

    /**
     * @brief Add a concrete operation instance to the pipeline
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
     */
    template <typename ConcreteOpType, typename... Args>
    ComputationPipeline& create_operation(const std::string& name = "", Args&&... args)
    {
        auto operation = std::make_shared<ConcreteOpType>(std::forward<Args>(args)...);
        return add_operation(operation, name);
    }

    /**
     * @brief Execute the pipeline with grammar rule application
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
     */
    [[nodiscard]] std::shared_ptr<ComputationGrammar> get_grammar() const
    {
        return m_grammar;
    }

    /**
     * @brief Set grammar instance
     */
    void set_grammar(std::shared_ptr<ComputationGrammar> grammar)
    {
        m_grammar = std::move(grammar);
    }

    /**
     * @brief Get operation by name
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
     */
    [[nodiscard]] size_t operation_count() const
    {
        return m_operations.size();
    }

    /**
     * @brief Clear all operations
     */
    void clear_operations()
    {
        m_operations.clear();
    }

private:
    std::shared_ptr<ComputationGrammar> m_grammar;
    std::vector<std::pair<std::shared_ptr<ComputeOperation<InputType, OutputType>>, std::string>> m_operations;
};

/**
 * @brief Factory functions for common pipeline configurations
 */
namespace PipelineFactory {

    /**
     * @brief Create an audio processing pipeline
     */
    template <ComputeData DataType = Kakshya::DataVariant>
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
     */
    template <ComputeData DataType = Kakshya::DataVariant>
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
 */
class GrammarAwareComputeMatrix : public ComputeMatrix {
private:
    std::shared_ptr<ComputationGrammar> m_grammar;

public:
    explicit GrammarAwareComputeMatrix(std::shared_ptr<ComputationGrammar> grammar = nullptr)
        : m_grammar(grammar ? std::move(grammar) : std::make_shared<ComputationGrammar>())
    {
    }

    /**
     * @brief Execute operations with grammar rule pre-processing
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
                    // Continue with original data
                }
            }
        }

        return input_data;
    }

    /**
     * @brief Get the grammar instance
     */
    std::shared_ptr<ComputationGrammar> get_grammar() const
    {
        return m_grammar;
    }

    /**
     * @brief Set grammar instance
     */
    void set_grammar(std::shared_ptr<ComputationGrammar> grammar)
    {
        m_grammar = std::move(grammar);
    }
};

} // namespace MayaFlux::Yantra
