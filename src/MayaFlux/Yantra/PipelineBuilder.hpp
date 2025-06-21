#pragma once

#include "ComputeMatrix.hpp"

namespace MayaFlux::Yantra {

/**
 * @class PipelineBuilder
 * @brief Fluent interface for building complex operation pipelines
 *
 * Replaces the ChainBuilder with a more flexible system that:
 * - Handles cross-type conversions automatically
 * - Supports branching and merging
 * - Works with the variant system
 * - Avoids circular dependencies
 */
class PipelineBuilder {
public:
    /**
     * @brief Pipeline stage representation
     */
    struct Stage {
        std::string operation_name;
        std::string stage_name;
        std::function<UniversalOutput(UniversalInput)> executor;

        Stage(const std::string& op_name, const std::string& name = "")
            : operation_name(op_name)
            , stage_name(name.empty() ? op_name : name)
        {
        }
    };

    /**
     * @brief Construct with a reference to the compute matrix
     */
    explicit PipelineBuilder(std::shared_ptr<ComputeMatrix> matrix)
        : m_matrix(matrix)
    {
    }

    /**
     * @brief Add a stage to the pipeline
     */
    PipelineBuilder& add_stage(const std::string& operation_name, const std::string& stage_name = "")
    {
        m_stages.emplace_back(operation_name, stage_name);
        return *this;
    }

    /**
     * @brief Add multiple stages at once
     */
    PipelineBuilder& add_stages(const std::vector<std::string>& operation_names)
    {
        for (const auto& name : operation_names) {
            add_stage(name);
        }
        return *this;
    }

    /**
     * @brief Add a custom processing stage with lambda
     */
    PipelineBuilder& add_custom_stage(const std::string& stage_name,
        std::function<UniversalOutput(UniversalInput)> processor)
    {
        Stage custom_stage("", stage_name);
        custom_stage.executor = processor;
        m_stages.push_back(custom_stage);
        return *this;
    }

    /**
     * @brief Build and return the complete pipeline as a callable
     */
    std::function<UniversalOutput(UniversalInput)> build()
    {
        if (m_stages.empty()) {
            throw std::runtime_error("Pipeline is empty");
        }

        auto stages = m_stages;
        auto matrix = m_matrix;

        return [stages, matrix, this](UniversalInput input) -> UniversalOutput {
            UniversalInput current_input = input;
            UniversalOutput current_output;

            for (const auto& stage : stages) {
                if (stage.executor) {
                    current_output = stage.executor(current_input);
                } else {
                    current_output = matrix->apply_operation(stage.operation_name, current_input);
                }

                if (&stage != &stages.back()) {
                    current_input = convert_output_to_input(current_output);
                }
            }

            return current_output;
        };
    }

    /**
     * @brief Build a reusable pipeline object
     */
    class Pipeline {
    public:
        Pipeline(std::function<UniversalOutput(UniversalInput)> executor,
            std::vector<std::string> stage_names)
            : m_executor(executor)
            , m_stage_names(stage_names)
        {
        }

        UniversalOutput execute(UniversalInput input)
        {
            return m_executor(input);
        }

        const std::vector<std::string>& get_stage_names() const
        {
            return m_stage_names;
        }

    private:
        std::function<UniversalOutput(UniversalInput)> m_executor;
        std::vector<std::string> m_stage_names;
    };

    /**
     * @brief Build a reusable pipeline object
     */
    Pipeline build_pipeline()
    {
        auto executor = build();

        std::vector<std::string> stage_names;
        for (const auto& stage : m_stages) {
            stage_names.push_back(stage.stage_name);
        }

        return Pipeline(executor, stage_names);
    }

    /**
     * @brief Get stage names for debugging
     */
    std::vector<std::string> get_stage_names() const
    {
        std::vector<std::string> names;
        for (const auto& stage : m_stages) {
            names.push_back(stage.stage_name);
        }
        return names;
    }

    /**
     * @brief Clear all stages
     */
    PipelineBuilder& clear()
    {
        m_stages.clear();
        return *this;
    }

private:
    std::shared_ptr<ComputeMatrix> m_matrix;
    std::vector<Stage> m_stages;

    /**
     * @brief Add a stage with explicit input/output type conversion
     * Use this when automatic conversion might be ambiguous
     */
    template <typename TargetInput>
    PipelineBuilder& add_stage_with_conversion(const std::string& operation_name, const std::string& stage_name = "")
    {
        Stage stage(operation_name, stage_name.empty() ? operation_name : stage_name);

        stage.executor = [operation_name, this](UniversalInput input) -> UniversalOutput {
            auto output = m_matrix->apply_operation(operation_name, input);
            return output;
        };

        m_stages.push_back(stage);
        return *this;
    }

    /**
     * @brief Specify the conversion strategy for the pipeline
     */
    enum class ConversionStrategy {
        ANALYZER_TO_EXTRACTOR, /// AnalyzerOutput -> ExtractorInput
        ANALYZER_TO_SORTER, /// AnalyzerOutput -> SorterInput
        EXTRACTOR_TO_SORTER, /// ExtractorOutput -> SorterInput (via compatible base types)
        SORTER_TO_EXTRACTOR, /// SorterOutput -> ExtractorInput (via compatible types)
        AUTO /// Try to determine automatically (may throw)
    };

    /**
     * @brief Build with explicit conversion strategy
     */
    std::function<UniversalOutput(UniversalInput)> build_with_strategy(ConversionStrategy strategy)
    {
        if (m_stages.empty()) {
            throw std::runtime_error("Pipeline is empty");
        }

        auto stages = m_stages;
        auto matrix = m_matrix;

        return [stages, matrix, strategy, this](UniversalInput input) -> UniversalOutput {
            UniversalInput current_input = input;
            UniversalOutput current_output;

            for (const auto& stage : stages) {
                if (stage.executor) {
                    current_output = stage.executor(current_input);
                } else {
                    current_output = matrix->apply_operation(stage.operation_name, current_input);
                }

                if (&stage != &stages.back()) {
                    current_input = convert_with_strategy(current_output, strategy);
                }
            }

            return current_output;
        };
    }

private:
    /**
     * @brief Convert with explicit strategy
     */
    UniversalInput convert_with_strategy(const UniversalOutput& output, ConversionStrategy strategy)
    {
        return std::visit([strategy](auto&& value) -> UniversalInput {
            using T = std::decay_t<decltype(value)>;

            if constexpr (std::same_as<T, AnalyzerOutput>) {
                switch (strategy) {
                case ConversionStrategy::ANALYZER_TO_EXTRACTOR:
                case ConversionStrategy::AUTO:
                    return UniversalInput { ExtractorInput { value } };
                case ConversionStrategy::ANALYZER_TO_SORTER:
                    return UniversalInput { SorterInput { value } };
                default:
                    throw std::runtime_error("Invalid conversion strategy for AnalyzerOutput");
                }
            } else if constexpr (std::same_as<T, ExtractorOutput>) {
                if (strategy == ConversionStrategy::EXTRACTOR_TO_SORTER || strategy == ConversionStrategy::AUTO) {
                    // Extract compatible types from ExtractorOutput.base_output
                    return std::visit([](auto&& base_value) -> UniversalInput {
                        using BaseT = std::decay_t<decltype(base_value)>;

                        if constexpr (std::same_as<BaseT, std::vector<double>> || std::same_as<BaseT, Kakshya::DataVariant> || std::same_as<BaseT, Kakshya::RegionGroup> || std::same_as<BaseT, std::vector<Kakshya::RegionSegment>>) {
                            return UniversalInput { SorterInput { base_value } };
                        } else {
                            throw std::runtime_error("ExtractorOutput base type not compatible with SorterInput: " + std::string(typeid(BaseT).name()));
                        }
                    },
                        value.base_output);
                } else {
                    throw std::runtime_error("Invalid conversion strategy for ExtractorOutput");
                }
            } else if constexpr (std::same_as<T, SorterOutput>) {
                if (strategy == ConversionStrategy::SORTER_TO_EXTRACTOR || strategy == ConversionStrategy::AUTO) {
                    return std::visit([](auto&& sorted_value) -> UniversalInput {
                        using SortedT = std::decay_t<decltype(sorted_value)>;

                        if constexpr (std::same_as<SortedT, Kakshya::DataVariant> || std::same_as<SortedT, Kakshya::RegionGroup> || std::same_as<SortedT, std::vector<Kakshya::RegionSegment>>) {
                            return UniversalInput { ExtractorInput { sorted_value } };
                        } else {
                            throw std::runtime_error("SorterOutput type not compatible with ExtractorInput: " + std::string(typeid(SortedT).name()));
                        }
                    },
                        value);
                } else {
                    throw std::runtime_error("Invalid conversion strategy for SorterOutput");
                }
            }
        },
            output);
    }

    /**
     * @brief Safe automatic conversion - only handles well-defined cases
     */
    UniversalInput convert_output_to_input(const UniversalOutput& output)
    {
        try {
            return convert_with_strategy(output, ConversionStrategy::AUTO);
        } catch (const std::runtime_error& e) {
            throw std::runtime_error(
                std::string("Automatic conversion failed: ") + e.what() + "\nUse build_with_strategy() to specify explicit conversion strategy");
        }
    }
};

/**
 * @brief Factory function to create pipeline builders
 */
inline PipelineBuilder create_pipeline_builder(std::shared_ptr<ComputeMatrix> matrix)
{
    return PipelineBuilder(matrix);
}

} // namespace MayaFlux::Yantra
