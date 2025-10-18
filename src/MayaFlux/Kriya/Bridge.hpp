#pragma once

#include "Capture.hpp"
#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Core/ProcessingTokens.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

namespace MayaFlux::Kriya {

class CycleCoordinator;

/**
 * @class BufferOperation
 * @brief Fundamental unit of operation in buffer processing pipelines.
 *
 * BufferOperation encapsulates discrete processing steps that can be composed
 * into complex data flow pipelines. Each operation represents a specific action
 * such as capturing data, transforming it, routing to destinations, or applying
 * conditional logic. Operations are designed to be chainable and support
 * sophisticated scheduling and priority management.
 *
 * **Operation Types:**
 * - **CAPTURE**: Extract data from AudioBuffer using configurable capture strategies
 * - **TRANSFORM**: Apply functional transformations to data variants
 * - **ROUTE**: Direct data to AudioBuffer or DynamicSoundStream destinations
 * - **LOAD**: Read data from containers into buffers with position control
 * - **SYNC**: Coordinate timing and synchronization across pipeline stages
 * - **CONDITION**: Apply conditional logic and branching to data flow
 * - **DISPATCH**: Send data to external handlers and callback systems
 * - **FUSE**: Combine multiple data sources using custom fusion functions
 *
 * **Example Usage:**
 * ```cpp
 * // Capture audio with windowed analysis
 * auto capture_op = BufferOperation::capture_from(input_buffer)
 *     .with_window(512, 0.5f)
 *     .on_data_ready([](const auto& data, uint32_t cycle) {
 *         analyze_spectrum(data);
 *     });
 *
 * // Transform and route to output
 * auto pipeline = BufferPipeline()
 *     >> capture_op
 *     >> BufferOperation::transform([](const auto& data, uint32_t cycle) {
 *         return apply_reverb(data);
 *     })
 *     >> BufferOperation::route_to_container(output_stream);
 * ```
 * @class BufferCapture
 * ...existing intro...
 *
 * **Cycle Behavior:**
 * The `for_cycles(N)` configuration controls how many times the capture operation
 * executes within a single pipeline cycle. When a capture has `.for_cycles(20)`,
 * the operation will capture 20 times sequentially, with each capture receiving
 * incrementing cycle numbers (0, 1, 2... 19) and calling `on_data_ready()` for
 * each iteration.
 *
 * This is distinct from pipeline-level cycle control:
 * - `.for_cycles(20)` on capture → operation executes 20 times per pipeline cycle
 * - `execute_scheduled(5, ...)` → pipeline runs 5 times total
 * - Combined: 5 × 20 = 100 total capture executions
 *
 * **Example:**
 * ```cpp
 * auto pipeline = BufferPipeline::create(*scheduler);
 * pipeline >> BufferOperation::capture_from(buffer)
 *     .for_cycles(10)  // Capture 10 times per pipeline invocation
 *     .on_data_ready([](const auto& data, uint32_t cycle) {
 *         std::cout << "Capture #" << cycle << '\n';  // Prints 0-9
 *     });
 * pipeline->execute_scheduled(3, 512);  // Runs pipeline 3 times → 30 total captures
 * ```
 *
 *
 * @see BufferPipeline For pipeline construction and execution
 * @see BufferCapture For flexible data capture strategies
 * @see CycleCoordinator For cross-pipeline synchronization
 */
class MAYAFLUX_API BufferOperation {
public:
    /**
     * @enum OpType
     * @brief Defines the fundamental operation types in the processing pipeline.
     */
    enum class OpType : uint8_t {
        CAPTURE, ///< Capture data from source buffer using BufferCapture strategy
        TRANSFORM, ///< Apply transformation function to data variants
        ROUTE, ///< Route data to destination (buffer or container)
        LOAD, ///< Load data from container to buffer with position control
        SYNC, ///< Synchronize with timing/cycles for coordination
        CONDITION, ///< Conditional operation for branching logic
        BRANCH, ///< Branch to sub-pipeline based on conditions
        DISPATCH, ///< Dispatch to external handler for custom processing
        FUSE ///< Fuse multiple sources using custom fusion functions
    };

    /**
     * @brief Create a capture operation using BufferCapture configuration.
     * @param capture Configured BufferCapture with desired capture strategy
     * @return BufferOperation configured for data capture
     */
    static BufferOperation capture(BufferCapture capture)
    {
        return { OpType::CAPTURE, std::move(capture) };
    }

    /**
     * @brief Create capture operation from input channel using convenience API.
     * Creates input buffer automatically and returns configured capture operation.
     * @param buffer_manager System buffer manager
     * @param input_channel Input channel to capture from
     * @param mode Capture mode (default: ACCUMULATE)
     * @param cycle_count Number of cycles (0 = continuous, default)
     * @return BufferOperation configured for input capture with default settings
     */
    static BufferOperation capture_input(
        const std::shared_ptr<Buffers::BufferManager>& buffer_manager,
        uint32_t input_channel,
        BufferCapture::CaptureMode mode = BufferCapture::CaptureMode::ACCUMULATE,
        uint32_t cycle_count = 0);

    /**
     * @brief Create CaptureBuilder for input channel with fluent configuration.
     * Uses the existing CaptureBuilder pattern but with input buffer creation.
     * @param buffer_manager System buffer manager
     * @param input_channel Input channel to capture from
     * @return CaptureBuilder for fluent configuration
     */
    static CaptureBuilder capture_input_from(
        const std::shared_ptr<Buffers::BufferManager>& buffer_manager,
        uint32_t input_channel);

    /**
     * @brief Create a file capture operation that reads from file and stores in stream.
     * @param filepath Path to audio file
     * @param channel Channel index (default: 0)
     * @param cycle_count Number of cycles to capture (0 = continuous)
     * @return BufferOperation configured for file capture
     */
    static BufferOperation capture_file(
        const std::string& filepath,
        uint32_t channel = 0,
        uint32_t cycle_count = 0);

    /**
     * @brief Create CaptureBuilder for file with fluent configuration.
     * @param filepath Path to audio file
     * @param channel Channel index (default: 0)
     * @return CaptureBuilder for fluent configuration
     */
    static CaptureBuilder capture_file_from(
        const std::string& filepath,
        uint32_t channel = 0);

    /**
     * @brief Create operation to route file data to DynamicSoundStream.
     * @param filepath Path to audio file
     * @param target_stream Target DynamicSoundStream
     * @param cycle_count Number of cycles to read (0 = entire file)
     * @return BufferOperation configured for file to stream routing
     */
    static BufferOperation file_to_stream(
        const std::string& filepath,
        std::shared_ptr<Kakshya::DynamicSoundStream> target_stream,
        uint32_t cycle_count = 0);

    /**
     * @brief Create a transform operation with custom transformation function.
     * @param transformer Function that transforms DataVariant with cycle information
     * @return BufferOperation configured for data transformation
     */
    static BufferOperation transform(std::function<Kakshya::DataVariant(const Kakshya::DataVariant&, uint32_t)> transformer);

    /**
     * @brief Create a routing operation to AudioBuffer destination.
     * @param target Target AudioBuffer to receive data
     * @return BufferOperation configured for buffer routing
     */
    static BufferOperation route_to_buffer(std::shared_ptr<Buffers::AudioBuffer> target);

    /**
     * @brief Create a routing operation to DynamicSoundStream destination.
     * @param target Target container to receive data
     * @return BufferOperation configured for container routing
     */
    static BufferOperation route_to_container(std::shared_ptr<Kakshya::DynamicSoundStream> target);

    /**
     * @brief Create a load operation from container to buffer.
     * @param source Source container to read from
     * @param target Target buffer to write to
     * @param start_frame Starting frame position (default: 0)
     * @param length Number of frames to load (default: 0 = all)
     * @return BufferOperation configured for container loading
     */
    static BufferOperation load_from_container(std::shared_ptr<Kakshya::DynamicSoundStream> source,
        std::shared_ptr<Buffers::AudioBuffer> target,
        uint64_t start_frame = 0,
        uint32_t length = 0);

    /**
     * @brief Create a conditional operation for pipeline branching.
     * @param condition Function that returns true when condition is met
     * @return BufferOperation configured for conditional execution
     */
    static BufferOperation when(std::function<bool(uint32_t)> condition);

    /**
     * @brief Create a dispatch operation for external processing.
     * @param handler Function to handle data with cycle information
     * @return BufferOperation configured for external dispatch
     */
    static BufferOperation dispatch_to(std::function<void(const Kakshya::DataVariant&, uint32_t)> handler);

    /**
     * @brief Create a fusion operation for multiple AudioBuffer sources.
     * @param sources Vector of source buffers to fuse
     * @param fusion_func Function that combines multiple DataVariants
     * @param target Target buffer for fused result
     * @return BufferOperation configured for buffer fusion
     */
    static BufferOperation fuse_data(std::vector<std::shared_ptr<Buffers::AudioBuffer>> sources,
        std::function<Kakshya::DataVariant(const std::vector<Kakshya::DataVariant>&, uint32_t)> fusion_func,
        std::shared_ptr<Buffers::AudioBuffer> target);

    /**
     * @brief Create a fusion operation for multiple DynamicSoundStream sources.
     * @param sources Vector of source containers to fuse
     * @param fusion_func Function that combines multiple DataVariants
     * @param target Target container for fused result
     * @return BufferOperation configured for container fusion
     */
    static BufferOperation fuse_containers(std::vector<std::shared_ptr<Kakshya::DynamicSoundStream>> sources,
        std::function<Kakshya::DataVariant(const std::vector<Kakshya::DataVariant>&, uint32_t)> fusion_func,
        std::shared_ptr<Kakshya::DynamicSoundStream> target);

    /**
     * @brief Create a CaptureBuilder for fluent capture configuration.
     * @param buffer AudioBuffer to capture from (must be registered with BufferManager if using AUTOMATIC processing)
     * @return CaptureBuilder for fluent operation construction
     *
     * @note If the buffer uses ProcessingControl::AUTOMATIC, ensure it's registered with
     *       the BufferManager via add_audio_buffer() before pipeline execution.
     */
    static CaptureBuilder capture_from(std::shared_ptr<Buffers::AudioBuffer> buffer);

    /**
     * @brief Set execution priority for scheduler ordering.
     * @param priority Priority value (0=highest, 255=lowest, default=128)
     * @return Reference to this operation for chaining
     */
    BufferOperation& with_priority(uint8_t priority);

    /**
     * @brief Set processing token for execution context.
     * @param token Processing token indicating execution context
     * @return Reference to this operation for chaining
     */
    BufferOperation& on_token(Buffers::ProcessingToken token);

    /**
     * @brief Set cycle interval for periodic execution.
     * @param n Execute every n cycles (default: 1)
     * @return Reference to this operation for chaining
     */
    BufferOperation& every_n_cycles(uint32_t n);

    /**
     * @brief Assign identification tag.
     * @param tag String identifier for debugging and organization
     * @return Reference to this operation for chaining
     */
    BufferOperation& with_tag(const std::string& tag);

    /**
     * @brief Getters for internal state (read-only)
     */
    inline OpType get_type() const { return m_type; }

    /**
     * @brief Getters for internal state (read-only)
     */
    inline uint8_t get_priority() const { return m_priority; }

    /**
     * @brief Getters for processing token
     */
    inline Buffers::ProcessingToken get_token() const { return m_token; }

    /**
     * @brief Getters for user defined tag
     */
    inline const std::string& get_tag() const { return m_tag; }

    BufferOperation(OpType type, BufferCapture capture)
        : m_type(type)
        , m_capture(std::move(capture))
        , m_tag(m_capture.get_tag())
    {
    }

    explicit BufferOperation(OpType type)
        : m_type(type)
        , m_capture(nullptr)
    {
    }

private:
    OpType m_type;
    BufferCapture m_capture;

    std::function<Kakshya::DataVariant(const Kakshya::DataVariant&, uint32_t)> m_transformer;

    std::shared_ptr<Buffers::AudioBuffer> m_target_buffer;
    std::shared_ptr<Kakshya::DynamicSoundStream> m_target_container;

    std::shared_ptr<Kakshya::DynamicSoundStream> m_source_container;
    uint64_t m_start_frame = 0;
    uint32_t m_load_length = 0;

    std::function<bool(uint32_t)> m_condition;
    std::function<void(const Kakshya::DataVariant&, uint32_t)> m_dispatch_handler;

    std::vector<std::shared_ptr<Buffers::AudioBuffer>> m_source_buffers;
    std::vector<std::shared_ptr<Kakshya::DynamicSoundStream>> m_source_containers;
    std::function<Kakshya::DataVariant(const std::vector<Kakshya::DataVariant>&, uint32_t)> m_fusion_function;

    uint8_t m_priority = 128;
    Buffers::ProcessingToken m_token = Buffers::ProcessingToken::AUDIO_BACKEND;
    uint32_t m_cycle_interval = 1;
    std::string m_tag;

    friend class BufferPipeline;
};

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
    static std::shared_ptr<BufferPipeline> create(Vruta::TaskScheduler& scheduler)
    {
        return std::make_shared<BufferPipeline>(scheduler);
    }

    BufferPipeline() = default;
    explicit BufferPipeline(Vruta::TaskScheduler& scheduler);

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

    std::vector<BufferOperation> m_operations;
    std::vector<DataState> m_data_states;

    Vruta::TaskScheduler* m_scheduler = nullptr;
    std::shared_ptr<CycleCoordinator> m_coordinator;

    uint32_t m_current_cycle = 0;
    bool m_continuous_execution = false;

    std::function<void(uint32_t)> m_cycle_start_callback;
    std::function<void(uint32_t)> m_cycle_end_callback;

    Vruta::SoundRoutine execute_internal(uint32_t max_cycles, uint64_t samples_per_operation);
    void process_operation(BufferOperation& op, uint32_t cycle);
    void cleanup_expired_data();
    Kakshya::DataVariant extract_buffer_data(std::shared_ptr<Buffers::AudioBuffer> buffer, bool should_process = false);
    void write_to_buffer(std::shared_ptr<Buffers::AudioBuffer> buffer, const Kakshya::DataVariant& data);
    void write_to_container(std::shared_ptr<Kakshya::DynamicSoundStream> container, const Kakshya::DataVariant& data);
    Kakshya::DataVariant read_from_container(std::shared_ptr<Kakshya::DynamicSoundStream> container, uint64_t start, uint32_t length);

    std::unordered_map<BufferOperation*, Kakshya::DataVariant> m_operation_data;

    std::vector<BranchInfo> m_branches;
    std::vector<std::shared_ptr<Vruta::SoundRoutine>> m_branch_tasks;

    std::shared_ptr<Vruta::SoundRoutine> dispatch_branch_async(BranchInfo& branch, uint32_t cycle);
    void cleanup_completed_branches();
};

inline std::shared_ptr<BufferPipeline> operator>>(
    std::shared_ptr<BufferPipeline> pipeline,
    BufferOperation&& operation)
{
    *pipeline >> std::move(operation);
    return pipeline;
}

}
