#pragma once

#include "Capture.hpp"
#include "MayaFlux/Core/ProcessingTokens.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

namespace MayaFlux::Kriya {

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
 *
 * @see BufferPipeline For pipeline construction and execution
 * @see BufferCapture For flexible data capture strategies
 * @see CycleCoordinator For cross-pipeline synchronization
 */
class BufferOperation {
public:
    /**
     * @enum OpType
     * @brief Defines the fundamental operation types in the processing pipeline.
     */
    enum class OpType {
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
        return BufferOperation(OpType::CAPTURE, std::move(capture));
    }

    /**
     * @brief Create a transform operation with custom transformation function.
     * @param transformer Function that transforms DataVariant with cycle information
     * @return BufferOperation configured for data transformation
     */
    static BufferOperation transform(std::function<Kakshya::DataVariant(const Kakshya::DataVariant&, uint32_t)> transformer)
    {
        BufferOperation op(OpType::TRANSFORM);
        op.m_transformer = transformer;
        return op;
    }

    /**
     * @brief Create a routing operation to AudioBuffer destination.
     * @param target Target AudioBuffer to receive data
     * @return BufferOperation configured for buffer routing
     */
    static BufferOperation route_to_buffer(std::shared_ptr<Buffers::AudioBuffer> target)
    {
        BufferOperation op(OpType::ROUTE);
        op.m_target_buffer = target;
        return op;
    }

    /**
     * @brief Create a routing operation to DynamicSoundStream destination.
     * @param target Target container to receive data
     * @return BufferOperation configured for container routing
     */
    static BufferOperation route_to_container(std::shared_ptr<Kakshya::DynamicSoundStream> target)
    {
        BufferOperation op(OpType::ROUTE);
        op.m_target_container = target;
        return op;
    }

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
        uint32_t length = 0)
    {
        BufferOperation op(OpType::LOAD);
        op.m_source_container = source;
        op.m_target_buffer = target;
        op.m_start_frame = start_frame;
        op.m_load_length = length;
        return op;
    }

    /**
     * @brief Create a conditional operation for pipeline branching.
     * @param condition Function that returns true when condition is met
     * @return BufferOperation configured for conditional execution
     */
    static BufferOperation when(std::function<bool(uint32_t)> condition)
    {
        BufferOperation op(OpType::CONDITION);
        op.m_condition = condition;
        return op;
    }

    /**
     * @brief Create a dispatch operation for external processing.
     * @param handler Function to handle data with cycle information
     * @return BufferOperation configured for external dispatch
     */
    static BufferOperation dispatch_to(std::function<void(const Kakshya::DataVariant&, uint32_t)> handler)
    {
        BufferOperation op(OpType::DISPATCH);
        op.m_dispatch_handler = handler;
        return op;
    }

    /**
     * @brief Create a fusion operation for multiple AudioBuffer sources.
     * @param sources Vector of source buffers to fuse
     * @param fusion_func Function that combines multiple DataVariants
     * @param target Target buffer for fused result
     * @return BufferOperation configured for buffer fusion
     */
    static BufferOperation fuse_data(std::vector<std::shared_ptr<Buffers::AudioBuffer>> sources,
        std::function<Kakshya::DataVariant(const std::vector<Kakshya::DataVariant>&, uint32_t)> fusion_func,
        std::shared_ptr<Buffers::AudioBuffer> target)
    {
        BufferOperation op(OpType::FUSE);
        op.m_source_buffers = sources;
        op.m_fusion_function = fusion_func;
        op.m_target_buffer = target;
        return op;
    }

    /**
     * @brief Create a fusion operation for multiple DynamicSoundStream sources.
     * @param sources Vector of source containers to fuse
     * @param fusion_func Function that combines multiple DataVariants
     * @param target Target container for fused result
     * @return BufferOperation configured for container fusion
     */
    static BufferOperation fuse_containers(std::vector<std::shared_ptr<Kakshya::DynamicSoundStream>> sources,
        std::function<Kakshya::DataVariant(const std::vector<Kakshya::DataVariant>&, uint32_t)> fusion_func,
        std::shared_ptr<Kakshya::DynamicSoundStream> target)
    {
        BufferOperation op(OpType::FUSE);
        op.m_source_containers = sources;
        op.m_fusion_function = fusion_func;
        op.m_target_container = target;
        return op;
    }

    /**
     * @brief Create a CaptureBuilder for fluent capture configuration.
     * @param buffer AudioBuffer to capture from
     * @return CaptureBuilder for fluent operation construction
     */
    static CaptureBuilder capture_from(std::shared_ptr<Buffers::AudioBuffer> buffer)
    {
        return CaptureBuilder(buffer);
    }

    /**
     * @brief Set execution priority for scheduler ordering.
     * @param priority Priority value (0=highest, 255=lowest, default=128)
     * @return Reference to this operation for chaining
     */
    BufferOperation& with_priority(uint8_t priority)
    {
        m_priority = priority;
        return *this;
    }

    /**
     * @brief Set processing token for execution context.
     * @param token Processing token indicating execution context
     * @return Reference to this operation for chaining
     */
    BufferOperation& on_token(Buffers::ProcessingToken token)
    {
        m_token = token;
        return *this;
    }

    /**
     * @brief Set cycle interval for periodic execution.
     * @param n Execute every n cycles (default: 1)
     * @return Reference to this operation for chaining
     */
    BufferOperation& every_n_cycles(uint32_t n)
    {
        m_cycle_interval = n;
        return *this;
    }

    /**
     * @brief Assign identification tag.
     * @param tag String identifier for debugging and organization
     * @return Reference to this operation for chaining
     */
    BufferOperation& with_tag(const std::string& tag)
    {
        m_tag = tag;
        return *this;
    }

    // Accessors
    OpType get_type() const { return m_type; }
    uint8_t get_priority() const { return m_priority; }
    Buffers::ProcessingToken get_token() const { return m_token; }
    const std::string& get_tag() const { return m_tag; }

    BufferOperation(OpType type, BufferCapture capture)
        : m_type(type)
        , m_capture(std::move(capture))
        , m_tag(m_capture.get_tag())
    {
    }

private:
    explicit BufferOperation(OpType type)
        : m_type(type)
        , m_capture(nullptr)
    {
    }

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
 * auto pipeline = BufferPipeline(*scheduler)
 *     >> BufferOperation::capture_from(mic_buffer)
 *         .with_window(1024, 0.75f)
 *         .on_data_ready([](const auto& data, uint32_t cycle) {
 *             auto spectrum = fft_transform(data);
 *             detect_pitch(spectrum);
 *         })
 *     >> BufferOperation::route_to_container(recording_stream)
 *         .with_priority(64);
 *
 * pipeline.execute_continuous();
 * ```
 *
 * **Conditional Branching:**
 * ```cpp
 * pipeline.branch_if([](uint32_t cycle) { return cycle % 10 == 0; },
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
class BufferPipeline {
public:
    BufferPipeline() = default;
    explicit BufferPipeline(Vruta::TaskScheduler& scheduler)
        : m_scheduler(&scheduler)
    {
    }

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
     * @return Reference to this pipeline for continued chaining
     */
    BufferPipeline& branch_if(std::function<bool(uint32_t)> condition,
        std::function<void(BufferPipeline&)> branch_builder);

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

    // Execution control
    inline void execute_once() { execute_internal(1); }
    inline void execute_for_cycles(uint32_t cycles) { execute_internal(cycles); }
    void execute_continuous();
    inline void stop_continuous() { m_continuous_execution = false; }

    // State management
    void mark_data_consumed(uint32_t operation_index);
    bool has_pending_data() const;
    uint32_t get_current_cycle() const { return m_current_cycle; }

private:
    enum class DataState {
        EMPTY, ///< No data available
        READY, ///< Data ready for processing
        CONSUMED, ///< Data has been processed
        EXPIRED ///< Data has expired and should be cleaned up
    };

    std::vector<BufferOperation> m_operations;
    std::vector<std::pair<std::function<bool(uint32_t)>, BufferPipeline>> m_branches;
    std::vector<DataState> m_data_states;

    Vruta::TaskScheduler* m_scheduler = nullptr;
    uint32_t m_current_cycle = 0;
    bool m_continuous_execution = false;

    std::function<void(uint32_t)> m_cycle_start_callback;
    std::function<void(uint32_t)> m_cycle_end_callback;

    void execute_internal(uint32_t max_cycles);
    void process_operation(BufferOperation& op, uint32_t cycle);
    void process_branches(uint32_t cycle);
    void cleanup_expired_data();
    Kakshya::DataVariant extract_buffer_data(std::shared_ptr<Buffers::AudioBuffer> buffer);
    void write_to_buffer(std::shared_ptr<Buffers::AudioBuffer> buffer, const Kakshya::DataVariant& data);
    void write_to_container(std::shared_ptr<Kakshya::DynamicSoundStream> container, const Kakshya::DataVariant& data);
    Kakshya::DataVariant read_from_container(std::shared_ptr<Kakshya::DynamicSoundStream> container, uint64_t start, uint32_t length);

    std::unordered_map<BufferOperation*, Kakshya::DataVariant> m_operation_data;
};

/**
 * @class CycleCoordinator
 * @brief Cross-pipeline synchronization and coordination system.
 *
 * CycleCoordinator provides synchronization mechanisms for coordinating multiple
 * BufferPipeline instances and managing transient data lifecycles. It integrates
 * with the Vruta scheduling system to provide sample-accurate timing coordination
 * across complex processing networks.
 *
 * **Example Usage:**
 * ```cpp
 * CycleCoordinator coordinator(*scheduler);
 *
 * // Synchronize multiple analysis pipelines
 * auto sync_routine = coordinator.sync_pipelines({
 *     std::ref(spectral_pipeline),
 *     std::ref(temporal_pipeline),
 *     std::ref(feature_pipeline)
 * }, 4); // Sync every 4 cycles
 *
 * // Manage transient capture data
 * auto data_routine = coordinator.manage_transient_data(
 *     capture_buffer,
 *     [](uint32_t cycle) { std::cout << "Data ready: " << cycle << std::endl; },
 *     [](uint32_t cycle) { cleanup_expired_data(cycle); }
 * );
 *
 * scheduler->add_task(sync_routine);
 * scheduler->add_task(data_routine);
 * ```
 *
 * @see BufferPipeline For pipeline construction
 * @see BufferCapture For data capture strategies
 * @see Vruta::TaskScheduler For execution scheduling
 */
class CycleCoordinator {
public:
    /**
     * @brief Construct coordinator with task scheduler integration.
     * @param scheduler Vruta task scheduler for timing coordination
     */
    explicit CycleCoordinator(Vruta::TaskScheduler& scheduler);

    /**
     * @brief Create a synchronization routine for multiple pipelines.
     * @param pipelines Vector of pipeline references to synchronize
     * @param sync_every_n_cycles Synchronization interval in cycles
     * @return SoundRoutine that manages pipeline synchronization
     */
    Vruta::SoundRoutine sync_pipelines(
        std::vector<std::reference_wrapper<BufferPipeline>> pipelines,
        uint32_t sync_every_n_cycles = 1);

    /**
     * @brief Create a transient data management routine.
     * @param buffer AudioBuffer with transient data lifecycle
     * @param on_data_ready Callback when data becomes available
     * @param on_data_expired Callback when data expires
     * @return SoundRoutine that manages data lifecycle
     */
    Vruta::SoundRoutine manage_transient_data(
        std::shared_ptr<Buffers::AudioBuffer> buffer,
        std::function<void(uint32_t)> on_data_ready,
        std::function<void(uint32_t)> on_data_expired);

private:
    Vruta::TaskScheduler& m_scheduler;
};

}
