#pragma once

#include <typeindex>

namespace MayaFlux::Yantra {

/**
 * @enum OperationType
 * @brief Operation categories for organization and discovery
 */
enum class OperationType : uint8_t {
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
enum class ExecutionMode : uint8_t {
    SYNC, ///< Synchronous execution
    ASYNC, ///< Asynchronous execution
    PARALLEL, ///< Parallel with other operations
    CHAINED, ///< Part of a sequential chain
    DEPENDENCY ///< Part of dependency graph
};

/**
 * @brief Callback type for pre/post operation hooks
 */
using OpererationHookCallback = std::function<void(std::any&)>;

/**
 * @brief Callback type for custom reconstruction logic
 */
using ReconstructionCallback = std::function<std::any(std::vector<std::vector<double>>&, std::any&)>;

/**
 * @struct ExecutionContext
 * @brief Context information for operation execution
 */
struct MAYAFLUX_API ExecutionContext {
    ExecutionMode mode = ExecutionMode::SYNC;
    std::shared_ptr<std::thread> thread_pool = nullptr;
    std::vector<std::type_index> dependencies;
    std::chrono::milliseconds timeout { 0 };
    std::unordered_map<std::string, std::any> execution_metadata;

    OpererationHookCallback pre_execution_hook = nullptr;
    OpererationHookCallback post_execution_hook = nullptr;
    ReconstructionCallback reconstruction_callback = nullptr;
};

}
