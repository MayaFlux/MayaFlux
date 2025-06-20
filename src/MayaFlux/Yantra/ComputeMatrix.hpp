#pragma once
#include "MayaFlux/Utils.hpp"

/**
 * @namespace MayaFlux::Yantra
 * @brief Computational framework for data processing and transformation operations
 */
namespace MayaFlux::Yantra {

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
template <typename InputType, typename OutputType = InputType>
class ComputeOperation {
public:
    /**
     * @brief Virtual destructor for proper cleanup of derived classes
     */
    virtual ~ComputeOperation() = default;

    /**
     * @brief Executes the computational transformation on the input data
     * @param input Data to be processed
     * @return Transformed output data
     */
    virtual OutputType apply_operation(InputType input) = 0;

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
    virtual std::any get_parameter(const std::string& name) const = 0;

    /**
     * @brief Retrieves all parameters and their values
     * @return Map of parameter names to their values
     */
    virtual std::map<std::string, std::any> get_all_parameters() const { return {}; }

    /**
     * @brief Validates if the input data meets the operation's requirements
     * @param input Data to validate
     * @return True if input is valid, false otherwise
     */
    virtual bool validate_input(const InputType& input) const { return true; }
};

/**
 * @class OperationChain
 * @brief Sequential composition of operations forming a processing pipeline
 *
 * Chains multiple operations where the output of one operation becomes the input
 * to the next, enabling complex data transformations through simple composable units.
 *
 * @tparam InputType The data type accepted by the first operation in the chain
 * @tparam OutputType The data type produced by the last operation in the chain
 */
template <typename InputType, typename OutputType>
class OperationChain : public ComputeOperation<InputType, OutputType> {
public:
    /**
     * @brief Adds an operation to the end of the processing chain
     * @param operation The operation to append to the chain
     */
    void add_operation(std::shared_ptr<ComputeOperation<std::any, std::any>> operation);

    /**
     * @brief Processes input data through the entire operation chain
     * @param input Data to be processed by the chain
     * @return Result after passing through all operations in the chain
     */
    virtual OutputType apply_operation(InputType input) override;

    /**
     * @brief Removes all operations from the chain
     */
    void clear_operations();

    /**
     * @brief Gets the number of operations in the chain
     * @return Count of operations in the chain
     */
    size_t get_operation_count() const;

private:
    /** @brief Ordered sequence of operations in the chain */
    std::vector<std::shared_ptr<ComputeOperation<std::any, std::any>>> m_operations;
};

/**
 * @class ParallelOperations
 * @brief Concurrent execution of multiple operations on the same input
 *
 * Processes the same input data through multiple independent operations simultaneously,
 * collecting all results into a named map.
 *
 * @tparam InputType The data type accepted by all parallel operations
 */
template <typename InputType>
class ParallelOperations : public ComputeOperation<InputType, std::map<std::string, std::any>> {
public:
    /**
     * @brief Adds a named operation to the parallel execution set
     * @param name Identifier for the operation's result in the output map
     * @param operation The operation to execute in parallel
     */
    void add_operation(const std::string& name,
        std::shared_ptr<ComputeOperation<InputType, std::any>> operation);

    /**
     * @brief Processes input data through all registered operations concurrently
     * @param input Data to be processed by all operations
     * @return Map of operation names to their respective results
     */
    virtual std::map<std::string, std::any> apply_operation(InputType input) override;

private:
    /** @brief Named operations to execute in parallel */
    std::map<std::string, std::shared_ptr<ComputeOperation<InputType, std::any>>> m_operations;
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
    void register_operation(const std::string& name);

    /**
     * @brief Creates an instance of a registered operation type
     * @tparam T Expected operation class type
     * @param name Identifier of the registered operation type
     * @return Shared pointer to the created operation instance
     */
    template <typename T>
    std::shared_ptr<T> create_operation(const std::string& name);

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
        std::shared_ptr<ComputeOperation<IntermediateType, OutputType>> second);

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
        const InputType& input);

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
        const std::vector<InputType>& inputs);

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
        ChainBuilder& then(const std::string& operation_name);

        /**
         * @brief Finalizes and returns the constructed operation chain
         * @return Shared pointer to the complete operation chain
         */
        std::shared_ptr<ComputeOperation<InputType, OutputType>> build();

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
    ChainBuilder<InputType, OutputType> create_chain();

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
    std::optional<T> find_in_cache(const std::string& key);

    /**
     * @brief Stores a result in the cache
     * @tparam T Type of the result to cache
     * @param key Cache storage key
     * @param result Value to cache
     */
    template <typename T>
    void cache_result(const std::string& key, const T& result);
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
