#pragma once

#include <utility>

#include "ComputeOperation.hpp"

namespace MayaFlux::Yantra {

/**
 * @enum OperationType
 * @brief Operation categories for organization and discovery
 */
enum class OperationType : u_int8_t {
    ANALYZER,
    SORTER,
    EXTRACTOR,
    TRANSFORMER
};

/**
 * @struct TypeKey
 * @brief Type-based operation identification
 */
struct TypeKey {
    OperationType category;
    std::type_index operation_type;

    bool operator==(const TypeKey& other) const
    {
        return category == other.category && operation_type == other.operation_type;
    }
};

struct TypeKeyHash {
    std::size_t operator()(const TypeKey& key) const
    {
        return std::hash<int> {}(static_cast<int>(key.category)) ^ (std::hash<std::type_index> {}(key.operation_type) << 1);
    }
};

/**
 * @class ComputeMatrix
 * @brief Central orchestrator for computational operations
 *
 * Coordinates operation execution in all paradigms through the unified
 * ComputeOperation interface - no more wrappers!
 */
class ComputeMatrix : public std::enable_shared_from_this<ComputeMatrix> {
public:
    using Factory = std::function<std::any()>;

    // === REGISTRATION INTERFACE ===

    template <typename OpClass, ComputeData InputType, ComputeData OutputType = InputType>
    void register_analyzer(std::function<std::shared_ptr<OpClass>()> factory = nullptr)
    {
        register_operation<OpClass, InputType, OutputType>(OperationType::ANALYZER, factory);
    }

    template <typename OpClass, ComputeData InputType, ComputeData OutputType = InputType>
    void register_sorter(std::function<std::shared_ptr<OpClass>()> factory = nullptr)
    {
        register_operation<OpClass, InputType, OutputType>(OperationType::SORTER, factory);
    }

    template <typename OpClass, ComputeData InputType, ComputeData OutputType = InputType>
    void register_extractor(std::function<std::shared_ptr<OpClass>()> factory = nullptr)
    {
        register_operation<OpClass, InputType, OutputType>(OperationType::EXTRACTOR, factory);
    }

    template <typename OpClass, ComputeData InputType, ComputeData OutputType = InputType>
    void register_transformer(std::function<std::shared_ptr<OpClass>()> factory = nullptr)
    {
        register_operation<OpClass, InputType, OutputType>(OperationType::TRANSFORMER, factory);
    }

    // === OPERATION POOL INTERFACE ===

    /**
     * @brief Add an operation instance to the pool with a name
     */
    template <typename OpClass>
    void add_operation(const std::string& name, std::shared_ptr<OpClass> op)
    {
        m_operation_pool[name] = std::static_pointer_cast<void>(op);
        m_operation_types[name] = std::type_index(typeid(OpClass));
    }

    /**
     * @brief Get an operation instance from the pool
     */
    template <typename OpClass>
    std::shared_ptr<OpClass> get_operation(const std::string& name)
    {
        auto it = m_operation_pool.find(name);
        if (it == m_operation_pool.end()) {
            return nullptr;
        }

        // Verify type safety
        auto type_it = m_operation_types.find(name);
        if (type_it != m_operation_types.end() && type_it->second != std::type_index(typeid(OpClass))) {
            return nullptr; // Type mismatch
        }

        return std::static_pointer_cast<OpClass>(it->second);
    }

    /**
     * @brief Remove operation from pool
     */
    void remove_operation(const std::string& name)
    {
        m_operation_pool.erase(name);
        m_operation_types.erase(name);
    }

    /**
     * @brief List all operation names in pool
     */
    std::vector<std::string> list_operations() const
    {
        std::vector<std::string> names;
        names.reserve(m_operation_pool.size());
        for (const auto& [name, _] : m_operation_pool) {
            names.push_back(name);
        }
        return names;
    }

    // === EXECUTION INTERFACE ===

    /**
     * @brief Synchronous execution
     */
    template <typename OpClass, ComputeData InputType, ComputeData OutputType = InputType>
    std::optional<IO<OutputType>> execute(const InputType& input)
    {
        auto op = create<OpClass>();
        return execute_internal(op, input);
    }

    /**
     * @brief Synchronous execution with named operation
     */
    template <typename OpClass, ComputeData InputType, ComputeData OutputType = InputType>
    std::optional<IO<OutputType>> execute(const std::string& name, const InputType& input)
    {
        auto op = get_operation<OpClass>(name);
        return execute_internal(op, input);
    }

    /**
     * @brief Asynchronous execution
     */
    template <typename OpClass, ComputeData InputType, ComputeData OutputType = InputType>
    std::future<std::optional<IO<OutputType>>> execute_async(const InputType& input)
    {
        auto op = create<OpClass>();
        return execute_async_internal<OpClass, InputType, OutputType>(op, input);
    }

    /**
     * @brief Asynchronous execution with named operation
     */
    template <typename OpClass, ComputeData InputType, ComputeData OutputType = InputType>
    std::future<std::optional<IO<OutputType>>> execute_async(const std::string& name, const InputType& input)
    {
        auto op = get_operation<OpClass>(name);
        return execute_async_internal<OpClass, InputType, OutputType>(op, input);
    }

    /**
     * @brief Parallel execution with operation types - each can have different output
     */
    template <ComputeData InputType, typename... OpClasses>
    auto execute_parallel(const InputType& input)
    {
        return execute_parallel_internal<InputType, OpClasses...>(input);
    }

    /**
     * @brief Parallel execution with named operations - same output type
     */
    template <typename OpClass, ComputeData InputType, ComputeData OutputType = InputType>
    std::vector<IO<OutputType>> execute_parallel(const std::vector<std::string>& names, const InputType& input)
    {
        std::vector<std::shared_ptr<OpClass>> ops;
        for (const auto& name : names) {
            auto op = get_operation<OpClass>(name);
            if (op)
                ops.push_back(op);
        }
        return execute_parallel_internal<OpClass, InputType, OutputType>(ops, input);
    }

    /**
     * @brief Chain execution - type-safe sequential operations
     */
    template <typename FirstOp, typename SecondOp, ComputeData InputType, ComputeData IntermediateType, ComputeData OutputType>
    std::optional<IO<OutputType>> execute_chain(const InputType& input)
    {
        auto first_op = create<FirstOp>();
        auto second_op = create<SecondOp>();
        return execute_chain_internal<FirstOp, SecondOp, InputType, IntermediateType, OutputType>(first_op, second_op, input);
    }

    /**
     * @brief Chain execution with named operations
     */
    template <typename FirstOp, typename SecondOp, ComputeData InputType, ComputeData IntermediateType, ComputeData OutputType>
    std::optional<IO<OutputType>> execute_chain(const std::string& first_name, const std::string& second_name, const InputType& input)
    {
        auto first_op = get_operation<FirstOp>(first_name);
        auto second_op = get_operation<SecondOp>(second_name);
        return execute_chain_internal<FirstOp, SecondOp, InputType, IntermediateType, OutputType>(first_op, second_op, input);
    }

    /**
     * @brief Chain execution with mixed (first from factory, second named)
     */
    template <typename FirstOp, typename SecondOp, ComputeData InputType, ComputeData IntermediateType, ComputeData OutputType>
    std::optional<IO<OutputType>> execute_chain_mixed(const std::string& second_name, const InputType& input)
    {
        auto first_op = create<FirstOp>();
        auto second_op = get_operation<SecondOp>(second_name);
        return execute_chain_internal<FirstOp, SecondOp, InputType, IntermediateType, OutputType>(first_op, second_op, input);
    }

    // === FLUENT INTERFACE ===

    template <ComputeData StartType>
    auto with(const StartType& input)
    {
        return FluentChain<StartType> { shared_from_this(), input };
    }

    // === DISCOVERY INTERFACE ===

    template <typename OpClass>
    bool available() const
    {
        TypeKey key { get_category<OpClass>(), std::type_index(typeid(OpClass)) };
        return m_factories.contains(key);
    }

    template <ComputeData InputType, ComputeData OutputType = InputType>
    std::vector<std::type_index> discover(OperationType category) const
    {
        std::vector<std::type_index> result;
        std::type_index input_idx(typeid(InputType));
        std::type_index output_idx(typeid(OutputType));

        for (const auto& [key, type_info] : m_type_info) {
            if (key.category == category && type_info.first == input_idx && type_info.second == output_idx) {
                result.push_back(key.operation_type);
            }
        }
        return result;
    }

    static std::shared_ptr<ComputeMatrix> instance()
    {
        static std::shared_ptr<ComputeMatrix> inst(new ComputeMatrix());
        return inst;
    }

private:
    std::unordered_map<TypeKey, Factory, TypeKeyHash> m_factories;
    std::unordered_map<TypeKey, std::pair<std::type_index, std::type_index>, TypeKeyHash> m_type_info;

    std::unordered_map<std::string, std::shared_ptr<void>> m_operation_pool;
    std::unordered_map<std::string, std::type_index> m_operation_types;

    template <typename OpClass>
    std::shared_ptr<OpClass> create()
    {
        TypeKey key { get_category<OpClass>(), std::type_index(typeid(OpClass)) };
        auto it = m_factories.find(key);
        if (it == m_factories.end())
            return nullptr;
        return std::any_cast<std::shared_ptr<OpClass>>(it->second());
    }

    template <typename OpClass, ComputeData InputType, ComputeData OutputType>
    void register_operation(OperationType category, std::function<std::shared_ptr<OpClass>()> factory)
    {
        if (!factory)
            factory = []() { return std::make_shared<OpClass>(); };

        TypeKey key { .category = category, .operation_type = std::type_index(typeid(OpClass)) };
        m_factories[key] = [factory]() -> std::any {
            return std::any(factory());
        };
        m_type_info[key] = { std::type_index(typeid(InputType)), std::type_index(typeid(OutputType)) };
    }

    template <typename OpClass>
    OperationType get_category() const
    {
        for (const auto& [key, _] : m_factories) {
            if (key.operation_type == std::type_index(typeid(OpClass))) {
                return key.category;
            }
        }
        return OperationType::ANALYZER;
    }

    template <typename OpClass, ComputeData InputType, ComputeData OutputType = InputType>
    std::optional<IO<OutputType>> execute_internal(std::shared_ptr<OpClass> op, const InputType& input)
    {
        if (!op)
            return std::nullopt;

        try {
            IO<InputType> input_wrapper(input);
            ExecutionContext ctx { .mode = ExecutionMode::SYNC };
            return op->apply_operation_internal(input_wrapper, ctx);
        } catch (const std::exception&) {
            return std::nullopt;
        }
    }

    /**
     * @brief Internal parallel execution - variadic template version
     */
    template <ComputeData InputType, typename... OpClasses>
    auto execute_parallel_internal(const InputType& input)
    {
        auto run_op = [input](auto op_ptr) {
            if (!op_ptr)
                return decltype(op_ptr->apply_operation(IO<InputType> { input })) {};

            IO<InputType> input_wrapper(input);
            ExecutionContext ctx { .mode = ExecutionMode::PARALLEL };
            return op_ptr->apply_operation_internal(input_wrapper, ctx);
        };

        auto futures = std::make_tuple(
            std::async(std::launch::async, run_op, create<OpClasses>())...);

        return std::make_tuple(std::get<OpClasses>(futures).get()...);
    }

    /**
     * @brief Internal parallel execution - same operation type version
     */
    template <typename OpClass, ComputeData InputType, ComputeData OutputType>
    std::vector<IO<OutputType>> execute_parallel_internal(
        const std::vector<std::shared_ptr<OpClass>>& ops,
        const InputType& input)
    {
        std::vector<std::future<IO<OutputType>>> futures;

        std::transform(ops.begin(), ops.end(), std::back_inserter(futures),
            [input](auto& op) {
                return std::async(std::launch::async, [op, input]() -> IO<OutputType> {
                    if (!op)
                        return IO<OutputType> {};

                    IO<InputType> input_wrapper(input);
                    ExecutionContext ctx { .mode = ExecutionMode::PARALLEL };
                    return op->apply_operation_internal(input_wrapper, ctx);
                });
            });

        std::vector<IO<OutputType>> results;
        std::transform(futures.begin(), futures.end(), std::back_inserter(results),
            [](auto& future) {
                try {
                    return future.get();
                } catch (const std::exception&) {
                    return IO<OutputType> {};
                }
            });

        return results;
    }

    /**
     * @brief Asynchronous execution
     */
    template <typename OpClass, ComputeData InputType, ComputeData OutputType = InputType>
    std::future<std::optional<IO<OutputType>>> execute_async_internal(std::shared_ptr<OpClass> op, const InputType& input)
    {
        return std::async(std::launch::async, [this, input, op]() -> std::optional<IO<OutputType>> {
            if (!op)
                return std::nullopt;

            try {
                IO<InputType> input_wrapper(input);
                ExecutionContext ctx { .mode = ExecutionMode::ASYNC };
                return op->apply_operation_internal(input_wrapper, ctx);
            } catch (const std::exception&) {
                return std::nullopt;
            }
        });
    }

    /**
     * @brief Internal chain execution implementation
     */
    template <typename FirstOp, typename SecondOp, ComputeData InputType, ComputeData IntermediateType, ComputeData OutputType>
    std::optional<IO<OutputType>> execute_chain_internal(
        std::shared_ptr<FirstOp> first_op,
        std::shared_ptr<SecondOp> second_op,
        const InputType& input)
    {
        if (!first_op || !second_op)
            return std::nullopt;

        try {
            IO<InputType> input_wrapper(input);
            ExecutionContext chain_ctx { .mode = ExecutionMode::CHAINED };

            // Execute first operation
            auto intermediate = first_op->apply_operation_internal(input_wrapper, chain_ctx);
            if (!intermediate)
                return std::nullopt;

            // Execute second operation with intermediate result
            IO<IntermediateType> intermediate_wrapper(intermediate->data);
            intermediate_wrapper.metadata = intermediate->metadata; // Thread metadata

            return second_op->apply_operation_internal(intermediate_wrapper, chain_ctx);
        } catch (const std::exception&) {
            return std::nullopt;
        }
    }

    template <ComputeData DataType>
    class FluentChain {
    public:
        FluentChain(std::shared_ptr<ComputeMatrix> matrix, const DataType& input)
            : m_matrix(std::move(matrix))
            , m_input(input)
        {
        }

        template <typename OpClass, ComputeData OutputType = DataType>
        auto then()
        {
            auto result = m_matrix->execute<OpClass, DataType, OutputType>(m_input);
            if (result) {
                return FluentChain<OutputType> { m_matrix, result->data };
            }
            throw std::runtime_error("Operation failed in chain");
        }

        template <typename OpClass, ComputeData OutputType = DataType>
        auto then(const std::string& name)
        {
            auto result = m_matrix->execute<OpClass, DataType, OutputType>(name, m_input);
            if (result) {
                return FluentChain<OutputType> { m_matrix, result->data };
            }
            throw std::runtime_error("Named operation failed in chain");
        }

        const DataType& get() const { return m_input; }

    private:
        std::shared_ptr<ComputeMatrix> m_matrix;
        DataType m_input;
    };

    friend class std::shared_ptr<ComputeMatrix>;
};

} // namespace MayaFlux::Yantra
