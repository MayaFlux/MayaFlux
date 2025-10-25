#pragma once

#include "BufferOperation.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

namespace MayaFlux::Kriya {

class CycleCoordinator;

/**
 * @class BufferPipeline
 * @brief Execution engine for composable buffer processing operations.
 *
 * BufferPipeline orchestrates the execution of BufferOperation sequences with
 * sophisticated control flow, data lifecycle management, and scheduling integration.
 * It supports linear operation chains, conditional branching, parallel execution,
 * and cycle-based coordination through fluent operator>> chaining.
 *
 * **Example Usage:**
 * ```cpp
 * auto pipeline = BufferPipeline::create(*scheduler)
 *     >> BufferOperation::capture_from(mic_buffer)
 *         .with_window(1024, 0.75f)
 *         .on_data_ready([](const auto& data, uint32_t cycle) {
 *             auto spectrum = fft_transform(data);
 *             detect_pitch(spectrum);
 *         })
 *     >> BufferOperation::route_to_container(recording_stream)
 *         .with_priority(64);
 *
 * pipeline->execute_continuous();
 * ```
 *
 * **Conditional Branching:**
 * ```cpp
 * pipeline->branch_if([](uint32_t cycle) { return cycle % 10 == 0; },
 *     [](BufferPipeline& branch) {
 *         branch >> BufferOperation::dispatch_to([](const auto& data, uint32_t cycle) {
 *             save_snapshot(data, cycle);
 *         });
 *     });
 * ```
 *
 * @see BufferOperation For composable operation units
 * @see CycleCoordinator For multi-pipeline synchronization
 * @see Vruta::TaskScheduler For execution scheduling
 */
class MAYAFLUX_API BufferPipeline : public std::enable_shared_from_this<BufferPipeline> {
public:
    static std::shared_ptr<BufferPipeline> create(Vruta::TaskScheduler& scheduler, std::shared_ptr<Buffers::BufferManager> buffer_manager = nullptr)
    {
        return std::make_shared<BufferPipeline>(scheduler, std::move(buffer_manager));
    }

    BufferPipeline() = default;
    explicit BufferPipeline(Vruta::TaskScheduler& scheduler, std::shared_ptr<Buffers::BufferManager> buffer_manager = nullptr);

    ~BufferPipeline();

    /**
     * @brief Chain an operation to the pipeline.
     * @param operation BufferOperation to add to the processing chain
     * @return Reference to this pipeline for continued chaining
     */
    BufferPipeline& operator>>(BufferOperation&& operation)
    {
        m_operations.emplace_back(std::move(operation));
        return *this;
    }

    /**
     * @brief Add conditional branch to the pipeline.
     * @param condition Function that determines if branch should execute
     * @param branch_builder Function that configures the branch pipeline
     * @param synchronous If true, branch executes within main cycle (default: false)
     * @param samples_per_operation Number of samples per operation in branch (default: 1)
     * @return Reference to this pipeline for continued chaining
     */
    BufferPipeline& branch_if(std::function<bool(uint32_t)> condition,
        const std::function<void(BufferPipeline&)>& branch_builder,
        bool synchronous = false, uint64_t samples_per_operation = 1);

    /**
     * @brief Execute operations in parallel within the current cycle.
     * @param operations List of operations to execute concurrently
     * @return Reference to this pipeline for continued chaining
     */
    BufferPipeline& parallel(std::initializer_list<BufferOperation> operations);

    /**
     * @brief Set lifecycle callbacks for cycle management.
     * @param on_cycle_start Function called at the beginning of each cycle
     * @param on_cycle_end Function called at the end of each cycle
     * @return Reference to this pipeline for continued chaining
     */
    BufferPipeline& with_lifecycle(
        std::function<void(uint32_t)> on_cycle_start,
        std::function<void(uint32_t)> on_cycle_end);

    /**
     * @brief Set the execution strategy for this pipeline
     * @param strategy Execution coordination strategy
     * @return Reference to this pipeline for chaining
     */
    BufferPipeline& with_strategy(ExecutionStrategy strategy)
    {
        m_execution_strategy = strategy;
        return *this;
    }

    /**
     * @brief Set timing mode for capture phase
     * @param mode Timing synchronization mode
     * @return Reference to this pipeline for chaining
     */
    BufferPipeline& capture_timing(Vruta::DelayContext mode)
    {
        m_capture_timing = mode;
        return *this;
    }

    /**
     * @brief Set timing mode for process phase
     * @param mode Timing synchronization mode
     * @return Reference to this pipeline for chaining
     */
    BufferPipeline& process_timing(Vruta::DelayContext mode)
    {
        m_process_timing = mode;
        return *this;
    }

    /**
     * @brief Get current execution strategy
     */
    inline ExecutionStrategy get_strategy() const { return m_execution_strategy; }

    // Update execute_buffer_rate to use new infrastructure:
    /**
     * @brief Execute pipeline synchronized to audio hardware cycle boundaries
     *
     * This now respects the configured execution strategy and timing modes.
     * Default strategy is PHASED with BUFFER_RATE timing.
     *
     * @param max_cycles Maximum number of audio cycles to process (0 = infinite)
     */
    void execute_buffer_rate(uint32_t max_cycles = 0);

    /**
     * @brief Execute the pipeline for a single cycle.
     *
     * Schedules the pipeline to run once through all operations. The execution
     * happens asynchronously via the scheduler's coroutine system. The pipeline
     * must have a scheduler and will be kept alive via shared_ptr until execution
     * completes.
     *
     * @throws std::runtime_error if pipeline has no scheduler
     *
     * @note This is asynchronous - the function returns immediately and execution
     *       happens when the scheduler processes the coroutine.
     */
    void execute_once();

    /**
     * @brief Execute the pipeline for a specified number of cycles.
     * @param cycles Number of processing cycles to execute
     *
     * Schedules the pipeline to run for the given number of cycles. Each cycle
     * processes all operations once. Execution is asynchronous via the scheduler.
     * The pipeline remains alive until all cycles complete.
     *
     * @throws std::runtime_error if pipeline has no scheduler
     *
     * @note Execution happens asynchronously - this function returns immediately.
     */
    void execute_for_cycles(uint32_t cycles);

    /**
     * @brief Start continuous execution of the pipeline.
     *
     * Schedules the pipeline to run indefinitely until stop_continuous() is called.
     * The pipeline processes all operations in each cycle and automatically
     * advances to the next cycle. The pipeline keeps itself alive via shared_ptr
     * during continuous execution.
     *
     * @throws std::runtime_error if pipeline has no scheduler
     *
     * @note Execution is asynchronous. Call stop_continuous() to halt execution.
     * @see stop_continuous()
     */
    void execute_continuous();

    /**
     * @brief Stop continuous execution of the pipeline.
     *
     * Signals the pipeline to stop after completing the current cycle. The
     * pipeline will gracefully terminate its execution loop. Has no effect
     * if the pipeline is not currently executing continuously.
     *
     * @note This only sets a flag - actual termination happens at the end of
     *       the current cycle.
     */
    inline void stop_continuous() { m_continuous_execution = false; }

    /**
     * @brief Execute pipeline with sample-accurate timing between operations.
     * @param max_cycles Maximum number of cycles to execute (0 = infinite)
     * @param samples_per_operation Number of samples to wait between operations (default: 1)
     *
     * Schedules pipeline execution with precise timing control. After each operation
     * within a cycle, the pipeline waits for the specified number of samples before
     * proceeding. Useful for rate-limiting operations or creating timed sequences.
     *
     * @throws std::runtime_error if pipeline has no scheduler
     *
     * @note Execution is asynchronous. The pipeline keeps itself alive until completion.
     */
    void execute_scheduled(uint32_t max_cycles, uint64_t samples_per_operation = 1);

    /**
     * @brief Execute pipeline with real-time rate control.
     * @param max_cycles Maximum number of cycles to execute
     * @param seconds_per_operation Duration between operations in seconds
     *
     * Convenience wrapper around execute_scheduled() that converts time intervals
     * to sample counts based on the scheduler's sample rate. Enables natural
     * specification of timing in seconds rather than samples.
     *
     * @throws std::runtime_error if pipeline has no scheduler
     *
     * Example:
     * ```cpp
     * // Execute 10 cycles with 0.5 seconds between operations
     * pipeline->execute_scheduled_at_rate(10, 0.5);
     * ```
     */
    void execute_scheduled_at_rate(uint32_t max_cycles, double seconds_per_operation);

    /**
     * @brief Execute pipeline synchronized to audio hardware cycle boundaries
     *
     * Creates a coroutine and registers it with the scheduler using the processing hook
     * for timing synchronization. The pipeline will be added to the scheduler at the
     * start of each audio cycle, ensuring buffer operations complete before the buffer
     * manager reads data.
     *
     * @param max_cycles Maximum number of audio cycles to process (0 = infinite)
     */
    // void execute_buffer_rate(uint32_t max_cycles = 0);

    // State management

    /**
     * @brief Mark operation data as consumed for cleanup.
     * @param operation_index Index of the operation in the pipeline
     *
     * Manually marks an operation's data state as consumed, allowing it to be
     * cleaned up in the next cleanup cycle. Useful for manual data lifecycle
     * management in custom processing scenarios.
     *
     * @note Out-of-range indices are silently ignored.
     */
    void mark_data_consumed(uint32_t operation_index);

    /**
     * @brief Check if any operations have pending data ready for processing.
     * @return true if any operation has data in READY state, false otherwise
     *
     * Queries the data states of all operations to determine if there is
     * unprocessed data available. Useful for synchronization and debugging.
     */
    bool has_pending_data() const;

    /**
     * @brief Get the current cycle number.
     * @return Current cycle count since pipeline creation or last reset
     *
     * Returns the internal cycle counter that increments with each complete
     * cycle execution. Useful for cycle-dependent logic and debugging.
     */
    uint32_t get_current_cycle() const { return m_current_cycle; }

private:
    enum class DataState : uint8_t {
        EMPTY, ///< No data available
        READY, ///< Data ready for processing
        CONSUMED, ///< Data has been processed
        EXPIRED ///< Data has expired and should be cleaned up
    };

    struct BranchInfo {
        std::function<bool(uint32_t)> condition;
        std::shared_ptr<BufferPipeline> pipeline;
        bool synchronous;
        uint64_t samples_per_operation;
    };

    std::shared_ptr<BufferPipeline> m_active_self;
    std::shared_ptr<CycleCoordinator> m_coordinator;
    std::shared_ptr<Buffers::BufferManager> m_buffer_manager;
    Vruta::TaskScheduler* m_scheduler = nullptr;

    std::vector<BufferOperation> m_operations;
    std::vector<DataState> m_data_states;
    std::unordered_map<BufferOperation*, Kakshya::DataVariant> m_operation_data;
    std::vector<BranchInfo> m_branches;
    std::vector<std::shared_ptr<Vruta::SoundRoutine>> m_branch_tasks;
    std::function<void(uint32_t)> m_cycle_start_callback;
    std::function<void(uint32_t)> m_cycle_end_callback;

    uint64_t m_current_cycle { 0 };
    uint64_t m_max_cycles { 0 };
    bool m_continuous_execution { false };
    ExecutionStrategy m_execution_strategy { ExecutionStrategy::PHASED };
    Vruta::DelayContext m_capture_timing { Vruta::DelayContext::BUFFER_BASED };
    Vruta::DelayContext m_process_timing { Vruta::DelayContext::SAMPLE_BASED };

    static Kakshya::DataVariant extract_buffer_data(const std::shared_ptr<Buffers::AudioBuffer>& buffer, bool should_process = false);
    static void write_to_buffer(const std::shared_ptr<Buffers::AudioBuffer>& buffer, const Kakshya::DataVariant& data);
    static void write_to_container(const std::shared_ptr<Kakshya::DynamicSoundStream>& container, const Kakshya::DataVariant& data);
    static Kakshya::DataVariant read_from_container(const std::shared_ptr<Kakshya::DynamicSoundStream>& container, uint64_t start, uint32_t length);

    void process_operation(BufferOperation& op, uint64_t cycle);
    std::shared_ptr<Vruta::SoundRoutine> dispatch_branch_async(BranchInfo& branch, uint64_t cycle);
    void await_timing(Vruta::DelayContext mode, uint64_t units);

    void cleanup_expired_data();
    void cleanup_completed_branches();

    Vruta::SoundRoutine execute_internal(uint64_t max_cycles, uint64_t samples_per_operation);
    Vruta::SoundRoutine execute_phased(uint64_t max_cycles, uint64_t samples_per_operation);
    Vruta::SoundRoutine execute_streaming(uint64_t max_cycles, uint64_t samples_per_operation);
    Vruta::SoundRoutine execute_parallel(uint64_t max_cycles, uint64_t samples_per_operation);
    Vruta::SoundRoutine execute_reactive(uint64_t max_cycles, uint64_t samples_per_operation);

    void execute_capture_phase(uint64_t cycle_base);
    void execute_process_phase(uint64_t cycle);
};

inline std::shared_ptr<BufferPipeline> operator>>(
    std::shared_ptr<BufferPipeline> pipeline,
    BufferOperation&& operation)
{
    *pipeline >> std::move(operation);
    return pipeline;
}

inline std::shared_ptr<BufferPipeline> operator>>(
    std::shared_ptr<BufferPipeline> pipeline,
    BufferOperation& operation) // <-- Accepts lvalue
{
    *pipeline >> std::move(operation);
    return pipeline;
}

}
