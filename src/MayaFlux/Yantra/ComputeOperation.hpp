#pragma once

#include "Executors/GpuExecutionContext.hpp"

#include <future>

namespace MayaFlux::Yantra {

class ComputeMatrix;

/**
 * @struct OperationConfig
 * @brief Base of every operation's config struct.
 *
 * Config structs derive from it and are stored whole by
 * ComputeOperation::set_config, keyed by their own type. No string keys are
 * used. MF_DESCRIBE on a config is only needed to read it member by member.
 */
struct OperationConfig {
    OperationConfig() = default;
    OperationConfig(const OperationConfig&) = default;
    OperationConfig(OperationConfig&&) noexcept = default;
    OperationConfig& operator=(const OperationConfig&) = default;
    OperationConfig& operator=(OperationConfig&&) noexcept = default;

    /** @brief Retain a concrete config whole when placed in a parent-config collection. */
    template <typename Config>
        requires(!std::same_as<std::remove_cvref_t<Config>, OperationConfig>
            && std::derived_from<std::remove_cvref_t<Config>, OperationConfig>)
    OperationConfig(Config&& config)
    {
        m_facets.set(std::forward<Config>(config));
    }

    /** @brief Stores a struct under its type identity. */
    template <typename T>
    void set(T value)
    {
        m_facets.set(std::move(value));
    }

    /** @brief The struct of this type, or nullptr when absent. */
    template <typename T>
    [[nodiscard]] const T* get() const
    {
        return m_facets.get<T>();
    }

    /** @brief Apply retained concrete settings to an operation through its parent contract. */
    template <typename Op>
    void apply_to(Op& operation) const
    {
        operation.merge_config(m_facets);
    }

private:
    Facets m_facets;
};

/** @brief Borrowed views of the configs an operation holds. Valid while the operation lives and its configs are not replaced. */
using ConfigRefs = std::vector<std::reference_wrapper<const OperationConfig>>;

/**
 * @struct Supports
 * @brief The config structs an operation class accepts, declared as `using supports`.
 *
 * Single-config is the convention for CPU classes. A class may list several.
 */
template <typename... Configs>
struct Supports {
    static constexpr size_t count = sizeof...(Configs);

    template <typename C>
    static constexpr bool has = (std::same_as<C, Configs> || ...);

    /** @brief Calls f.template operator()<Config>() for each listed config. */
    template <typename F>
    static void each(F f)
    {
        (f.template operator()<Configs>(), ...);
    }
};

namespace detail {
    template <typename S>
    struct SoleConfig;

    template <typename C>
    struct SoleConfig<Supports<C>> {
        using type = C;
    };
}

/** @brief The only config of a single-config class. Does not compile for a class listing several. */
template <typename Op>
using default_config_t = typename detail::SoleConfig<typename Op::supports>::type;

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
class ComputeOperation {
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
     * @brief Stores a config struct, whole, under its own type.
     * @tparam Config A struct deriving from OperationConfig.
     */
    template <typename Config>
        requires std::derived_from<Config, OperationConfig>
    void set_config(Config config)
    {
        m_facets.set(std::move(config));
    }

    /**
     * @brief The stored config struct of type Config, or nullptr when absent.
     */
    template <typename Config>
        requires std::derived_from<Config, OperationConfig>
    [[nodiscard]] const Config* resolved_config() const
    {
        return m_facets.get<Config>();
    }

    /**
     * @brief The stored config struct of type Config, or its defaults when none was set.
     */
    template <typename Config>
        requires std::derived_from<Config, OperationConfig>
    [[nodiscard]] const Config& config() const
    {
        if (const auto* stored = m_facets.get<Config>()) {
            return *stored;
        }
        static const Config defaults {};
        return defaults;
    }

    /**
     * @brief Adds the config structs of @p incoming, replacing stored ones of the same type.
     */
    void merge_config(const Facets& incoming)
    {
        m_facets.merge(incoming);
    }

    /**
     * @brief The base view of the config structs this operation holds.
     *
     * Families and concrete operations override this with stored_configs().
     * The default is empty.
     */
    [[nodiscard]] virtual ConfigRefs configs() const
    {
        return {};
    }

    /**
     * @brief The stored configs among those listed, in listed order.
     */
    template <typename... Configs>
    [[nodiscard]] ConfigRefs stored_configs(Supports<Configs...>) const
    {
        ConfigRefs stored;
        (
            [&] {
                if (const auto* found = resolved_config<Configs>()) {
                    stored.emplace_back(*found);
                }
            }(),
            ...);
        return stored;
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
        constexpr bool can_reconstruct_from_double =
            (ComputeData<output_type> && !is_IO<output_type>::value && !CompositeData<output_type> && !TensorContainerData<output_type>)
            || (is_IO<output_type>::value && !detail::is_structured_operation_data_v<output_type>)
            || CompositeArrayData<output_type>
            || (is_IO<output_type>::value && CompositeArrayData<std::decay_t<decltype(std::declval<output_type>().data)>>);

        if constexpr (!can_reconstruct_from_double) {
            error<std::runtime_error>(
                Journal::Component::Yantra,
                Journal::Context::Runtime,
                std::source_location::current(),
                "Reconstruction from double is not available for this output type");
            return output_type {};
        } else {
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

    ExecutionContext m_last_execution_context;

private:
    std::vector<std::shared_ptr<ComputeOperation>> m_dependencies;
    Facets m_facets;

    /**
     * @brief Validate input/output types and warn about marker types
     */
    void validate_operation_data_types() const
    {
        if constexpr (std::is_same_v<InputType, Kakshya::Region>) {
            MF_INFO(
                Journal::Component::Yantra,
                Journal::Context::Runtime,
                "InputType 'Region' is an expressive marker, not a data holder. Operations will process coordinate data rather than signal data. Consider using DataVariant or SignalSourceContainer for signal processing.");
        } else if constexpr (std::is_same_v<InputType, Kakshya::RegionGroup>) {
            MF_INFO(
                Journal::Component::Yantra,
                Journal::Context::Runtime,
                "InputType 'RegionGroup' is an expressive marker, not a data holder. Operations will process coordinate data rather than signal data. Consider using DataVariant or SignalSourceContainer for signal processing.");
        } else if constexpr (std::is_same_v<InputType, std::vector<Kakshya::RegionSegment>>) {
            MF_INFO(
                Journal::Component::Yantra,
                Journal::Context::Runtime,
                "InputType 'RegionSegments' are expressive markers, not primary data holders. Operations will attempt to extract data from segment metadata. Consider using DataVariant or SignalSourceContainer for direct signal processing.");
        }

        if constexpr (std::is_same_v<OutputType, Kakshya::Region>) {
            MF_INFO(
                Journal::Component::Yantra,
                Journal::Context::Runtime,
                "OutputType 'Region' is an expressive marker, not a data holder. Operations will create spatial/temporal markers with results as metadata.");
        } else if constexpr (std::is_same_v<OutputType, Kakshya::RegionGroup>) {
            MF_INFO(
                Journal::Component::Yantra,
                Journal::Context::Runtime,
                "OutputType 'RegionGroup' is an expressive marker, not a data holder. Operations will organize results into spatial/temporal groups.");
        } else if constexpr (std::is_same_v<OutputType, std::vector<Kakshya::RegionSegment>>) {
            MF_INFO(
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
