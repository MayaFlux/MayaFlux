#pragma once

#include "OperationSpec/ExecutionContext.hpp"
#include "OperationSpec/OperationHelper.hpp"

#include "Data/DataIO.hpp"

#include <future>

namespace MayaFlux::Yantra {

class ComputeMatrix;

/**
 * @class ComputeOperation
 * @brief Base interface for all computational operations in the processing pipeline
 *
 * Defines the core contract for operations that transform data from one type to another.
 * Operations can be parameterized, validated, and composed into complex processing networks.
 *
 * @tparam InputType The data type accepted by this operation
 * @tparam OutputType The data type produced by this operation, defaults to InputType
 */
template <ComputeData InputType = std::vector<Kakshya::DataVariant>, ComputeData OutputType = InputType>
class MAYAFLUX_API ComputeOperation {
public:
    using input_type = IO<InputType>;
    using output_type = IO<OutputType>;

    /**
     * @brief Constructor with data type validation warnings
     */
    ComputeOperation()
    {
        validate_operation_data_types();
    }

    /**
     * @brief Virtual destructor for proper cleanup of derived classes
     */
    virtual ~ComputeOperation() = default;

    /**
     * @brief Public synchronous execution interface
     */
    output_type apply_operation(const input_type& input)
    {
        return apply_operation_internal(input, m_last_execution_context);
    }

    /**
     * @brief Applies the operation with dependencies resolved
     * @param input Input data to process
     * @return Processed output data
     *
     * This method ensures that all dependencies are executed before applying the operation.
     * It is intended for use in scenarios where the operation is part of a larger processing graph.
     *
     * @note: This method is for usage outside of ComputeMatrix, and will not work recursively.
               For ComputeMatrix, simply use a chain of operations.
     */
    output_type apply_operation_with_dependencies(const input_type& input)
    {
        m_last_execution_context.mode = ExecutionMode::DEPENDENCY;

        for (auto& dep : m_dependencies) {
            if (dep->validate_input(input)) {
                dep->apply_operation_internal(input, m_last_execution_context);
            }
        }

        m_last_execution_context.mode = ExecutionMode::SYNC;
        return apply_operation_internal(input, m_last_execution_context);
    }

    /**
     * @brief Convenience overload for direct data processing (backward compatibility)
     * @param data Raw data to be processed
     * @return Transformed output as IO context
     */
    output_type operator()(const InputType& data)
    {
        return apply_operation(input_type { data });
    }

    /**
     * @brief Convenience overload that extracts just the data from result
     * @param data Raw data to be processed
     * @return Just the transformed data (no metadata/recursion)
     */
    OutputType apply_to_data(const InputType& data)
    {
        return apply_operation(input_type { data }).data;
    }

    /**
     * @brief Sets a named parameter that configures the operation's behavior
     * @param name Parameter identifier
     * @param value Parameter value stored as std::any
     */
    virtual void set_parameter(const std::string& name, std::any value) = 0;

    /**
     * @brief Retrieves a parameter's current value
     * @param name Parameter identifier
     * @return Current parameter value as std::any
     */
    [[nodiscard]] virtual std::any get_parameter(const std::string& name) const = 0;

    /**
     * @brief Retrieves all parameters and their values
     * @return Map of parameter names to their values
     */
    [[nodiscard]] virtual std::map<std::string, std::any> get_all_parameters() const { return {}; }

    /**
     * @brief Validates if the input data meets the operation's requirements
     * @param input Data to validate
     * @return True if input is valid, false otherwise
     */
    virtual bool validate_input(const input_type&) const { return true; }

    /**
     * @brief Get operation name for debugging/introspection
     */
    [[nodiscard]] virtual std::string get_name() const { return "ComputeOperation"; }

    /**
     * @brief OpUnit interface - operations can act as units in dependency graphs
     */
    output_type execute(const input_type& input)
    {
        m_last_execution_context.mode = ExecutionMode::DEPENDENCY;
        return apply_operation_internal(input, m_last_execution_context);
    }

    void add_dependency(std::shared_ptr<ComputeOperation> dep)
    {
        m_dependencies.push_back(std::move(dep));
    }

    const auto& get_dependencies() const { return m_dependencies; }

    virtual void set_container_for_regions(const std::shared_ptr<Kakshya::SignalSourceContainer>& container)
    {
        m_container = container;
    }

    [[nodiscard]] virtual const std::shared_ptr<Kakshya::SignalSourceContainer>& get_container_for_regions() const
    {
        return m_container;
    }

    void set_last_execution_context(const ExecutionContext& ctx)
    {
        m_last_execution_context = ctx;
    }

    [[nodiscard]] const ExecutionContext& get_last_execution_context() const
    {
        return m_last_execution_context;
    }

    void set_pre_execution_hook(const OpererationHookCallback& hook)
    {
        m_last_execution_context.pre_execution_hook = hook;
    }

    void set_post_execution_hook(const OpererationHookCallback& hook)
    {
        m_last_execution_context.post_execution_hook = hook;
    }

    void set_reconstruction_callback(const ReconstructionCallback& callback)
    {
        m_last_execution_context.reconstruction_callback = callback;
    }

protected:
    /**
     * @brief Executes the computational transformation on the input data
     * @param input Data to be processed
     * @return Transformed output data
     */
    virtual output_type operation_function(const input_type& input) = 0;

    /**
     * @brief Internal execution method - ComputeMatrix can access this
     * @param input Input data wrapped in IO
     * @param context Execution context with mode, threading, etc.
     * @return Processed output
     */
    virtual output_type apply_operation_internal(const input_type& input, const ExecutionContext& context)
    {
        switch (context.mode) {
        case ExecutionMode::ASYNC:
            // Return the result of the future (this might need different handling)
            return apply_operation_async(input).get();

        case ExecutionMode::PARALLEL:
            return apply_operation_parallel(input, context);

        case ExecutionMode::CHAINED:
            return apply_operation_chained(input, context);

        case ExecutionMode::DEPENDENCY:
        case ExecutionMode::SYNC:
        default:
            return apply_hooks(input, context);
        }
    }

    /**
     * @brief Optional async implementation - default delegates to operation_function
     */
    virtual std::future<output_type> apply_operation_async(const input_type& input)
    {
        return std::async(std::launch::async, [this, input]() {
            return apply_hooks(input, m_last_execution_context);
        });
    }

    /**
     * @brief Optional parallel-aware implementation - default delegates to operation_function
     */
    virtual output_type apply_operation_parallel(const input_type& input, const ExecutionContext& ctx)
    {
        return apply_hooks(input, ctx);
    }

    /**
     * @brief Optional chain-aware implementation - default delegates to operation_function
     */
    virtual output_type apply_operation_chained(const input_type& input, const ExecutionContext& ctx)
    {
        return apply_hooks(input, ctx);
    }

    /**
     * @brief Convert processed double data back to OutputType using metadata and optional callback
     */
    output_type convert_result(std::vector<std::vector<double>>& result_data, DataStructureInfo& metadata)
    {
        std::any any_data = metadata;
        if (m_last_execution_context.reconstruction_callback) {
            auto reconstructed = m_last_execution_context.reconstruction_callback(result_data, any_data);
            try {
                return std::any_cast<output_type>(reconstructed);
            } catch (const std::bad_any_cast&) {
                std::cerr << "Reconstruction callback did not return the correct output type\n";
                return OperationHelper::reconstruct_from_double<output_type>(result_data, metadata);
            }
        }
        return OperationHelper::reconstruct_from_double<output_type>(result_data, metadata);
    }

    std::shared_ptr<Kakshya::SignalSourceContainer> m_container;

    ExecutionContext m_last_execution_context;

private:
    std::vector<std::shared_ptr<ComputeOperation>> m_dependencies;

    /**
     * @brief Validate input/output types and warn about marker types
     */
    void validate_operation_data_types() const
    {
        if constexpr (std::is_same_v<InputType, Kakshya::Region>) {
            std::cerr << "OPERATION WARNING: InputType 'Region' is an expressive marker, not a data holder.\n"
                      << "Operations will process coordinate data rather than signal data.\n"
                      << "Consider using DataVariant or SignalSourceContainer for signal processing.\n";
        } else if constexpr (std::is_same_v<InputType, Kakshya::RegionGroup>) {
            std::cerr << "OPERATION WARNING: InputType 'RegionGroup' is an expressive marker, not a data holder.\n"
                      << "Operations will process coordinate data rather than signal data.\n"
                      << "Consider using DataVariant or SignalSourceContainer for signal processing.\n";
        } else if constexpr (std::is_same_v<InputType, std::vector<Kakshya::RegionSegment>>) {
            std::cerr << "OPERATION WARNING: InputType 'RegionSegments' are expressive markers, not primary data holders.\n"
                      << "Operations will attempt to extract data from segment metadata.\n"
                      << "Consider using DataVariant or SignalSourceContainer for direct signal processing.\n";
        }

        if constexpr (std::is_same_v<OutputType, Kakshya::Region>) {
            std::cerr << "OPERATION INFO: OutputType 'Region' will create spatial/temporal markers with results as metadata.\n";
        } else if constexpr (std::is_same_v<OutputType, Kakshya::RegionGroup>) {
            std::cerr << "OPERATION INFO: OutputType 'RegionGroup' will organize results into spatial/temporal groups.\n";
        } else if constexpr (std::is_same_v<OutputType, std::vector<Kakshya::RegionSegment>>) {
            std::cerr << "OPERATION INFO: OutputType 'RegionSegments' will create segments with results in metadata.\n";
        }
    }

    output_type apply_hooks(const input_type& input, const ExecutionContext& context)
    {
        if (context.pre_execution_hook) {
            std::any input_any = const_cast<input_type&>(input);
            context.pre_execution_hook(input_any);
        }

        auto result = operation_function(input);

        if (context.post_execution_hook) {
            std::any result_any = &result;
            context.post_execution_hook(result_any);
        }
        return result;
    }

    friend class ComputeMatrix;
};

// Type aliases for common operation patterns
using DataOperation = ComputeOperation<Kakshya::DataVariant>;
using ContainerOperation = ComputeOperation<std::shared_ptr<Kakshya::SignalSourceContainer>>;
using RegionOperation = ComputeOperation<Kakshya::Region>;
using RegionGroupOperation = ComputeOperation<Kakshya::RegionGroup>;
using SegmentOperation = ComputeOperation<std::vector<Kakshya::RegionSegment>>;

}
