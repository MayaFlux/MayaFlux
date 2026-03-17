#pragma once

#include "MayaFlux/Yantra/Data/DataIO.hpp"

namespace MayaFlux::Yantra {

/**
 * @class FluentExecutor
 * @brief Fluent interface for chaining operations on any executor
 *
 * Provides a composable, type-safe way to chain operations together.
 * This class is executor-agnostic and can work with any executor that
 * exposes execute<OpClass, InputType, OutputType>(Datum<InputType>) and
 * execute_named<OpClass, InputType, OutputType>(name, Datum<InputType>).
 *
 * Internal storage is always Datum<DataType>, preserving container
 * references and structural metadata across every link in the chain.
 * Entry points (constructors, make_fluent) accept raw DataType as a
 * convenience and wrap it immediately.
 *
 * Key Features:
 * - Type-safe operation chaining with compile-time verification
 * - Support for both type-based and named operations
 * - Container and metadata preservation through the full chain
 * - Custom function application within the chain
 * - Multiple terminal operations for different use cases
 * - Error accumulation and reporting
 *
 * Usage:
 * ```cpp
 * auto result = matrix->with(input_datum)
 *     .then<SegmentOp>("segment")
 *     .then<AttributeOp>("attribute")
 *     .then<SortOp>("sort")
 *     .get();
 * ```
 */
template <typename Executor, ComputeData DataType>
class MAYAFLUX_API FluentExecutor {
public:
    using executor_type = Executor;
    using data_type = DataType;
    using io_type = Datum<DataType>;

    // ------------------------------------------------------------------
    // Constructors — Datum overloads are primary; raw overloads wrap
    // ------------------------------------------------------------------

    /**
     * @brief Construct with executor and Datum input
     * @param executor Shared pointer to the executor instance
     * @param input Datum carrying data, container, and structural metadata
     */
    FluentExecutor(std::shared_ptr<Executor> executor, const Datum<DataType>& input)
        : m_executor(std::move(executor))
        , m_data(input)
        , m_successful(true)
    {
        if (!m_executor) {
            error<std::invalid_argument>(
                Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(),
                "FluentExecutor requires non-null executor");
        }
    }

    /**
     * @brief Construct with executor and Datum input (move)
     * @param executor Shared pointer to the executor instance
     * @param input Datum to move into the chain
     */
    FluentExecutor(std::shared_ptr<Executor> executor, Datum<DataType>&& input)
        : m_executor(std::move(executor))
        , m_data(std::move(input))
        , m_successful(true)
    {
        if (!m_executor) {
            error<std::invalid_argument>(
                Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(),
                "FluentExecutor requires non-null executor");
        }
    }

    /**
     * @brief Construct with executor and raw data (convenience entry point)
     *
     * Wraps data in a Datum immediately. No container is attached; use the
     * Datum overload when a container must be preserved through the chain.
     *
     * @param executor Shared pointer to the executor instance
     * @param input Raw data to wrap and process
     */
    FluentExecutor(std::shared_ptr<Executor> executor, const DataType& input)
        : FluentExecutor(std::move(executor), Datum<DataType>(input))
    {
    }

    /**
     * @brief Construct with executor and raw data (move, convenience entry point)
     *
     * Wraps data in a Datum immediately. No container is attached; use the
     * Datum overload when a container must be preserved through the chain.
     *
     * @param executor Shared pointer to the executor instance
     * @param input Raw data to move and wrap
     */
    FluentExecutor(std::shared_ptr<Executor> executor, DataType&& input)
        : FluentExecutor(std::move(executor), Datum<DataType>(std::move(input)))
    {
    }

    // ------------------------------------------------------------------
    // Chain operations
    // ------------------------------------------------------------------

    /**
     * @brief Chain an anonymous operation instance by type
     *
     * Constructs a new instance of OpClass and executes it on the current
     * Datum. The full Datum (data + container + metadata) is passed through.
     *
     * @tparam OpClass Operation class to instantiate and execute
     * @tparam OutputType Expected output type (defaults to current DataType)
     * @return New FluentExecutor carrying the result Datum
     */
    template <typename OpClass, ComputeData OutputType = DataType>
    FluentExecutor<Executor, OutputType> then()
    {
        if (!m_successful) {
            error<std::runtime_error>(
                Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(),
                "Cannot continue chain after failed operation");
        }

        try {
            auto result = m_executor->template execute<OpClass, DataType, OutputType>(m_data);
            if (!result) {
                m_successful = false;
                record_error("Operation " + std::string(typeid(OpClass).name()) + " failed");
                error<std::runtime_error>(
                    Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                    std::source_location::current(),
                    "Operation failed in fluent chain: {}", std::string(typeid(OpClass).name()));
            }

            auto next = FluentExecutor<Executor, OutputType>(m_executor, std::move(*result));
            next.m_operation_history = m_operation_history;
            next.m_operation_history.push_back(typeid(OpClass).name());
            return next;
        } catch (const std::exception& e) {
            m_successful = false;
            record_error(e.what());
            error_rethrow(
                Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(),
                "Exception in fluent chain: " + std::string(e.what()));
        }
    }

    /**
     * @brief Chain a named operation fetched from the executor's pool
     *
     * Retrieves the pre-configured operation instance registered under @p name
     * and executes it on the current Datum. The full Datum is passed through,
     * preserving container and metadata.
     *
     * @tparam OpClass Operation class type
     * @tparam OutputType Expected output type (defaults to current DataType)
     * @param name Name of the operation in the pool
     * @return New FluentExecutor carrying the result Datum
     */
    template <typename OpClass, ComputeData OutputType = DataType>
    FluentExecutor<Executor, OutputType> then(const std::string& name)
    {
        if (!m_successful) {
            error<std::runtime_error>(
                Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(),
                "Cannot continue chain after failed operation: {}", name);
        }

        try {
            auto result = m_executor->template execute_named<OpClass, DataType, OutputType>(name, m_data);
            if (!result) {
                m_successful = false;
                record_error("Named operation '" + name + "' failed");
                error<std::runtime_error>(
                    Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                    std::source_location::current(),
                    "Named operation failed in fluent chain: {}", name);
            }

            auto next = FluentExecutor<Executor, OutputType>(m_executor, std::move(*result));
            next.m_operation_history = m_operation_history;
            next.m_operation_history.push_back(name);
            return next;
        } catch (const std::exception& e) {
            m_successful = false;
            record_error(e.what());
            error_rethrow(
                Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(),
                "Exception in named operation '{}': {}", name, e.what());
        }
    }

    /**
     * @brief Apply a custom transformation function within the chain
     *
     * @p func receives the raw @c DataType (not the Datum) and returns a
     * value of any ComputeData type. The result is immediately wrapped in a
     * fresh Datum. Container from the current Datum is not carried forward
     * since the function may change the data type entirely; attach a container
     * explicitly via tap() before apply() if it must survive.
     *
     * @tparam Func Callable type: (const DataType&) -> U
     * @param func Transformation to apply
     * @return New FluentExecutor whose DataType is the return type of @p func
     */
    template <typename Func>
        requires std::invocable<Func, const DataType&>
    auto apply(Func&& func)
    {
        if (!m_successful) {
            error<std::runtime_error>(
                Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(),
                "Cannot continue chain after failed operation in apply");
        }

        using ResultType = std::invoke_result_t<Func, const DataType&>;

        try {
            auto result = std::forward<Func>(func)(m_data.data);
            auto next = FluentExecutor<Executor, ResultType>(
                m_executor, Datum<ResultType>(std::move(result)));
            next.m_operation_history = m_operation_history;
            next.m_operation_history.push_back("custom_function");
            return next;
        } catch (const std::exception& e) {
            m_successful = false;
            record_error(std::string("Custom function failed: ") + e.what());
            error_rethrow(
                Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(),
                "Exception in custom function: " + std::string(e.what()));
        }
    }

    /**
     * @brief Apply a side-effect function without changing data or type
     *
     * @p func receives a mutable reference to the raw @c DataType. Useful
     * for logging, validation, or in-place mutation that does not change type.
     * Does not affect the Datum wrapper or container.
     *
     * @tparam Func Callable type: (DataType&) -> void
     * @param func Function to apply for side effects
     * @return Reference to this executor for continued chaining
     */
    template <typename Func>
        requires std::invocable<Func, DataType&>
    FluentExecutor& tap(Func&& func)
    {
        if (!m_successful) {
            error<std::runtime_error>(
                Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(),
                "Cannot continue chain after failed operation in tap");
        }

        try {
            std::forward<Func>(func)(m_data.data);
            m_operation_history.emplace_back("tap");
            return *this;
        } catch (const std::exception& e) {
            m_successful = false;
            record_error(std::string("Tap function failed: ") + e.what());
            error_rethrow(
                Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(),
                "Exception in tap function: " + std::string(e.what()));
        }
    }

    // ------------------------------------------------------------------
    // Conditional execution
    // ------------------------------------------------------------------

    /**
     * @brief Execute an operation conditionally on a boolean flag
     * @tparam OpClass Operation to execute if @p condition is true
     * @param condition If true, executes the operation; otherwise passes through
     * @return Reference to this executor for continued chaining
     */
    template <typename OpClass>
    FluentExecutor& when(bool condition)
    {
        if (condition && m_successful)
            return then<OpClass>();
        return *this;
    }

    /**
     * @brief Execute an operation conditionally on a predicate over the raw data
     * @tparam OpClass Operation to execute if @p predicate returns true
     * @tparam Pred Callable type: (const DataType&) -> bool
     * @param predicate Function evaluated against the current raw data
     * @return Reference to this executor for continued chaining
     */
    template <typename OpClass, typename Pred>
        requires std::predicate<Pred, const DataType&>
    FluentExecutor& when(Pred&& predicate)
    {
        if (m_successful && std::forward<Pred>(predicate)(m_data.data))
            return then<OpClass>();
        return *this;
    }

    // ------------------------------------------------------------------
    // Parallel fork
    // ------------------------------------------------------------------

    /**
     * @brief Fork execution into multiple independent parallel paths
     *
     * Executes each operation in @p OpClasses against the current Datum
     * concurrently and returns a tuple of results. Each result is an
     * @c std::optional<Datum<DataType>>.
     *
     * @tparam OpClasses Operation classes to execute in parallel
     * @return std::tuple of optional Datum results, one per OpClass
     */
    template <typename... OpClasses>
    auto fork()
    {
        if (!m_successful) {
            error<std::runtime_error>(
                Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(),
                "Cannot fork after failed operation");
        }

        return std::make_tuple(
            m_executor->template execute<OpClasses, DataType>(m_data)...);
    }

    // ------------------------------------------------------------------
    // Terminal accessors
    // ------------------------------------------------------------------

    /**
     * @brief Get the final raw result by const reference
     * @return Const reference to the processed DataType
     */
    [[nodiscard]] const DataType& get() const
    {
        if (!m_successful) {
            error<std::runtime_error>(
                Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(),
                "Cannot get result from failed chain");
        }
        return m_data.data;
    }

    /**
     * @brief Get a mutable reference to the final raw result
     * @return Mutable reference to the processed DataType
     */
    DataType& get_mutable()
    {
        if (!m_successful) {
            error<std::runtime_error>(
                Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(),
                "Cannot get mutable result from failed chain");
        }
        return m_data.data;
    }

    /**
     * @brief Move the final raw result out of the chain
     * @return Moved DataType
     */
    DataType consume() &&
    {
        if (!m_successful) {
            error<std::runtime_error>(
                Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(),
                "Cannot consume result from failed chain");
        }
        return std::move(m_data.data);
    }

    /**
     * @brief Get the full result Datum by const reference
     *
     * Prefer this over get() when the container or metadata must be
     * inspected or passed to a subsequent non-chain call.
     *
     * @return Const reference to the result Datum
     */
    [[nodiscard]] const Datum<DataType>& get_datum() const
    {
        if (!m_successful) {
            error<std::runtime_error>(
                Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(),
                "Cannot get datum from failed chain");
        }
        return m_data;
    }

    /**
     * @brief Move the full result Datum out of the chain
     * @return Moved Datum including container and metadata
     */
    Datum<DataType> consume_datum() &&
    {
        if (!m_successful) {
            error<std::runtime_error>(
                Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(),
                "Cannot consume datum from failed chain");
        }
        return std::move(m_data);
    }

    /**
     * @brief Return the result Datum with execution history appended to metadata
     *
     * Equivalent to get_datum() but stamps execution_history, successful,
     * and errors into the Datum metadata before returning.
     *
     * @return Datum copy with execution metadata attached
     */
    [[nodiscard]] Datum<DataType> to_io() const
    {
        Datum<DataType> result = m_data;
        result.metadata["execution_history"] = m_operation_history;
        result.metadata["successful"] = m_successful;
        if (!m_errors.empty())
            result.metadata["errors"] = m_errors;
        return result;
    }

    /**
     * @brief Return the raw result or a fallback value if the chain failed
     * @param default_value Value returned when the chain is in a failed state
     * @return Processed DataType or @p default_value
     */
    [[nodiscard]] DataType get_or(const DataType& default_value) const
    {
        return m_successful ? m_data.data : default_value;
    }

    /**
     * @brief Return the raw result or invoke a generator if the chain failed
     * @tparam Generator Callable type: () -> DataType
     * @param generator Invoked to produce a fallback when the chain failed
     * @return Processed DataType or result of @p generator
     */
    template <typename Generator>
        requires std::invocable<Generator>
    [[nodiscard]] DataType get_or_else(Generator&& generator) const
    {
        return m_successful ? m_data.data : std::forward<Generator>(generator)();
    }

    // ------------------------------------------------------------------
    // Reset
    // ------------------------------------------------------------------

    /**
     * @brief Reset the chain with a new Datum, clearing history and errors
     *
     * Primary reset overload. Replaces internal state with @p new_datum,
     * restoring the chain to a successful state ready for reuse.
     *
     * @param new_datum Datum to restart the chain from
     * @return Reference to this executor for continued chaining
     */
    FluentExecutor& reset(const Datum<DataType>& new_datum)
    {
        m_data = new_datum;
        m_successful = true;
        m_operation_history.clear();
        m_errors.clear();
        return *this;
    }

    /**
     * @brief Reset the chain with raw data (convenience overload)
     *
     * Wraps @p new_data in a Datum and delegates to the primary reset.
     * No container is attached; use the Datum overload if a container
     * must be present from the start of the next chain.
     *
     * @param new_data Raw data to wrap and restart the chain from
     * @return Reference to this executor for continued chaining
     */
    FluentExecutor& reset(const DataType& new_data)
    {
        return reset(Datum<DataType>(new_data));
    }

    // ------------------------------------------------------------------
    // Accessors
    // ------------------------------------------------------------------

    /**
     * @brief Check whether all operations in the chain have succeeded
     * @return true if no operation has failed
     */
    [[nodiscard]] bool is_successful() const { return m_successful; }

    /**
     * @brief Get the ordered list of operation names executed so far
     * @return Const reference to the operation history vector
     */
    [[nodiscard]] const std::vector<std::string>& get_history() const { return m_operation_history; }

    /**
     * @brief Get all errors accumulated during the chain
     * @return Const reference to the error message vector
     */
    [[nodiscard]] const std::vector<std::string>& get_errors() const { return m_errors; }

    /**
     * @brief Get the underlying executor
     * @return Shared pointer to the executor instance
     */
    [[nodiscard]] std::shared_ptr<Executor> get_executor() const { return m_executor; }

private:
    std::shared_ptr<Executor> m_executor;
    Datum<DataType> m_data;
    bool m_successful;
    std::vector<std::string> m_operation_history;
    std::vector<std::string> m_errors;

    template <typename E, ComputeData U>
    friend class FluentExecutor;

    void record_error(const std::string& err) { m_errors.push_back(err); }
};

// ------------------------------------------------------------------
// make_fluent — free function entry points
// ------------------------------------------------------------------

/**
 * @brief Construct a FluentExecutor from a Datum with type deduction
 * @tparam Executor Executor type
 * @tparam DataType Inner data type (deduced from Datum)
 * @param executor Shared pointer to the executor
 * @param datum Datum to start the chain from
 * @return FluentExecutor<Executor, DataType>
 */
template <typename Executor, ComputeData DataType>
auto make_fluent(std::shared_ptr<Executor> executor, Datum<DataType>&& datum)
{
    return FluentExecutor<Executor, std::decay_t<DataType>>(
        std::move(executor),
        std::forward<Datum<DataType>>(datum));
}

/**
 * @brief Construct a FluentExecutor from raw data with type deduction (convenience entry point)
 *
 * Wraps @p data in a Datum immediately. No container is attached. Use the
 * Datum overload when a container must survive through the chain.
 *
 * @tparam Executor Executor type
 * @tparam DataType Data type (deduced)
 * @param executor Shared pointer to the executor
 * @param data Raw data to wrap and start the chain from
 * @return FluentExecutor<Executor, decay_t<DataType>>
 */
template <typename Executor, ComputeData DataType>
auto make_fluent(std::shared_ptr<Executor> executor, DataType&& data)
{
    return FluentExecutor<Executor, std::decay_t<DataType>>(
        std::move(executor),
        Datum<std::decay_t<DataType>>(std::forward<DataType>(data)));
}

} // namespace MayaFlux::Yantra
