#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"
#include "ComputationSignature.hpp"
#include "ExecutionTarget.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Transitive/Facet/Facets.hpp"

#include <typeindex>

namespace MayaFlux::Yantra {

struct DependencyStage;

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
    CHAINED_INDIRECT, ///< Chained dispatch where a GPU-written indirect buffer gates each pass's workgroup count.
    DEPENDENCY ///< Part of dependency graph
};

/**
 * @brief Callback type for pre/post operation hooks
 */
using OperationHookCallback = std::function<void(std::any&)>;

/**
 * @brief Callback type for custom reconstruction logic
 */
using ReconstructionCallback = std::function<std::any(std::vector<std::vector<double>>&, std::any&)>;

/**
 * @brief Parameters for ExecutionMode::CHAINED.
 */
struct ChainedParams {
    uint32_t pass_count;
    std::function<void(uint32_t, void*)> pc_updater;
    std::optional<uint32_t> passes_per_batch;
};

/**
 * @brief Parameters for ExecutionMode::CHAINED_INDIRECT.
 *
 * The indirect binding itself is discovered from declare_buffer_bindings
 * via usage_hint == INDIRECT, not carried here.
 */
struct ChainedIndirectParams {
    uint32_t pass_count;
    std::function<void(uint32_t, uint32_t, void*)> pc_updater;
    std::optional<uint32_t> passes_per_batch;
};

using ExecutionParams = std::variant<
    std::monostate,
    ChainedParams,
    ChainedIndirectParams>;

/**
 * @struct Permit
 * @brief Targets a call may run on. Stored by type in ExecutionContext::facets.
 *
 * Without a Permit every target is allowed. A rule whose target does not meet
 * the permitted one is skipped.
 */
struct Permit {
    Target allowed;
};

MF_DESCRIBE(Permit)

/**
 * @struct ExecutionContext
 * @brief Context information controlling how a compute operation executes.
 *
 * ExecutionContext provides typed settings, dependency hints, and hooks
 * that influence how a Yantra operation is scheduled and run.
 *
 * Settings for a call are described structs in `facets`, read back by type.
 *
 * Typical usage:
 *
 * @code
 * ExecutionContext ctx;
 *
 * ctx.facets.set(SegmentConfig { .grain_size = 1024, .hop_size = 512 });
 * ctx.depends_on<MyAnalyzer>();
 * @endcode
 */
struct MAYAFLUX_API ExecutionContext {

    /** @brief Operation selections being resolved by the grammar. */
    ComputationSignature signature;

    /**
     * @brief Execution mode controlling scheduling behavior.
     */
    ExecutionMode mode = ExecutionMode::SYNC;

    /**
     * @brief Optional thread pool for asynchronous or parallel execution.
     */
    std::shared_ptr<std::thread> thread_pool = nullptr;

    /**
     * @brief Operation dependencies required before execution.
     *
     * Stores type identifiers for operations that must complete before
     * this context's operation may run.
     */
    std::vector<std::type_index> dependencies;

    /**
     * @brief Optional timeout for operation execution.
     */
    std::chrono::milliseconds timeout { 0 };

    /**
     * @brief Optional parameters specific to the execution mode.
     *
     * For example, `ChainedParams` for ExecutionMode::CHAINED,
     * `ChainedIndirectParams` for ExecutionMode::CHAINED_INDIRECT.
     */
    ExecutionParams parameters;

    /**
     * @brief Typed settings for this call, stored as described structs and read by type.
     *
     * Entries are looked up by type, not by name. They belong to the call and
     * do not travel with the data.
     */
    Facets facets;

    /**
     * @brief Optional callback invoked before operation execution.
     */
    OperationHookCallback pre_execution_hook = nullptr;

    /**
     * @brief Optional callback invoked after operation execution.
     */
    OperationHookCallback post_execution_hook = nullptr;

    /**
     * @brief Optional callback used for custom reconstruction of results.
     */
    ReconstructionCallback reconstruction_callback = nullptr;

    //=====================================================================
    // Dependency helpers
    //=====================================================================

    /**
     * @brief Register dependency on a specific operation type.
     *
     * @tparam T Operation type
     * @return Reference to this context for fluent chaining
     */
    template <typename T>
    ExecutionContext& depends_on()
    {
        dependencies.emplace_back(typeid(T));
        return *this;
    }

    //=====================================================================
    // Hook helpers
    //=====================================================================

    /**
     * @brief Set pre-execution hook.
     *
     * @param cb Callback invoked before operation execution
     * @return Reference to this context for fluent chaining
     */
    ExecutionContext& on_pre(OperationHookCallback cb)
    {
        pre_execution_hook = std::move(cb);
        return *this;
    }

    /**
     * @brief Set post-execution hook.
     *
     * @param cb Callback invoked after operation execution
     * @return Reference to this context for fluent chaining
     */
    ExecutionContext& on_post(OperationHookCallback cb)
    {
        post_execution_hook = std::move(cb);
        return *this;
    }

    /**
     * @brief Set reconstruction callback.
     *
     * @param cb Reconstruction logic for output data
     * @return Reference to this context for fluent chaining
     */
    ExecutionContext& on_reconstruct(ReconstructionCallback cb)
    {
        reconstruction_callback = std::move(cb);
        return *this;
    }

    /**
     * @brief Set execution timeout.
     *
     * @param duration Maximum allowed runtime
     * @return Reference to this context for fluent chaining
     */
    ExecutionContext& with_timeout(std::chrono::milliseconds duration)
    {
        timeout = duration;
        return *this;
    }

    /**
     * @brief Set execution mode.
     *
     * @param m Desired execution mode
     * @return Reference to this context for fluent chaining
     */
    ExecutionContext& set_mode(ExecutionMode m)
    {
        mode = m;
        return *this;
    }
};

}
#include "ExecutionContext.describe.hpp"
