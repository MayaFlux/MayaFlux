#pragma once

#include "MayaFlux/Utils.hpp"

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
    using input_type = InputType;
    using output_type = OutputType;

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
    virtual bool validate_input(const InputType&) const { return true; }
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
    inline void add_operation(std::shared_ptr<ComputeOperation<std::any, std::any>> operation)
    {
        m_operations.push_back(operation);
    }

    /**
     * @brief Processes input data through the entire operation chain
     * @param input Data to be processed by the chain
     * @return Result after passing through all operations in the chain
     */
    virtual OutputType apply_operation(InputType input) override
    {
        if (m_operations.empty()) {
            throw std::runtime_error("Operation chain is empty");
        }

        std::any current = input;

        for (auto& operation : m_operations) {
            current = operation->apply_operation(current);
        }

        try {
            return std::any_cast<OutputType>(current);
        } catch (const std::bad_any_cast&) {
            throw std::runtime_error("Type mismatch in operation chain result");
        }
    }

    /**
     * @brief Removes all operations from the chain
     */
    inline void clear_operations() { m_operations.clear(); }

    /**
     * @brief Gets the number of operations in the chain
     * @return Count of operations in the chain
     */
    inline size_t get_operation_count() const { return m_operations.size(); }

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
    inline void add_operation(const std::string& name,
        std::shared_ptr<ComputeOperation<InputType, std::any>> operation)
    {
        m_operations[name] = operation;
    }

    /**
     * @brief Processes input data through all registered operations concurrently
     * @param input Data to be processed by all operations
     * @return Map of operation names to their respective results
     */
    virtual std::map<std::string, std::any> apply_operation(InputType input) override
    {
        std::map<std::string, std::any> results;

        for (const auto& [name, operation] : m_operations) {
            results[name] = operation->apply_operation(input);
        }

        return results;
    }

private:
    /** @brief Named operations to execute in parallel */
    std::map<std::string, std::shared_ptr<ComputeOperation<InputType, std::any>>> m_operations;
};
}
