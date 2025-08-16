#pragma once

#include "MayaFlux/Yantra/Data/DataIO.hpp"

namespace MayaFlux::Yantra {

/**
 * @concept ExecutorConcept
 * @brief Defines requirements for executor types that can be used with FluentExecutor
 */
template <typename T>
concept ExecutorConcept = requires(T t) {
    { t.template execute<int, int, int>(std::declval<int>()) };
};

/**
 * @class FluentExecutor
 * @brief Fluent interface for chaining operations on any executor
 *
 * Provides a composable, type-safe way to chain operations together.
 * This class is executor-agnostic and can work with any type that
 * satisfies the ExecutorConcept.
 *
 * Key Features:
 * - Type-safe operation chaining with compile-time verification
 * - Support for both type-based and named operations
 * - Custom function application within the chain
 * - Multiple terminal operations for different use cases
 * - Error accumulation and reporting
 *
 * Usage:
 * ```cpp
 * auto result = executor.with(input_data)
 *     .then<Transformer>()
 *     .apply([](auto& data) { return preprocess(data); })
 *     .then<Analyzer>("my_analyzer")
 *     .get();
 * ```
 */
template <typename Executor, ComputeData DataType>
class FluentExecutor {
public:
    using executor_type = Executor;
    using data_type = DataType;

    /**
     * @brief Construct with executor and initial data
     * @param executor Shared pointer to the executor instance
     * @param input Initial data to process
     */
    FluentExecutor(std::shared_ptr<Executor> executor, const DataType& input)
        : m_executor(std::move(executor))
        , m_data(input)
        , m_successful(true)
    {
        if (!m_executor) {
            throw std::invalid_argument("FluentExecutor requires non-null executor");
        }
    }

    /**
     * @brief Move constructor for efficiency
     */
    FluentExecutor(std::shared_ptr<Executor> executor, DataType&& input)
        : m_executor(std::move(executor))
        , m_data(std::move(input))
        , m_successful(true)
    {
        if (!m_executor) {
            throw std::invalid_argument("FluentExecutor requires non-null executor");
        }
    }

    /**
     * @brief Chain operation execution by type
     * @tparam OpClass Operation class to execute
     * @tparam OutputType Expected output type (defaults to current DataType)
     * @return New FluentExecutor with transformed data
     * @throws std::runtime_error if operation fails
     */
    template <typename OpClass, ComputeData OutputType = DataType>
    FluentExecutor<Executor, OutputType> then()
    {
        if (!m_successful) {
            throw std::runtime_error("Cannot continue chain after failed operation");
        }

        try {
            auto result = m_executor->template execute<OpClass, DataType, OutputType>(m_data);
            if (!result) {
                m_successful = false;
                record_error("Operation " + std::string(typeid(OpClass).name()) + " failed");
                throw std::runtime_error("Operation failed in fluent chain: " + std::string(typeid(OpClass).name()));
            }

            auto next = FluentExecutor<Executor, OutputType>(m_executor, std::move(result->data));
            next.m_operation_history = m_operation_history;
            next.m_operation_history.push_back(typeid(OpClass).name());
            return next;
        } catch (const std::exception& e) {
            m_successful = false;
            record_error(e.what());
            throw;
        }
    }

    /**
     * @brief Chain named operation
     * @tparam OpClass Operation class to execute
     * @tparam OutputType Expected output type
     * @param name Name of the operation in the pool
     * @return New FluentExecutor with transformed data
     */
    template <typename OpClass, ComputeData OutputType = DataType>
    FluentExecutor<Executor, OutputType> then(const std::string& name)
    {
        if (!m_successful) {
            throw std::runtime_error("Cannot continue chain after failed operation");
        }

        try {
            auto result = m_executor->template execute<OpClass, DataType, OutputType>(name, m_data);
            if (!result) {
                m_successful = false;
                record_error("Named operation '" + name + "' failed");
                throw std::runtime_error("Named operation failed in fluent chain: " + name);
            }

            auto next = FluentExecutor<Executor, OutputType>(m_executor, std::move(result->data));
            next.m_operation_history = m_operation_history;
            next.m_operation_history.push_back(name);
            return next;
        } catch (const std::exception& e) {
            m_successful = false;
            record_error(e.what());
            throw;
        }
    }

    /**
     * @brief Apply custom transformation function
     * @tparam Func Function type
     * @param func Transformation function to apply
     * @return New FluentExecutor with transformed data
     */
    template <typename Func>
        requires std::invocable<Func, const DataType&>
    auto apply(Func&& func)
    {
        if (!m_successful) {
            throw std::runtime_error("Cannot continue chain after failed operation");
        }

        using ResultType = std::invoke_result_t<Func, const DataType&>;

        try {
            auto result = std::forward<Func>(func)(m_data);
            auto next = FluentExecutor<Executor, ResultType>(m_executor, std::move(result));
            next.m_operation_history = m_operation_history;
            next.m_operation_history.push_back("custom_function");
            return next;
        } catch (const std::exception& e) {
            m_successful = false;
            record_error(std::string("Custom function failed: ") + e.what());
            throw;
        }
    }

    /**
     * @brief Apply function with side effects (doesn't change data type)
     * @param func Function to apply for side effects
     * @return Reference to this executor for chaining
     */
    template <typename Func>
        requires std::invocable<Func, DataType&>
    FluentExecutor& tap(Func&& func)
    {
        if (!m_successful) {
            throw std::runtime_error("Cannot continue chain after failed operation");
        }

        try {
            std::forward<Func>(func)(m_data);
            m_operation_history.emplace_back("tap");
            return *this;
        } catch (const std::exception& e) {
            m_successful = false;
            record_error(std::string("Tap function failed: ") + e.what());
            throw;
        }
    }

    /**
     * @brief Conditional execution
     * @tparam OpClass Operation to execute if condition is true
     * @param condition Condition to check
     * @return FluentExecutor with possibly transformed data
     */
    template <typename OpClass>
    FluentExecutor& when(bool condition)
    {
        if (condition && m_successful) {
            return then<OpClass>();
        }
        return *this;
    }

    /**
     * @brief Conditional execution with predicate
     * @tparam OpClass Operation to execute if predicate returns true
     * @param predicate Function to evaluate condition
     * @return FluentExecutor with possibly transformed data
     */
    template <typename OpClass, typename Pred>
        requires std::predicate<Pred, const DataType&>
    FluentExecutor& when(Pred&& predicate)
    {
        if (m_successful && std::forward<Pred>(predicate)(m_data)) {
            return then<OpClass>();
        }
        return *this;
    }

    /**
     * @brief Fork execution into multiple paths
     * @tparam OpClasses Operation classes to execute in parallel
     * @return Tuple of results from each operation
     */
    template <typename... OpClasses>
    auto fork()
    {
        if (!m_successful) {
            throw std::runtime_error("Cannot fork after failed operation");
        }

        return std::make_tuple(
            m_executor->template execute<OpClasses, DataType>(m_data)...);
    }

    /**
     * @brief Get the final result
     * @return Const reference to the processed data
     */
    const DataType& get() const
    {
        if (!m_successful) {
            throw std::runtime_error("Cannot get result from failed chain");
        }
        return m_data;
    }

    /**
     * @brief Get mutable reference to the result
     * @return Mutable reference to the processed data
     */
    DataType& get_mutable()
    {
        if (!m_successful) {
            throw std::runtime_error("Cannot get result from failed chain");
        }
        return m_data;
    }

    /**
     * @brief Move the result out
     * @return Moved data
     */
    DataType consume() &&
    {
        if (!m_successful) {
            throw std::runtime_error("Cannot consume result from failed chain");
        }
        return std::move(m_data);
    }

    /**
     * @brief Extract to IO wrapper with metadata
     * @return IO-wrapped data with execution metadata
     */
    IO<DataType> to_io() const
    {
        IO<DataType> result(m_data);
        result.metadata["execution_history"] = m_operation_history;
        result.metadata["successful"] = m_successful;
        if (!m_errors.empty()) {
            result.metadata["errors"] = m_errors;
        }
        return result;
    }

    /**
     * @brief Get or provide default value
     * @param default_value Value to return if chain failed
     * @return Processed data or default
     */
    DataType get_or(const DataType& default_value) const
    {
        return m_successful ? m_data : default_value;
    }

    /**
     * @brief Get or compute default value
     * @param generator Function to generate default value
     * @return Processed data or generated default
     */
    template <typename Generator>
        requires std::invocable<Generator>
    DataType get_or_else(Generator&& generator) const
    {
        return m_successful ? m_data : std::forward<Generator>(generator)();
    }

    /**
     * @brief Check if all operations succeeded
     */
    [[nodiscard]] bool is_successful() const { return m_successful; }

    /**
     * @brief Get operation history
     */
    [[nodiscard]] const std::vector<std::string>& get_history() const { return m_operation_history; }

    /**
     * @brief Get accumulated errors
     */
    [[nodiscard]] const std::vector<std::string>& get_errors() const { return m_errors; }

    /**
     * @brief Get the executor
     */
    std::shared_ptr<Executor> get_executor() const { return m_executor; }

    /**
     * @brief Reset with new data
     * @param new_data New initial data
     * @return Reference to this executor
     */
    FluentExecutor& reset(const DataType& new_data)
    {
        m_data = new_data;
        m_successful = true;
        m_operation_history.clear();
        m_errors.clear();
        return *this;
    }

private:
    std::shared_ptr<Executor> m_executor;
    DataType m_data;
    bool m_successful;
    std::vector<std::string> m_operation_history;
    std::vector<std::string> m_errors;

    void record_error(const std::string& error)
    {
        m_errors.push_back(error);
    }
};

/**
 * @brief Helper to create FluentExecutor with type deduction
 */
template <typename Executor, ComputeData DataType>
auto make_fluent(std::shared_ptr<Executor> executor, DataType&& data)
{
    return FluentExecutor<Executor, std::decay_t<DataType>>(
        std::move(executor),
        std::forward<DataType>(data));
}

} // namespace MayaFlux::Yantra
