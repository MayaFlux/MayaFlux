#pragma once

#include "Capture.hpp"

#include "MayaFlux/Buffers/BufferSpec.hpp"
#include "MayaFlux/Core/ProcessingTokens.hpp"
#include "MayaFlux/Portal/Graphics/GraphicsUtils.hpp"

namespace MayaFlux {

namespace Core {
    class VKImage;
}

namespace Buffers {
    class BufferProcessor;
    class BufferManager;
    class TextureArrayBuffer;
}

namespace Kakshya {
    class DynamicSoundStream;
    class DynamicVideoStream;
    class VideoStreamContainer;
}

namespace IO {
    class IOManager;
    struct CameraConfig;
    struct VideoLoadConfig;
}

namespace Kriya {

    using TransformVectorFunction = std::function<Kakshya::DataVariant(std::vector<Kakshya::DataVariant>&, uint32_t)>;

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

    class BufferPipeline;

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
     * - **CAPTURE**: Extract data from a buffer using configurable capture strategies
     * - **TRANSFORM**: Apply functional transformations to data variants
     * - **ROUTE**: Direct data to audio or graphics buffers and streams
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
     *     }, Buffers::ProcessingToken::AUDIO_BACKEND)
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
         * @brief Open and capture a camera through IOManager.
         * @param render Draws the live camera when given; a camera buffer that
         *        already renders is left as it is. Without it the capture only
         *        routes the frames.
         */
        static BufferOperation capture_camera(
            const std::shared_ptr<IO::IOManager>& io_manager,
            const IO::CameraConfig& config,
            BufferCapture::CaptureMode mode = BufferCapture::CaptureMode::TRANSIENT,
            uint32_t cycle_count = 1,
            std::optional<Portal::Graphics::RenderConfig> render = std::nullopt);

        /**
         * @brief Open a camera and return its capture builder.
         * @param render Draws the live camera when given.
         */
        static CaptureBuilder capture_camera_from(
            const std::shared_ptr<IO::IOManager>& io_manager,
            const IO::CameraConfig& config,
            std::optional<Portal::Graphics::RenderConfig> render = std::nullopt);

        /**
         * @brief Create a file capture operation that reads from file and stores in stream.
         * @param io_manager IOManager for file loading
         * @param filepath Path to audio file
         * @param channel Channel index (default: 0)
         * @param cycle_count Number of cycles to capture (0 = continuous)
         * @return BufferOperation configured for file capture
         */
        static BufferOperation capture_file(
            const std::shared_ptr<IO::IOManager>& io_manager,
            const std::string& filepath,
            uint32_t channel = 0,
            uint32_t cycle_count = 0);

        /**
         * @brief Capture a video file with explicit video load configuration.
         * @param render Draws the video when given.
         */
        static BufferOperation capture_file(
            const std::shared_ptr<IO::IOManager>& io_manager,
            const std::string& filepath,
            IO::VideoLoadConfig config,
            uint32_t cycle_count = 1,
            std::optional<Portal::Graphics::RenderConfig> render = std::nullopt);

        /**
         * @brief Create CaptureBuilder for file with fluent configuration.
         * @param io_manager IOManager for file loading
         * @param filepath Path to audio file
         * @param channel Channel index (default: 0)
         * @return CaptureBuilder for fluent configuration
         */
        static CaptureBuilder capture_file_from(
            const std::shared_ptr<IO::IOManager>& io_manager,
            const std::string& filepath,
            uint32_t channel = 0);

        /**
         * @brief Create a capture builder for a video file.
         * @param render Draws the video when given.
         */
        static CaptureBuilder capture_file_from(
            const std::shared_ptr<IO::IOManager>& io_manager,
            const std::string& filepath,
            IO::VideoLoadConfig config,
            std::optional<Portal::Graphics::RenderConfig> render = std::nullopt);

        /**
         * @brief Create operation to route file data to DynamicSoundStream.
         * @param io_manager IOManager for file loading
         * @param filepath Path to audio file
         * @param target_stream Target DynamicSoundStream
         * @param cycle_count Number of cycles to read (0 = entire file)
         * @return BufferOperation configured for file to stream routing
         */
        static BufferOperation file_to_stream(
            const std::shared_ptr<IO::IOManager>& io_manager,
            const std::string& filepath,
            std::shared_ptr<Kakshya::DynamicSoundStream> target_stream,
            uint32_t cycle_count = 0);

        /**
         * @brief Route frames from a video file to a dynamic video stream.
         * @param render Draws the video file while it routes when given.
         */
        static BufferOperation file_to_stream(
            const std::shared_ptr<IO::IOManager>& io_manager,
            const std::string& filepath,
            std::shared_ptr<Kakshya::DynamicVideoStream> target_stream,
            IO::VideoLoadConfig config,
            uint32_t cycle_count = 0,
            std::optional<Portal::Graphics::RenderConfig> render = std::nullopt);

        /**
         * @brief Capture a camera straight into a dynamic video stream it creates.
         *
         * One operation opens the camera, hooks it to a buffer, makes a ring sized
         * from that buffer's real frame size and format, and each cycle appends the
         * camera frame to it. The stream is reached through get_graphics_stream() so
         * playback can be set up, for example lag_behind_head.
         *
         * @param ring_frames Frames the ring keeps; zero keeps three seconds at the
         *        registered frame rate.
         * @param live Draws the live camera when given.
         * @param display Draws the stream through the pipeline's IOManager when
         *        given; the pipeline needs one.
         */
        static BufferOperation capture_to_stream(
            const std::shared_ptr<IO::IOManager>& io_manager,
            const IO::CameraConfig& config,
            uint64_t ring_frames = 0,
            std::optional<Portal::Graphics::RenderConfig> live = std::nullopt,
            std::optional<Portal::Graphics::RenderConfig> display = std::nullopt);

        /**
         * @brief Capture a video file straight into a dynamic video stream it creates.
         *
         * The video counterpart of the camera form: it loads the file, hooks it to
         * a buffer and appends each played frame to a ring sized from that buffer.
         *
         * @param ring_frames Frames the ring keeps; zero keeps three seconds at the
         *        registered frame rate.
         * @param live Draws the playing file when given.
         * @param display Draws the stream through the pipeline's IOManager when
         *        given; the pipeline needs one.
         */
        static BufferOperation capture_to_stream(
            const std::shared_ptr<IO::IOManager>& io_manager,
            const std::string& filepath,
            IO::VideoLoadConfig config,
            uint64_t ring_frames = 0,
            std::optional<Portal::Graphics::RenderConfig> live = std::nullopt,
            std::optional<Portal::Graphics::RenderConfig> display = std::nullopt);

        /**
         * @brief The dynamic video stream this operation writes to.
         *
         * For capture_to_stream it is the stream the operation created; for a
         * route it is the stream routed to. Null for other operations. The audio
         * counterpart would be get_audio_stream.
         */
        [[nodiscard]] std::shared_ptr<Kakshya::DynamicVideoStream> get_graphics_stream() const
        {
            return m_target_graphics_stream;
        }

        /**
         * @brief Create a transform operation with custom transformation function.
         * @param transformer Function that transforms DataVariant with cycle information
         * @param token Processing domain for the operation
         * @return BufferOperation configured for data transformation
         */
        static BufferOperation transform(TransformationFunction transformer,
            Buffers::ProcessingToken token = Buffers::ProcessingToken::AUDIO_BACKEND);

        /**
         * @brief Create a routing operation to AudioBuffer destination.
         * @param target Target AudioBuffer to receive data.
         * @return BufferOperation configured for buffer routing.
         *
         * ROUTE is a batch output operation. It writes the accumulated or transformed
         * result of a pipeline into a target buffer or container after processing is
         * complete. It is not intended for live per-cycle routing: that is covered by
         * supply, clone, and processing chains on individual buffers. The typical use
         * is to accumulate or transform over N cycles via CAPTURE/TRANSFORM, then
         * route the result to a buffer or DynamicSoundStream for playback or storage
         * via SoundContainerBuffer.
         */
        static BufferOperation route_to_buffer(std::shared_ptr<Buffers::AudioBuffer> target);

        /** @brief Route data to a graphics buffer. */
        static BufferOperation route_to_buffer(std::shared_ptr<Buffers::VKBuffer> target);

        /**
         * @brief Route frames into one layer of a texture array buffer.
         *
         * A capture of an image buffer followed directly by this route is copied
         * on the GPU with no host read, and the array buffer fits the frame to its
         * layer. Raw bytes carry no extent and must already match the layer.
         *
         * @param target Array buffer that receives the frames.
         * @param layer  Layer that holds this source.
         */
        static BufferOperation route_to_buffer(
            std::shared_ptr<Buffers::TextureArrayBuffer> target, uint32_t layer = 0);

        /**
         * @brief Route many GPU images into the layers of a texture array buffer.
         *
         * Each cycle images[i] is copied into layer i on the GPU, and the array
         * buffer fits it to the layer. An image that stays the same object is copied
         * again with whatever it holds that cycle.
         *
         * @param target Array buffer that receives the frames.
         * @param images One image per layer, in layer order.
         */
        static BufferOperation route_to_buffer(
            std::shared_ptr<Buffers::TextureArrayBuffer> target,
            std::vector<std::shared_ptr<Core::VKImage>> images);

        /**
         * @brief Create a routing operation to DynamicSoundStream destination.
         *
         * Each routed block is appended at the stream's write head for
         * @p channel, wrapping if the stream is circular, so repeated routing
         * records a continuous signal.
         *
         * @param target Target container to receive data.
         * @param channel Channel of the target that receives the data.
         * @return BufferOperation configured for container routing.
         *
         * @see route_to_buffer for usage intent and constraints.
         */
        static BufferOperation route_to_container(std::shared_ptr<Kakshya::DynamicSoundStream> target, uint32_t channel = 0);

        /**
         * @brief Append frames to a dynamic video stream.
         *
         * @param render Draws the stream in a window when given. The pipeline
         *        hooks the stream to a display buffer through its IOManager when
         *        it starts, as the audio route attaches its writer through the
         *        BufferManager, so the pipeline needs an IOManager and the stream
         *        must already hold data or have its ring enabled.
         */
        static BufferOperation route_to_container(
            std::shared_ptr<Kakshya::DynamicVideoStream> target,
            std::optional<Portal::Graphics::RenderConfig> render = std::nullopt);

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

        /** @brief Load video frames from a dynamic stream into a graphics buffer. */
        static BufferOperation load_from_container(
            std::shared_ptr<Kakshya::DynamicVideoStream> source,
            std::shared_ptr<Buffers::VKBuffer> target,
            uint64_t start_frame = 0,
            uint32_t length = 0);

        /**
         * @brief Create a conditional operation for pipeline branching.
         * @param condition Function that returns true when condition is met
         * @param token Processing domain for the operation
         * @return BufferOperation configured for conditional execution
         */
        static BufferOperation when(std::function<bool(uint32_t)> condition,
            Buffers::ProcessingToken token = Buffers::ProcessingToken::AUDIO_BACKEND);

        /**
         * @brief Create a dispatch operation for external processing.
         * @param handler Function to handle data with cycle information
         * @param token Processing domain for the operation
         * @return BufferOperation configured for external dispatch
         */
        static BufferOperation dispatch_to(OperationFunction handler,
            Buffers::ProcessingToken token = Buffers::ProcessingToken::AUDIO_BACKEND);

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
            Buffers::AudioProcessingFunction modifier);

        /** @brief Attach a graphics quick processor to a Vulkan buffer. */
        static BufferOperation modify_buffer(
            std::shared_ptr<Buffers::VKBuffer> buffer,
            Buffers::GraphicsProcessingFunction modifier);

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

        /** @brief Fuse graphics buffer data into a graphics destination. */
        static BufferOperation fuse_data(
            std::vector<std::shared_ptr<Buffers::VKBuffer>> sources,
            TransformVectorFunction fusion_func,
            std::shared_ptr<Buffers::VKBuffer> target);

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

        /** @brief Fuse video stream frames into a dynamic video stream. */
        static BufferOperation fuse_containers(
            std::vector<std::shared_ptr<Kakshya::DynamicVideoStream>> sources,
            TransformVectorFunction fusion_func,
            std::shared_ptr<Kakshya::DynamicVideoStream> target);

        /**
         * @brief Create a CaptureBuilder for fluent capture configuration.
         * @param buffer Buffer to capture from
         * @return CaptureBuilder for fluent operation construction
         *
         * @note Register the buffer with BufferManager before automatic processing.
         */
        static CaptureBuilder capture_from(std::shared_ptr<Buffers::AudioBuffer> buffer);
        static CaptureBuilder capture_from(
            std::shared_ptr<Buffers::VKBuffer> buffer,
            std::optional<Portal::Graphics::RenderConfig> render = std::nullopt);
        static CaptureBuilder capture_from(std::nullptr_t);

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

        /** @brief Return the operation's processing token. */
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

        BufferOperation(OpType type, Buffers::ProcessingToken token);

        static bool is_capture_phase_operation(const BufferOperation& op);

        static bool is_process_phase_operation(const BufferOperation& op);

    private:
        ExecutionPhase m_execution_phase { ExecutionPhase::AUTO };
        OpType m_type;
        BufferCapture m_capture;
        uint32_t m_modify_cycle_count {};
        bool m_is_streaming {};

        TransformationFunction m_transformer;
        Buffers::AudioProcessingFunction m_audio_buffer_modifier;
        Buffers::GraphicsProcessingFunction m_graphics_buffer_modifier;

        std::shared_ptr<Buffers::AudioBuffer> m_target_audio_buffer;
        std::shared_ptr<Buffers::VKBuffer> m_target_graphics_buffer;
        std::shared_ptr<Kakshya::DynamicSoundStream> m_target_audio_stream;
        std::shared_ptr<Kakshya::DynamicVideoStream> m_target_graphics_stream;
        std::shared_ptr<Buffers::VKBuffer> m_source_graphics_buffer;
        std::optional<Portal::Graphics::RenderConfig> m_render;
        bool m_display_attached {};
        uint32_t m_target_audio_channel {};
        uint32_t m_target_layer {};
        std::vector<std::shared_ptr<Core::VKImage>> m_source_images;

        std::shared_ptr<Buffers::BufferProcessor> m_attached_processor;

        std::shared_ptr<Kakshya::DynamicSoundStream> m_source_audio_stream;
        std::shared_ptr<Kakshya::VideoStreamContainer> m_source_graphics_stream;
        uint64_t m_start_frame {};
        uint32_t m_load_length {};

        std::function<bool(uint32_t)> m_condition;
        OperationFunction m_dispatch_handler;

        std::vector<std::shared_ptr<Buffers::AudioBuffer>> m_source_audio_buffers;
        std::vector<std::shared_ptr<Buffers::VKBuffer>> m_source_graphics_buffers;
        std::vector<std::shared_ptr<Kakshya::DynamicSoundStream>> m_source_audio_streams;
        std::vector<std::shared_ptr<Kakshya::DynamicVideoStream>> m_source_graphics_streams;
        TransformVectorFunction m_fusion_function;

        uint8_t m_priority = 128;
        Buffers::ProcessingToken m_token { Buffers::ProcessingToken::AUDIO_BACKEND };
        uint32_t m_cycle_interval = 1;
        std::string m_tag;

        friend class BufferPipeline;
    };

} // namespace Kriya

} // namespace MayaFlux
