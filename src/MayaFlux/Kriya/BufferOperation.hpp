#pragma once

#include "Capture.hpp"

#include "MayaFlux/Core/ProcessingTokens.hpp"

namespace MayaFlux {

namespace Buffers {
    class BufferProcessor;
    class BufferManager;
}

namespace Kakshya {
    class DynamicSoundStream;
}

namespace Kriya {

    using TransformVectorFunction = std::function<Kakshya::DataVariant(std::vector<Kakshya::DataVariant>&, uint32_t)>;
    using AudioProcessingFunction = std::function<void(std::shared_ptr<Buffers::AudioBuffer>)>;

    /**
     * @enum ExecutionStrategy
     * @brief Defines how operations in a pipeline are coordinated and executed
     */
    enum class ExecutionStrategy : uint8_t {
        /**
         * PHASED: Traditional phased execution (default)
         * - All CAPTURE operations complete first (capture phase)
         * - Then all processing operations execute (process phase)
         * - Best for: accumulation, windowed analysis, batch processing
         * - Predictable data availability, clear phase boundaries
         */
        PHASED,

        /**
         * STREAMING: Immediate flow-through execution
         * - Each capture iteration flows immediately through dependent operations
         * - Minimal latency, data processed as it arrives
         * - Best for: real-time effects, low-latency processing, modify_buffer chains
         * - Natural for operations that modify state continuously
         */
        STREAMING,

        /**
         * PARALLEL: Concurrent capture with synchronization
         * - Multiple capture operations can run concurrently
         * - Explicit synchronization points coordinate data flow
         * - Best for: multi-source capture, independent data streams
         * - Requires CycleCoordinator for proper synchronization
         */
        PARALLEL,

        /**
         * REACTIVE: Data-driven reactive execution
         * - Operations execute when input data becomes available
         * - Dynamic dependency resolution
         * - Best for: event-driven workflows, complex dependencies
         * - Non-deterministic execution order
         */
        REACTIVE
    };

    // Forward declarations
    // class BufferOperation;
    class BufferPipeline;
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
    private:
        enum class ExecutionPhase : uint8_t {
            AUTO, // Automatically determined by operation type
            CAPTURE, // Explicitly runs in capture phase
            PROCESS // Explicitly runs in process phase
        };

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
            FUSE, ///< Fuse multiple sources using custom fusion functions
            MODIFY ///< Modify Buffer Data using custom quick process
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
        static BufferOperation transform(TransformationFunction transformer);

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
        static BufferOperation dispatch_to(OperationFunction handler);

        /**
         * @brief Create a modify operation for direct buffer manipulation.
         * @param buffer AudioBuffer to modify in-place
         * @param modifier Function that modifies buffer data directly
         * @return BufferOperation configured for buffer modification
         *
         * Unlike TRANSFORM which works on data copies, MODIFY attaches a processor
         * to the buffer that modifies it in-place during buffer processing.
         * The processor is automatically managed based on pipeline lifecycle.
         */
        static BufferOperation modify_buffer(
            std::shared_ptr<Buffers::AudioBuffer> buffer,
            AudioProcessingFunction modifier);

        /**
         * @brief Create a fusion operation for multiple AudioBuffer sources.
         * @param sources Vector of source buffers to fuse
         * @param fusion_func Function that combines multiple DataVariants
         * @param target Target buffer for fused result
         * @return BufferOperation configured for buffer fusion
         */
        static BufferOperation fuse_data(std::vector<std::shared_ptr<Buffers::AudioBuffer>> sources,
            TransformVectorFunction fusion_func,
            std::shared_ptr<Buffers::AudioBuffer> target);

        /**
         * @brief Create a fusion operation for multiple DynamicSoundStream sources.
         * @param sources Vector of source containers to fuse
         * @param fusion_func Function that combines multiple DataVariants
         * @param target Target container for fused result
         * @return BufferOperation configured for container fusion
         */
        static BufferOperation fuse_containers(std::vector<std::shared_ptr<Kakshya::DynamicSoundStream>> sources,
            TransformVectorFunction fusion_func,
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

        BufferOperation& for_cycles(uint32_t count);

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

        /**
         * @brief Hint that this operation should execute in capture phase
         * @return Reference to this operation for chaining
         */
        BufferOperation& as_capture_phase()
        {
            m_execution_phase = ExecutionPhase::CAPTURE;
            return *this;
        }

        /**
         * @brief Hint that this operation should execute in process phase
         * @return Reference to this operation for chaining
         */
        BufferOperation& as_process_phase();

        /**
         * @brief Mark this operation as streaming (executes continuously)
         * Useful for modify_buffer and similar stateful operations
         * @return Reference to this operation for chaining
         */
        BufferOperation& as_streaming();

        /**
         * @brief Check if this operation is a streaming operation
         */
        inline bool is_streaming() const { return m_is_streaming; }

        /**
         * @brief Get the execution phase hint for this operation
         */
        ExecutionPhase get_execution_phase() const { return m_execution_phase; }

        BufferOperation(OpType type, BufferCapture capture);

        explicit BufferOperation(OpType type);

        static bool is_capture_phase_operation(const BufferOperation& op);

        static bool is_process_phase_operation(const BufferOperation& op);

    private:
        ExecutionPhase m_execution_phase { ExecutionPhase::AUTO };
        OpType m_type;
        BufferCapture m_capture;
        uint32_t m_modify_cycle_count {};
        bool m_is_streaming {};

        TransformationFunction m_transformer;
        AudioProcessingFunction m_buffer_modifier;

        std::shared_ptr<Buffers::AudioBuffer> m_target_buffer;
        std::shared_ptr<Kakshya::DynamicSoundStream> m_target_container;

        std::shared_ptr<Buffers::BufferProcessor> m_attached_processor;

        std::shared_ptr<Kakshya::DynamicSoundStream> m_source_container;
        uint64_t m_start_frame {};
        uint32_t m_load_length {};

        std::function<bool(uint32_t)> m_condition;
        OperationFunction m_dispatch_handler;

        std::vector<std::shared_ptr<Buffers::AudioBuffer>> m_source_buffers;
        std::vector<std::shared_ptr<Kakshya::DynamicSoundStream>> m_source_containers;
        TransformVectorFunction m_fusion_function;

        uint8_t m_priority = 128;
        Buffers::ProcessingToken m_token = Buffers::ProcessingToken::AUDIO_BACKEND;
        uint32_t m_cycle_interval = 1;
        std::string m_tag;

        friend class BufferPipeline;
    };

} // namespace Kriya

} // namespace MayaFlux
