#include "ComputeMatrix.hpp"

namespace MayaFlux::Yantra {

template <typename T>
void ComputeMatrix::register_operation(const std::string& name)
{
    m_operation_factories[name] = []() -> std::shared_ptr<ComputeOperation<std::any, std::any>> {
        return std::static_pointer_cast<ComputeOperation<std::any, std::any>>(std::make_shared<T>());
    };
}

template <typename T>
std::shared_ptr<T> ComputeMatrix::create_operation(const std::string& name)
{
    auto it = m_operation_factories.find(name);
    if (it == m_operation_factories.end()) {
        return nullptr;
    }

    auto base_operation = it->second();
    return std::dynamic_pointer_cast<T>(base_operation);
}

template <typename InputType, typename IntermediateType, typename OutputType>
std::shared_ptr<ComputeOperation<InputType, OutputType>> ComputeMatrix::create_pipeline(std::shared_ptr<ComputeOperation<InputType, IntermediateType>> first, std::shared_ptr<ComputeOperation<IntermediateType, OutputType>> second)
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

template <typename InputType, typename OutputType>
OutputType ComputeMatrix::get_cached_result(const std::string& operation_id, const InputType& input)
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

template <typename InputType, typename OutputType>
std::vector<OutputType> ComputeMatrix::process_batch(const std::string& operation_id, const std::vector<InputType>& inputs)
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

template <typename InputType, typename OutputType>
ComputeMatrix::ChainBuilder<InputType, OutputType> ComputeMatrix::create_chain()
{
    return ChainBuilder<InputType, OutputType>(this);
}

template <typename InputType, typename OutputType>
template <typename IntermediateType>
ComputeMatrix::ChainBuilder<InputType, OutputType>&
ComputeMatrix::ChainBuilder<InputType, OutputType>::then(const std::string& operation_name)
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

template <typename InputType, typename OutputType>
std::shared_ptr<ComputeOperation<InputType, OutputType>>
ComputeMatrix::ChainBuilder<InputType, OutputType>::build()
{
    if (!m_chain) {
        m_chain = std::make_shared<OperationChain<InputType, OutputType>>();
    }
    return m_chain;
}

template <typename T>
std::optional<T> ComputeMatrix::find_in_cache(const std::string& key)
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

template <typename T>
void ComputeMatrix::cache_result(const std::string& key, const T& result)
{
    std::lock_guard<std::mutex> lock(m_cache_mutex);
    m_result_cache[key] = result;
}

template <typename InputType, typename OutputType>
void OperationChain<InputType, OutputType>::add_operation(
    std::shared_ptr<ComputeOperation<std::any, std::any>> operation)
{
    m_operations.push_back(operation);
}

template <typename InputType, typename OutputType>
OutputType OperationChain<InputType, OutputType>::apply_operation(InputType input)
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

template <typename InputType, typename OutputType>
void OperationChain<InputType, OutputType>::clear_operations()
{
    m_operations.clear();
}

template <typename InputType, typename OutputType>
size_t OperationChain<InputType, OutputType>::get_operation_count() const
{
    return m_operations.size();
}

// Implement ParallelOperations methods
template <typename InputType>
void ParallelOperations<InputType>::add_operation(
    const std::string& name, std::shared_ptr<ComputeOperation<InputType, std::any>> operation)
{
    m_operations[name] = operation;
}

template <typename InputType>
std::map<std::string, std::any> ParallelOperations<InputType>::apply_operation(InputType input)
{
    std::map<std::string, std::any> results;

    for (const auto& [name, operation] : m_operations) {
        results[name] = operation->apply_operation(input);
    }

    return results;
}
}
