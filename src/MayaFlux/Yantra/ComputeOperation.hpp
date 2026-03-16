#pragma once

#include "Executors/GpuExecutionContext.hpp"

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
    using input_type = Datum<InputType>;
    using output_type = Datum<OutputType>;

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
     * @return Transformed output as Datum context
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
     * @brief Returns the category of this operation for grammar and registry discovery.
     */
    [[nodiscard]] virtual OperationType get_operation_type() const = 0;

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

    void set_pre_execution_hook(const OperationHookCallback& hook)
    {
        m_last_execution_context.pre_execution_hook = hook;
    }

    void set_post_execution_hook(const OperationHookCallback& hook)
    {
        m_last_execution_context.post_execution_hook = hook;
    }

    void set_reconstruction_callback(const ReconstructionCallback& callback)
    {
        m_last_execution_context.reconstruction_callback = callback;
    }

    /**
     * @brief Attach a GPU execution backend.
     *
     * When set and ready, apply_operation_internal delegates to the backend
     * instead of operation_function. The CPU implementation in operation_function
     * remains the automatic fallback when no backend is attached or GPU
     * initialisation has not yet succeeded.
     *
     * @param backend Configured GpuExecutionContext instance.
     */
    void set_gpu_backend(std::shared_ptr<GpuExecutionContext<InputType, OutputType>> backend)
    {
        m_gpu_backend = std::move(backend);
    }

    [[nodiscard]] bool has_gpu_backend() const { return m_gpu_backend != nullptr; }

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
        if (m_gpu_backend && m_gpu_backend->ensure_gpu_ready()) {
            return m_gpu_backend->execute(input, context);
        }

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
            auto result = safe_any_cast<output_type>(reconstructed);
            if (result) {
                return *result.value;
            }

            MF_WARN(
                Journal::Component::Yantra,
                Journal::Context::Runtime,
                "Reconstruction callback type mismatch: {}",
                result.error);
            return OperationHelper::reconstruct_from_double<output_type>(result_data, metadata);
        }

        return OperationHelper::reconstruct_from_double<output_type>(result_data, metadata);
    }

    std::shared_ptr<Kakshya::SignalSourceContainer> m_container;

    ExecutionContext m_last_execution_context;

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

private:
    std::vector<std::shared_ptr<ComputeOperation>> m_dependencies;
    std::shared_ptr<GpuExecutionContext<InputType, OutputType>> m_gpu_backend;

    /**
     * @brief Validate input/output types and warn about marker types
     */
    void validate_operation_data_types() const
    {
        if constexpr (std::is_same_v<InputType, Kakshya::Region>) {
            MF_WARN(
                Journal::Component::Yantra,
                Journal::Context::Runtime,
                "InputType 'Region' is an expressive marker, not a data holder. Operations will process coordinate data rather than signal data. Consider using DataVariant or SignalSourceContainer for signal processing.");
        } else if constexpr (std::is_same_v<InputType, Kakshya::RegionGroup>) {
            MF_WARN(
                Journal::Component::Yantra,
                Journal::Context::Runtime,
                "InputType 'RegionGroup' is an expressive marker, not a data holder. Operations will process coordinate data rather than signal data. Consider using DataVariant or SignalSourceContainer for signal processing.");
        } else if constexpr (std::is_same_v<InputType, std::vector<Kakshya::RegionSegment>>) {
            MF_WARN(
                Journal::Component::Yantra,
                Journal::Context::Runtime,
                "InputType 'RegionSegments' are expressive markers, not primary data holders. Operations will attempt to extract data from segment metadata. Consider using DataVariant or SignalSourceContainer for direct signal processing.");
        }

        if constexpr (std::is_same_v<OutputType, Kakshya::Region>) {
            MF_WARN(
                Journal::Component::Yantra,
                Journal::Context::Runtime,
                "OutputType 'Region' is an expressive marker, not a data holder. Operations will create spatial/temporal markers with results as metadata.");
        } else if constexpr (std::is_same_v<OutputType, Kakshya::RegionGroup>) {
            MF_WARN(
                Journal::Component::Yantra,
                Journal::Context::Runtime,
                "OutputType 'RegionGroup' is an expressive marker, not a data holder. Operations will organize results into spatial/temporal groups.");
        } else if constexpr (std::is_same_v<OutputType, std::vector<Kakshya::RegionSegment>>) {
            MF_WARN(
                Journal::Component::Yantra,
                Journal::Context::Runtime,
                "OutputType 'RegionSegments' is an expressive marker, not a data holder. Operations will create segments with results in metadata. Consider using DataVariant or SignalSourceContainer for direct signal processing.");
        }
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
