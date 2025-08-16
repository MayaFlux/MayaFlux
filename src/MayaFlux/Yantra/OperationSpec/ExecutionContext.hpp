#pragma once

#include <typeindex>

namespace MayaFlux::Yantra {

/**
 * @enum OperationType
 * @brief Operation categories for organization and discovery
 */
enum class OperationType : u_int8_t {
    ANALYZER,
    SORTER,
    EXTRACTOR,
    TRANSFORMER,
    CUSTOM
};

/**
 * @enum ExecutionMode
 * @brief Execution paradigms for operations
 */
enum class ExecutionMode : u_int8_t {
    SYNC, ///< Synchronous execution
    ASYNC, ///< Asynchronous execution
    PARALLEL, ///< Parallel with other operations
    CHAINED, ///< Part of a sequential chain
    DEPENDENCY ///< Part of dependency graph
};

/**
 * @struct ExecutionContext
 * @brief Context information for operation execution
 */
struct ExecutionContext {
    ExecutionMode mode = ExecutionMode::SYNC;
    std::shared_ptr<std::thread> thread_pool = nullptr;
    std::vector<std::type_index> dependencies;
    std::chrono::milliseconds timeout { 0 };
    std::unordered_map<std::string, std::any> execution_metadata;
};

}
