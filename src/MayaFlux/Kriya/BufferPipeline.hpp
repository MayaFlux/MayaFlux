#pragma once

#include "BufferOperation.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

namespace MayaFlux::Kriya {

class CycleCoordinator;

/**
 * @class BufferPipeline
 * @brief Coroutine-based execution engine for composable, multi-strategy buffer processing.
 *
 * BufferPipeline provides a flexible framework for orchestrating complex data flow patterns
 * through declarative operation chains. It supports multiple execution strategies (phased,
 * streaming, parallel, reactive), sophisticated data accumulation modes, and sample-accurate
 * timing coordination via the coroutine scheduler.
 *
 * **Core Concepts:**
 * - **Operations**: Composable units (capture, transform, route, modify, fuse) chained via >>
 * - **Execution Strategies**: PHASED (capture-then-process), STREAMING (immediate flow-through),
 *   PARALLEL (concurrent captures), REACTIVE (data-driven)
 * - **Capture Modes**: TRANSIENT (single), ACCUMULATE (concatenate), CIRCULAR (rolling buffer),
 *   WINDOWED (overlapping windows), TRIGGERED (conditional)
 * - **Timing Control**: Buffer-rate synchronization, sample-accurate delays, or immediate execution
 *
 * **Simple Capture & Route:**
 * ```cpp
 * auto pipeline = BufferPipeline::create(*scheduler, buffer_manager);
 * *pipeline
 *     >> BufferOperation::capture_from(input_buffer)
 *         .for_cycles(1)
 *     >> BufferOperation::route_to_buffer(output_buffer);
 *
 * pipeline->execute_buffer_rate(100);  // Run for 100 audio buffer cycles
 * ```
 *
 * **Accumulation & Batch Processing (PHASED strategy):**
 * ```cpp
 * auto pipeline = BufferPipeline::create(*scheduler, buffer_manager)
 *     ->with_strategy(ExecutionStrategy::PHASED)
 *     ->capture_timing(Vruta::DelayContext::BUFFER_BASED);
 *
 * *pipeline
 *     >> BufferOperation::capture_from(audio_buffer)
 *         .for_cycles(20)  // Captures 20 times, concatenates into single buffer
 *     >> BufferOperation::transform([](const auto& data, uint32_t cycle) {
 *         const auto& accumulated = std::get<std::vector<double>>(data);
 *         // Process 20 * buffer_size samples as one batch
 *         return apply_batch_fft(accumulated);
 *     })
 *     >> BufferOperation::route_to_container(output_stream);
 *
 * pipeline->execute_buffer_rate();
 * ```
 *
 * **Real-Time Streaming Modification (STREAMING strategy):**
 * ```cpp
 * auto pipeline = BufferPipeline::create(*scheduler, buffer_manager)
 *     ->with_strategy(ExecutionStrategy::STREAMING);
 *
 * *pipeline
 *     >> BufferOperation::capture_from(audio_buffer).for_cycles(1)
 *     >> BufferOperation::modify_buffer(audio_buffer, [noise](auto buf) {
 *         auto& data = buf->get_data();
 *         for (auto& sample : data) {
 *             sample *= noise->process_sample();  // Apply effect in-place
 *         }
 *     }).as_streaming();  // Processor stays attached across cycles
 *
 * pipeline->execute_buffer_rate();  // Runs continuously
 * ```
 *
 * **Circular Buffer for Rolling Analysis:**
 * ```cpp
 * *pipeline
 *     >> BufferOperation::capture_from(input_buffer)
 *         .for_cycles(100)
 *         .as_circular(2048)  // Maintains last 2048 samples
 *     >> BufferOperation::transform([](const auto& data, uint32_t cycle) {
 *         const auto& history = std::get<std::vector<double>>(data);
 *         return analyze_recent_trends(history);  // Always sees last 2048 samples
 *     });
 * ```
 *
 * **Windowed Capture with Overlap:**
 * ```cpp
 * *pipeline
 *     >> BufferOperation::capture_from(input_buffer)
 *         .for_cycles(50)
 *         .with_window(1024, 0.5f)  // 1024 samples, 50% overlap
 *     >> BufferOperation::transform([](const auto& data, uint32_t cycle) {
 *         const auto& window = std::get<std::vector<double>>(data);
 *         return apply_hann_window_and_fft(window);
 *     });
 * ```
 *
 * **Multi-Source Fusion:**
 * ```cpp
 * *pipeline
 *     >> BufferOperation::fuse_data(
 *         {mic_buffer, synth_buffer, file_buffer},
 *         [](std::vector<Kakshya::DataVariant>& sources, uint32_t cycle) {
 *             // Combine three audio sources with custom mixing
 *             return mix_sources(sources, {0.5, 0.3, 0.2});
 *         },
 *         output_buffer);
 * ```
 *
 * **Conditional Branching:**
 * ```cpp
 * pipeline->branch_if(
 *     [](uint32_t cycle) { return cycle % 16 == 0; },  // Every 16 cycles
 *     [](BufferPipeline& branch) {
 *         branch >> BufferOperation::dispatch_to([](const auto& data, uint32_t cycle) {
 *             save_analysis_snapshot(data, cycle);
 *         });
 *     },
 *     true  // Synchronous - wait for branch to complete
 * );
 * ```
 *
 * **Per-Iteration Routing (Immediate Routing):**
 * ```cpp
 * // ROUTE directly after CAPTURE → routes each iteration immediately
 * *pipeline
 *     >> BufferOperation::capture_from(input_buffer).for_cycles(20)
 *     >> BufferOperation::route_to_container(stream);  // Writes 20 times (streaming output)
 *
 * // ROUTE after TRANSFORM → routes accumulated result once
 * *pipeline
 *     >> BufferOperation::capture_from(input_buffer).for_cycles(20)
 *     >> BufferOperation::transform(process_fn)
 *     >> BufferOperation::route_to_container(stream);  // Writes 1 time (batch output)
 * ```
 *
 * **Lifecycle Callbacks:**
 * ```cpp
 * pipeline->with_lifecycle(
 *     [](uint32_t cycle) { std::cout << "Cycle " << cycle << " start\n"; },
 *     [](uint32_t cycle) { std::cout << "Cycle " << cycle << " end\n"; }
 * );
 * ```
 *
 * **Execution Modes:**
 * - `execute_buffer_rate(N)`: Synchronized to audio buffer boundaries for N cycles
 * - `execute_continuous()`: Runs indefinitely until stopped
 * - `execute_for_cycles(N)`: Runs exactly N cycles then stops
 * - `execute_once()`: Single cycle execution
 * - `execute_scheduled(N, samples)`: With sample-accurate delays between operations
 *
 * **Strategy Selection:**
 * - **PHASED** (default): Capture phase completes, then process phase runs
 *   - Best for: batch analysis, accumulation, FFT processing
 * - **STREAMING**: Operations flow through immediately, minimal latency
 *   - Best for: real-time effects, low-latency processing, modify_buffer
 * - **PARALLEL**: Multiple captures run concurrently (TODO)
 *   - Best for: multi-source synchronized capture
 * - **REACTIVE**: Data-driven execution when inputs available (TODO)
 *   - Best for: event-driven workflows, complex dependencies
 *
 * @see BufferOperation For operation types and configuration
 * @see BufferCapture For capture modes and data accumulation strategies
 * @see CycleCoordinator For multi-pipeline synchronization
 * @see Vruta::TaskScheduler For coroutine scheduling and timing
 * @see ExecutionStrategy For execution coordination patterns
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

    void capture_operation(BufferOperation& op, uint64_t cycle);
    void reset_accumulated_data();
    bool has_immediate_routing(const BufferOperation& op) const;

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
