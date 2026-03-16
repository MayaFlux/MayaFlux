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
using OperationHookCallback = std::function<void(std::any&)>;

/**
 * @brief Callback type for custom reconstruction logic
 */
using ReconstructionCallback = std::function<std::any(std::vector<std::vector<double>>&, std::any&)>;

/**
 * @struct ExecutionContext
 * @brief Context information controlling how a compute operation executes.
 *
 * ExecutionContext provides execution metadata, dependency hints, and hooks
 * that influence how a Yantra operation is scheduled and run.
 *
 * The `execution_metadata` map allows arbitrary user-defined parameters
 * to be passed into operations. All reads should be performed using the
 * provided accessors (`get`, `get_or`, `get_or_throw`) which internally
 * use `safe_any_cast` to provide robust type conversion and diagnostics.
 *
 * Typical usage:
 *
 * @code
 * ExecutionContext ctx;
 *
 * ctx.set("grain_size", 1024)
 *    .set("hop_size", 512)
 *    .depends_on<MyAnalyzer>();
 *
 * auto grain = ctx.get_or<uint32_t>("grain_size", 512);
 * @endcode
 */
struct MAYAFLUX_API ExecutionContext {

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
     * @brief Arbitrary metadata parameters used by operations.
     *
     * This key/value store carries runtime configuration such as
     * algorithm parameters, flags, thresholds, or domain-specific values.
     *
     * Values are stored as `std::any` and should be retrieved via
     * `get()` or `get_or()` to ensure safe casting.
     */
    std::unordered_map<std::string, std::any> execution_metadata;

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
    // Metadata helpers
    //=====================================================================

    /**
     * @brief Insert or update metadata value.
     *
     * Adds or replaces a value in the metadata store.
     *
     * @tparam T Value type
     * @param key Metadata key
     * @param value Value to store
     * @return Reference to this context for fluent chaining
     */
    template <typename T>
    ExecutionContext& set(std::string key, T&& value)
    {
        execution_metadata[std::move(key)] = std::forward<T>(value);
        return *this;
    }

    /**
     * @brief Retrieve metadata value using safe casting.
     *
     * Uses `safe_any_cast` internally, allowing numeric conversions
     * and providing detailed error reporting.
     *
     * @tparam T Expected type
     * @param key Metadata key
     * @return CastResult containing the value or error details
     */
    template <typename T>
    CastResult<T> get(const std::string& key) const
    {
        auto it = execution_metadata.find(key);

        if (it == execution_metadata.end()) {
            CastResult<T> result;
            result.error = "ExecutionContext missing key: " + key;
            return result;
        }

        return safe_any_cast<T>(it->second);
    }

    /**
     * @brief Retrieve metadata value or return a default.
     *
     * @tparam T Expected type
     * @param key Metadata key
     * @param default_value Value returned if key missing or conversion fails
     * @return Retrieved or default value
     */
    template <typename T>
    T get_or(const std::string& key, const T& default_value) const
    {
        auto it = execution_metadata.find(key);

        if (it == execution_metadata.end())
            return default_value;

        return safe_any_cast<T>(it->second).value_or(default_value);
    }

    /**
     * @brief Retrieve metadata value or throw if unavailable.
     *
     * Uses `safe_any_cast_or_throw`.
     *
     * @tparam T Expected type
     * @param key Metadata key
     * @return Retrieved value
     *
     * @throws std::runtime_error if key missing or conversion fails
     */
    template <typename T>
    T get_or_throw(const std::string& key) const
    {
        auto it = execution_metadata.find(key);

        if (it == execution_metadata.end())
            throw std::runtime_error("ExecutionContext missing key: " + key);

        return safe_any_cast_or_throw<T>(it->second);
    }

    /**
     * @brief Check whether a metadata key exists.
     *
     * @param key Metadata key
     * @return True if key is present
     */
    bool contains(const std::string& key) const
    {
        return execution_metadata.contains(key);
    }

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
