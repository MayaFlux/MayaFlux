#pragma once
#include "ComputeOperation.hpp"

/**
 * @namespace MayaFlux::Yantra
 * @brief Computational framework for data processing and transformation operations
 */
namespace MayaFlux::Yantra {

/**
 * @class TypeErasedOperationWrapper
 * @brief Wrapper that converts strongly-typed operations to type-erased operations
 *
 * This wrapper allows operations with specific input/output types to be stored
 * in the ComputeMatrix registry which expects ComputeOperation<std::any, std::any>.
 * It handles the conversion between std::any and the concrete types.
 *
 * @tparam T The concrete operation type to wrap
 */
template <typename T>
class TypeErasedOperationWrapper : public ComputeOperation<std::any, std::any> {
private:
    std::shared_ptr<T> m_wrapped;

public:
    TypeErasedOperationWrapper()
        : m_wrapped(std::make_shared<T>())
    {
    }

    explicit TypeErasedOperationWrapper(std::shared_ptr<T> wrapped)
        : m_wrapped(std::move(wrapped))
    {
    }

    std::any apply_operation(std::any input) override
    {
        using InputType = typename T::input_type;
        using OutputType = typename T::output_type;

        try {
            auto typed_input = std::any_cast<InputType>(input);
            auto result = m_wrapped->apply_operation(typed_input);
            return std::any(result);

        } catch (const std::bad_any_cast& e) {
            throw std::runtime_error(
                std::string("Type mismatch in TypeErasedOperationWrapper: ") + e.what() + "\nExpected type: " + typeid(InputType).name() + "\nActual type: " + input.type().name());
        }
    }

    void set_parameter(const std::string& name, std::any value) override
    {
        m_wrapped->set_parameter(name, value);
    }

    std::any get_parameter(const std::string& name) const override
    {
        return m_wrapped->get_parameter(name);
    }

    std::map<std::string, std::any> get_all_parameters() const override
    {
        return m_wrapped->get_all_parameters();
    }

    bool validate_input(const std::any& input) const override
    {
        using InputType = typename T::input_type;

        // Check if the input can be cast to the expected type
        try {
            auto typed_input = std::any_cast<InputType>(input);
            return m_wrapped->validate_input(typed_input);
        } catch (const std::bad_any_cast&) {
            return false;
        }
    }

    // Provide access to the wrapped operation for debugging/introspection
    std::shared_ptr<T> get_wrapped() const { return m_wrapped; }
};

/**
 * @class ComputeMatrix
 * @brief Central registry and factory for computational operations
 *
 * Manages operation registration, instantiation, composition, and result caching.
 * Provides a fluent interface for building operation chains and pipelines.
 */
class ComputeMatrix : public std::enable_shared_from_this<ComputeMatrix> {
public:
    /**
     * @brief Registers an operation type with a unique name
     * @tparam T Operation class type to register
     * @param name Unique identifier for the operation type
     */
    template <typename T>
    void register_operation(const std::string& name)
    {
        m_operation_factories[name] = []() -> std::shared_ptr<ComputeOperation<std::any, std::any>> {
            // return std::static_pointer_cast<ComputeOperation<std::any, std::any>>(std::make_shared<T>());
            return std::make_shared<TypeErasedOperationWrapper<T>>();
        };
    }

    /**
     * @brief Creates an instance of a registered operation type
     * @tparam T Expected operation class type
     * @param name Identifier of the registered operation type
     * @return Shared pointer to the created operation instance
     */
    template <typename T>
    std::shared_ptr<T> create_operation(const std::string& name)
    {
        auto it = m_operation_factories.find(name);
        if (it == m_operation_factories.end()) {
            return nullptr;
        }

        auto base_operation = it->second();
        return std::dynamic_pointer_cast<T>(base_operation);
    }

    /**
     * @brief Creates a two-stage pipeline by connecting two operations
     * @tparam InputType Data type accepted by the first operation
     * @tparam IntermediateType Data type passed between operations
     * @tparam OutputType Data type produced by the second operation
     * @param first First operation in the pipeline
     * @param second Second operation in the pipeline
     * @return Composite operation representing the pipeline
     */
    template <typename InputType, typename IntermediateType, typename OutputType>
    std::shared_ptr<ComputeOperation<InputType, OutputType>>
    create_pipeline(std::shared_ptr<ComputeOperation<InputType, IntermediateType>> first,
        std::shared_ptr<ComputeOperation<IntermediateType, OutputType>> second)
    {
        auto pipeline = std::make_shared<OperationChain<InputType, OutputType>>();

        auto first_any = std::static_pointer_cast<ComputeOperation<std::any, std::any>>(
            std::shared_ptr<ComputeOperation<InputType, IntermediateType>>(first));

        auto second_any = std::static_pointer_cast<ComputeOperation<std::any, std::any>>(
            std::shared_ptr<ComputeOperation<IntermediateType, OutputType>>(second));

        pipeline->add_operation(first_any);
        pipeline->add_operation(second_any);

        return pipeline;
    }

    /**
     * @brief Retrieves a cached result for an operation and input, computing if not found
     * @tparam InputType Type of the operation input
     * @tparam OutputType Type of the operation output
     * @param operation_id Identifier for the operation
     * @param input Input data for the operation
     * @return Cached or newly computed result
     */
    template <typename InputType, typename OutputType>
    OutputType get_cached_result(const std::string& operation_id,
        const InputType& input)
    {
        std::string cache_key = operation_id + "_" + std::to_string(std::hash<InputType> {}(input));

        auto cached_result = find_in_cache<OutputType>(cache_key);
        if (cached_result) {
            return *cached_result;
        }

        auto operation = create_operation<ComputeOperation<InputType, OutputType>>(operation_id);
        if (!operation) {
            throw std::runtime_error("Operation not found: " + operation_id);
        }

        OutputType result = operation->apply_operation(input);

        cache_result(cache_key, result);

        return result;
    }

    /**
     * @brief Processes a batch of inputs through the same operation
     * @tparam InputType Type of the operation inputs
     * @tparam OutputType Type of the operation outputs
     * @param operation_id Identifier for the operation
     * @param inputs Collection of input data to process
     * @return Collection of corresponding output results
     */
    template <typename InputType, typename OutputType>
    std::vector<OutputType> process_batch(const std::string& operation_id,
        const std::vector<InputType>& inputs)
    {
        std::vector<OutputType> results;
        results.reserve(inputs.size());

        auto operation = create_operation<ComputeOperation<InputType, OutputType>>(operation_id);
        if (!operation) {
            throw std::runtime_error("Operation not found: " + operation_id);
        }

        for (const auto& input : inputs) {
            results.push_back(operation->apply_operation(input));
        }

        return results;
    }

    /**
     * @class ChainBuilder
     * @brief Fluent interface for constructing operation chains
     *
     * Provides a declarative API for building sequential operation chains
     * with type safety between connected operations.
     *
     * @tparam InputType Type accepted by the first operation in the chain
     * @tparam OutputType Type produced by the last operation in the chain
     */
    template <typename InputType, typename OutputType>
    class ChainBuilder {
    public:
        /**
         * @brief Constructs a chain builder associated with a compute matrix
         * @param matrix Parent compute matrix for operation creation
         */
        ChainBuilder(ComputeMatrix* matrix)
            : m_matrix(matrix)
        {
        }

        /**
         * @brief Adds the next operation to the chain
         * @tparam IntermediateType Expected output type of the added operation
         * @param operation_name Identifier of the registered operation to add
         * @return Reference to this builder for method chaining
         */
        template <typename IntermediateType>
        ChainBuilder& then(const std::string& operation_name)
        {
            if (!m_chain) {
                m_chain = std::make_shared<OperationChain<InputType, OutputType>>();
            }

            auto operation = m_matrix->create_operation<ComputeOperation<InputType, IntermediateType>>(operation_name);
            if (!operation) {
                throw std::runtime_error("Operation not found: " + operation_name);
            }

            auto op_any = std::static_pointer_cast<ComputeOperation<std::any, std::any>>(operation);
            m_chain->add_operation(op_any);

            return *this;
        }

        /**
         * @brief Finalizes and returns the constructed operation chain
         * @return Shared pointer to the complete operation chain
         */
        std::shared_ptr<ComputeOperation<InputType, OutputType>> build()
        {
            if (!m_chain) {
                m_chain = std::make_shared<OperationChain<InputType, OutputType>>();
            }
            return m_chain;
        }

    private:
        /** @brief Parent compute matrix for operation creation */
        ComputeMatrix* m_matrix;

        /** @brief The operation chain being constructed */
        std::shared_ptr<OperationChain<InputType, OutputType>> m_chain;
    };

    /**
     * @brief Creates a new chain builder for constructing operation sequences
     * @tparam InputType Type accepted by the first operation in the chain
     * @tparam OutputType Type produced by the last operation in the chain
     * @return Chain builder instance for fluent chain construction
     */
    template <typename InputType, typename OutputType>
    ChainBuilder<InputType, OutputType> create_chain()
    {
        return ChainBuilder<InputType, OutputType>(this);
    }

    /**
     * @brief Gets or creates the singleton instance of ComputeMatrix
     * @return Shared pointer to the global ComputeMatrix instance
     */
    inline static std::shared_ptr<ComputeMatrix> get_instance()
    {
        static std::shared_ptr<ComputeMatrix> instance(new ComputeMatrix());
        return instance;
    }

private:
    /** @brief Registry of operation factories indexed by name */
    std::unordered_map<std::string, std::function<std::shared_ptr<ComputeOperation<std::any, std::any>>()>> m_operation_factories;

    /** @brief Cache of operation results indexed by operation and input hash */
    std::unordered_map<std::string, std::any> m_result_cache;

    /** @brief Mutex for thread-safe cache access */
    std::mutex m_cache_mutex;

    /**
     * @brief Attempts to find a cached result
     * @tparam T Expected type of the cached result
     * @param key Cache lookup key
     * @return Optional containing the cached value if found
     */
    template <typename T>
    std::optional<T> find_in_cache(const std::string& key)
    {
        std::lock_guard<std::mutex> lock(m_cache_mutex);

        auto it = m_result_cache.find(key);
        if (it != m_result_cache.end()) {
            try {
                return std::any_cast<T>(it->second);
            } catch (const std::bad_any_cast&) {
                return std::nullopt;
            }
        }

        return std::nullopt;
    }

    /**
     * @brief Stores a result in the cache
     * @tparam T Type of the result to cache
     * @param key Cache storage key
     * @param result Value to cache
     */
    template <typename T>
    void cache_result(const std::string& key, const T& result)
    {
        std::lock_guard<std::mutex> lock(m_cache_mutex);
        m_result_cache[key] = result;
    }
};

/**
 * @def REGISTER_OPERATION(Matrix, Name, Type)
 * @brief Convenience macro for registering operations with a compute matrix
 * @param Matrix Pointer to the compute matrix
 * @param Name String identifier for the operation
 * @param Type C++ type of the operation class
 */
#define REGISTER_OPERATION(Matrix, Name, Type) \
    Matrix->register_operation<Type>(Name)
}
