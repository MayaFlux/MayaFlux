#pragma once

#include "ComputeOperation.hpp"
#include "OperationSpec/OperationChain.hpp"
#include "OperationSpec/OperationPool.hpp"

#include "MayaFlux/Transitive/Parallel/Execution.hpp"

namespace MayaFlux::Yantra {

/**
 * @enum ExecutionPolicy
 * @brief Policy for execution strategy selection
 */
enum class ExecutionPolicy : uint8_t {
    CONSERVATIVE, // Prefer safety and predictability
    BALANCED, // Balance between performance and safety
    AGGRESSIVE // Maximize performance
};

/**
 * @class ComputeMatrix
 * @brief Local execution orchestrator for computational operations
 *
 * ComputeMatrix provides a self-contained execution environment for operations.
 * It maintains its own operation instances and execution strategies without
 * relying on any global registries. Each matrix instance is independent.
 *
 * Key Design Principles:
 * - Instance-local operation management
 * - Focus on execution patterns and strategies
 * - Clean separation from registration concerns
 *
 * Core Responsibilities:
 * - Execute operations with various strategies (sync, async, parallel, chain)
 * - Manage instance-local named operations
 * - Provide fluent interface through FluentExecutor
 * - Configure execution contexts for optimization
 */
class MAYAFLUX_API ComputeMatrix : public std::enable_shared_from_this<ComputeMatrix> {
public:
    /**
     * @brief Create a new ComputeMatrix instance
     */
    static std::shared_ptr<ComputeMatrix> create()
    {
        return std::make_shared<ComputeMatrix>();
    }

    /**
     * @brief Add a pre-configured operation instance to this matrix
     * @tparam OpClass Operation class type
     * @param name Unique name within this matrix
     * @param operation Shared pointer to the operation
     * @return true if added successfully
     */
    template <typename OpClass>
    bool add_operation(const std::string& name, std::shared_ptr<OpClass> operation)
    {
        if (!operation)
            return false;
        return m_operations.add(name, operation);
    }

    /**
     * @brief Create and add an operation to this matrix
     * @tparam OpClass Operation class type
     * @tparam Args Constructor argument types
     * @param name Unique name within this matrix
     * @param args Constructor arguments
     * @return Shared pointer to the created operation
     */
    template <typename OpClass, typename... Args>
    std::shared_ptr<OpClass> create_operation(const std::string& name, Args&&... args)
    {
        auto operation = std::make_shared<OpClass>(std::forward<Args>(args)...);
        if (m_operations.add(name, operation)) {
            return operation;
        }
        return nullptr;
    }

    /**
     * @brief Get a named operation from this matrix
     * @tparam OpClass Expected operation type
     * @param name Operation name
     * @return Shared pointer to operation or nullptr
     */
    template <typename OpClass>
    std::shared_ptr<OpClass> get_operation(const std::string& name)
    {
        return m_operations.get<OpClass>(name);
    }

    /**
     * @brief Remove a named operation from this matrix
     * @param name Operation name to remove
     * @return true if removed successfully
     */
    bool remove_operation(const std::string& name)
    {
        return m_operations.remove(name);
    }

    /**
     * @brief List all operation names in this matrix
     * @return Vector of operation names
     */
    std::vector<std::string> list_operations() const
    {
        return m_operations.list_names();
    }

    /**
     * @brief Clear all operations from this matrix
     */
    void clear_operations()
    {
        m_operations.clear();
    }

    /**
     * @brief Execute an operation by creating a new instance
     * @tparam OpClass Operation class to instantiate and execute
     * @tparam Datum InputType Input data type
     * @tparam Datum OutputType Output data type
     * @param input Input data
     * @param args Constructor arguments for the operation
     * @return Optional containing result or nullopt on failure
     */
    template <typename OpClass, ComputeData InputType, ComputeData OutputType = InputType, typename... Args>
    std::optional<Datum<OutputType>> execute(const Datum<InputType>& input, Args&&... args)
    {
        auto operation = std::make_shared<OpClass>(std::forward<Args>(args)...);
        return execute_operation<OpClass, InputType, OutputType>(operation, input);
    }

    /**
     * @brief Execute a named operation from the pool
     * @tparam OpClass Operation class type
     * @tparam Datum InputType Input data type
     * @tparam Datum OutputType Output data type
     * @param name Name of the operation
     * @param input Input data
     * @return Optional containing result or nullopt on failure
     */
    template <typename OpClass, ComputeData InputType, ComputeData OutputType = InputType>
    std::optional<Datum<OutputType>> execute_named(const std::string& name, const Datum<InputType>& input)
    {
        auto operation = m_operations.get<OpClass>(name);
        if (!operation)
            return std::nullopt;
        return execute_operation<OpClass, InputType, OutputType>(operation, input);
    }

    /**
     * @brief Execute with provided operation instance
     * @tparam OpClass Operation class type
     * @tparam Datum InputType Input data type
     * @tparam Datum OutputType Output data type
     * @param operation Operation instance to execute
     * @param input Input data
     * @return Optional containing result or nullopt on failure
     */
    template <typename OpClass, ComputeData InputType, ComputeData OutputType = InputType>
    std::optional<Datum<OutputType>> execute_with(std::shared_ptr<OpClass> operation, const Datum<InputType>& input)
    {
        return execute_operation<OpClass, InputType, OutputType>(operation, input);
    }

    /**
     * @brief Execute operation asynchronously
     * @tparam OpClass Operation class type
     * @tparam InputType Input data type
     * @tparam OutputType Output data type
     * @param input Input data
     * @param args Constructor arguments
     * @return Future containing the result
     */
    template <typename OpClass, ComputeData InputType, ComputeData OutputType = InputType, typename... Args>
    std::future<std::optional<Datum<OutputType>>> execute_async(const Datum<InputType>& input, Args&&... args)
    {
        return std::async(std::launch::async, [this, input, args...]() {
            return execute<OpClass, InputType, OutputType>(input, args...);
        });
    }

    /**
     * @brief Execute named operation asynchronously
     */
    template <typename OpClass, ComputeData InputType, ComputeData OutputType = InputType>
    std::future<std::optional<Datum<OutputType>>> execute_named_async(const std::string& name, const Datum<InputType>& input)
    {
        return std::async(std::launch::async, [this, name, input]() {
            return execute_named<OpClass, InputType, OutputType>(name, input);
        });
    }

    /**
     * @brief Execute multiple operations in parallel
     * @tparam InputType Input data type
     * @tparam OpClasses Operation classes to execute
     * @param input Input data
     * @return Tuple of results from each operation
     */
    template <ComputeData InputType, typename... OpClasses>
    auto execute_parallel(const Datum<InputType>& input)
    {
        return std::make_tuple(
            execute_async<OpClasses, InputType>(input).get()...);
    }

    /**
     * @brief Execute multiple named operations in parallel
     * @tparam OpClass Base operation class
     * @tparam InputType Input data type
     * @tparam OutputType Output data type
     * @param names Vector of operation names
     * @param input Input data
     * @return Vector of results
     */
    template <typename OpClass, ComputeData InputType, ComputeData OutputType = InputType>
    std::vector<std::optional<Datum<OutputType>>> execute_parallel_named(
        const std::vector<std::string>& names,
        const Datum<InputType>& input)
    {
        std::vector<std::future<std::optional<Datum<OutputType>>>> futures;
        futures.reserve(names.size());

        for (const auto& name : names) {
            futures.push_back(execute_named_async<OpClass, InputType, OutputType>(name, input));
        }

        std::vector<std::optional<Datum<OutputType>>> results;
        results.reserve(futures.size());

        for (auto& future : futures) {
            results.push_back(future.get());
        }

        return results;
    }

    /**
     * @brief Execute operations in sequence (type-safe chain)
     * @tparam FirstOp First operation type
     * @tparam SecondOp Second operation type
     * @tparam InputType Initial input type
     * @tparam IntermediateType Type between operations
     * @tparam OutputType Final output type
     * @param input Initial input
     * @return Optional containing final result
     */
    template <typename FirstOp, typename SecondOp,
        ComputeData InputType,
        ComputeData IntermediateType,
        ComputeData OutputType>
    std::optional<Datum<OutputType>> execute_chain(const Datum<InputType>& input)
    {
        auto first_result = execute<FirstOp, InputType, IntermediateType>(input);
        if (!first_result)
            return std::nullopt;

        return execute<SecondOp, IntermediateType, OutputType>(*first_result);
    }

    /**
     * @brief Execute named operations in sequence
     */
    template <typename FirstOp, typename SecondOp,
        ComputeData InputType,
        ComputeData IntermediateType,
        ComputeData OutputType>
    std::optional<Datum<OutputType>> execute_chain_named(
        const std::string& first_name,
        const std::string& second_name,
        const Datum<InputType>& input)
    {
        auto first_result = execute_named<FirstOp, InputType, IntermediateType>(first_name, input);
        if (!first_result)
            return std::nullopt;

        return execute_named<SecondOp, IntermediateType, OutputType>(second_name, *first_result);
    }

    /**
     * @brief Execute operation on multiple inputs
     * @tparam OpClass Operation class
     * @tparam InputType Input data type
     * @tparam OutputType Output data type
     * @param inputs Vector of inputs
     * @param args Constructor arguments for operation
     * @return Vector of results
     */
    template <typename OpClass, ComputeData InputType, ComputeData OutputType = InputType, typename... Args>
    std::vector<std::optional<Datum<OutputType>>> execute_batch(
        const std::vector<Datum<InputType>>& inputs,
        Args&&... args)
    {
        auto operation = std::make_shared<OpClass>(std::forward<Args>(args)...);

        std::vector<std::optional<Datum<OutputType>>> results;
        results.reserve(inputs.size());

        for (const auto& input : inputs) {
            results.push_back(execute_operation<OpClass, InputType, OutputType>(operation, input));
        }

        return results;
    }

    /**
     * @brief Execute operation on multiple inputs in parallel
     */
    template <typename OpClass, ComputeData InputType, ComputeData OutputType = InputType, typename... Args>
    std::vector<std::optional<Datum<OutputType>>> execute_batch_parallel(
        const std::vector<Datum<InputType>>& inputs,
        Args&&... args)
    {
        auto operation = std::make_shared<OpClass>(std::forward<Args>(args)...);

        std::vector<std::optional<Datum<OutputType>>> results(inputs.size());

        MayaFlux::Parallel::transform(MayaFlux::Parallel::par_unseq,
            inputs.begin(), inputs.end(),
            results.begin(),
            [this, operation](const Datum<InputType>& input) {
                return execute_operation<OpClass, InputType, OutputType>(operation, input);
            });

        return results;
    }

    /**
     * @brief Create a fluent executor for chaining operations
     * @tparam StartType Initial data type
     * @param input Initial input data
     * @return FluentExecutor for this matrix
     */
    template <ComputeData StartType>
    FluentExecutor<ComputeMatrix, StartType> with(const Datum<StartType>& input)
    {
        return FluentExecutor<ComputeMatrix, StartType>(shared_from_this(), input);
    }

    template <ComputeData StartType>
    FluentExecutor<ComputeMatrix, StartType> with(const StartType& input)
    {
        return with(Datum<StartType>(input));
    }

    /**
     * @brief Create a fluent executor with move semantics
     */

    template <ComputeData StartType>
    FluentExecutor<ComputeMatrix, StartType> with(Datum<StartType>&& input)
    {
        return FluentExecutor<ComputeMatrix, StartType>(shared_from_this(), std::forward<Datum<StartType>>(input));
    }

    template <ComputeData StartType>
    FluentExecutor<ComputeMatrix, StartType> with(StartType&& input)
    {
        return with(Datum<StartType>(std::forward<StartType>(input)));
    }

    /**
     * @brief Execute an operation chain asynchronously.
     *
     * @p chain receives a FluentExecutor seeded with @p input and must return
     * the terminal Datum. The entire sequence runs on a background thread.
     * @p on_complete is invoked with the result on that same thread.
     *
     * The future is owned by this matrix instance. drain_async() and the
     * destructor guarantee all in-flight chains finish before the matrix
     * is destroyed. No thread management is required by the caller.
     *
     * @code
     * matrix->with_async(make_granular_input(container),
     *     [](auto chain) {
     *         return chain.then<SegmentOp>("segment")
     *                     .then<AttributeOp>("attribute")
     *                     .then<SortOp>("sort")
     *                     .to_io();
     *     },
     *     [ex](Datum<Kakshya::RegionGroup> result) {
     *         ex->grains = std::move(result);
     *         ex->ready  = true;
     *     });
     * @endcode
     *
     * @tparam StartType   Input data type.
     * @tparam ChainFunc   Callable: (FluentExecutor<ComputeMatrix, StartType>) -> Datum<ResultType>.
     * @tparam CompleteFn  Callable: (Datum<R>) -> void, where R is deduced from ChainFunc.
     * @param input        Seed datum for the chain.
     * @param chain        Lambda describing the full operation sequence.
     * @param on_complete  Called with the final Datum on completion.
     */
    template <ComputeData StartType, typename ChainFunc, typename CompleteFn>
    void with_async(Datum<StartType> input, ChainFunc&& chain, CompleteFn&& on_complete)
    {
        auto self = shared_from_this();
        register_async(std::async(std::launch::async,
            [self,
                input = std::move(input),
                chain = std::forward<ChainFunc>(chain),
                on_complete = std::forward<CompleteFn>(on_complete)]() mutable {
                on_complete(chain(
                    FluentExecutor<ComputeMatrix, StartType>(self, std::move(input))));
            }));
    }

    template <ComputeData StartType, typename ChainFunc, typename CompleteFn>
    void with_async(StartType input, ChainFunc&& chain, CompleteFn&& on_complete)
    {
        with_async(Datum<StartType>(std::move(input)),
            std::forward<ChainFunc>(chain),
            std::forward<CompleteFn>(on_complete));
    }

    /**
     * @brief Set execution policy for this matrix
     * @param policy Execution policy to use
     */
    void set_execution_policy(ExecutionPolicy policy)
    {
        m_execution_policy = policy;
    }

    /**
     * @brief Get current execution policy
     */
    ExecutionPolicy get_execution_policy() const
    {
        return m_execution_policy;
    }

    /**
     * @brief Enable/disable execution profiling
     */
    void set_profiling(bool enabled)
    {
        m_profiling_enabled = enabled;
    }

    /**
     * @brief Get execution statistics
     */
    std::unordered_map<std::string, std::any> get_statistics() const
    {
        auto stats = m_operations.get_statistics();
        stats["total_executions"] = m_total_executions.load();
        stats["failed_executions"] = m_failed_executions.load();
        if (m_profiling_enabled) {
            stats["average_execution_time_ms"] = m_average_execution_time.load();
        }
        return stats;
    }

    /**
     * @brief Set custom context configurator
     * @param configurator Function to configure execution contexts
     */
    void set_context_configurator(
        std::function<void(ExecutionContext&, const std::type_index&)> configurator)
    {
        m_context_configurator = std::move(configurator);
    }

    /**
     * @brief Set default execution timeout
     * @param timeout Timeout duration
     */
    void set_default_timeout(std::chrono::milliseconds timeout)
    {
        m_default_timeout = timeout;
    }

    ComputeMatrix() = default;

private:
    /**
     * @brief Core execution implementation
     */
    template <typename OpClass, ComputeData InputType, ComputeData OutputType>
    std::optional<Datum<OutputType>> execute_operation(
        std::shared_ptr<OpClass> operation,
        const Datum<InputType>& input)
    {
        if (!operation)
            return std::nullopt;

        m_total_executions++;

        try {
            ExecutionContext ctx;
            configure_execution_context(ctx, std::type_index(typeid(OpClass)));

            auto start = std::chrono::steady_clock::now();

            auto result = operation->apply_operation_internal(input, ctx);

            if (m_profiling_enabled) {
                auto end = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
                update_execution_time(duration.count());
            }

            return result;

        } catch (const std::exception& e) {
            m_failed_executions++;
            handle_execution_error(e, std::type_index(typeid(OpClass)));
            return std::nullopt;
        }
    }

    /**
     * @brief Configure execution context based on operation type and policy
     */
    void configure_execution_context(ExecutionContext& ctx, const std::type_index& op_type)
    {
        switch (m_execution_policy) {
        case ExecutionPolicy::CONSERVATIVE:
        case ExecutionPolicy::BALANCED:
            ctx.mode = ExecutionMode::SYNC;
            break;
        case ExecutionPolicy::AGGRESSIVE:
            ctx.mode = ExecutionMode::PARALLEL;
            break;
        }

        ctx.timeout = m_default_timeout;

        if (m_context_configurator) {
            m_context_configurator(ctx, op_type);
        }

        // TODO: Add thread pool assignment, GPU context, etc.
    }

    /**
     * @brief Handle execution errors
     */
    void handle_execution_error(const std::exception& e, const std::type_index& op_type)
    {
        m_last_error = e.what();
        m_last_error_type = op_type;

        if (m_error_callback) {
            m_error_callback(e, op_type);
        }
    }

    /**
     * @brief Update execution time statistics
     */
    void update_execution_time(double ms)
    {
        double current_avg = m_average_execution_time.load();
        double total_execs = m_total_executions.load();
        double new_avg = (current_avg * (total_execs - 1) + ms) / total_execs;
        m_average_execution_time.store(new_avg);
    }

    void register_async(std::future<void> f)
    {
        std::lock_guard lk(m_async_mtx);

        std::erase_if(m_async_futures, [](std::future<void>& f) {
            return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
        });
        m_async_futures.push_back(std::move(f));
    }

    OperationPool m_operations;

    ExecutionPolicy m_execution_policy = ExecutionPolicy::BALANCED;
    std::chrono::milliseconds m_default_timeout { 0 };
    std::function<void(ExecutionContext&, const std::type_index&)> m_context_configurator;

    std::atomic<size_t> m_total_executions { 0 };
    std::atomic<size_t> m_failed_executions { 0 };
    std::atomic<double> m_average_execution_time { 0.0 };
    bool m_profiling_enabled = false;

    std::mutex m_async_mtx;
    std::vector<std::future<void>> m_async_futures;

    std::string m_last_error;
    std::type_index m_last_error_type { typeid(void) };
    std::function<void(const std::exception&, const std::type_index&)> m_error_callback;

public:
    /**
     * @brief Set error callback
     */
    void set_error_callback(
        std::function<void(const std::exception&, const std::type_index&)> callback)
    {
        m_error_callback = std::move(callback);
    }

    /**
     * @brief Get last error message
     */
    std::string get_last_error() const
    {
        return m_last_error;
    }

    /**
     * @brief Block until all in-flight async chains complete.
     *
     * Called automatically by the destructor. May also be called explicitly
     * before teardown of any resource a chain callback might write into.
     */
    void drain_async()
    {
        std::lock_guard lk(m_async_mtx);
        for (auto& f : m_async_futures) {
            f.wait();
        }

        m_async_futures.clear();
    }

    ~ComputeMatrix()
    {
        drain_async();
    }
};

} // namespace MayaFlux::Yantra
