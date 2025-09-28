#pragma once
#include "MayaFlux/Kakshya/NDData.hpp"

namespace MayaFlux {

namespace Buffers {
    class Buffer;
    class AudioBuffer;
}

namespace Kakshya {
    class DynamicSoundStream;
}

namespace Kriya {

    class BufferOperation;

    /**
     * @class BufferCapture
     * @brief Flexible data capture interface for buffer-based processing pipelines.
     *
     * BufferCapture provides sophisticated data capture capabilities within the Kriya
     * processing system, enabling various capture modes and lifecycle management for
     * buffer data. It supports transient, accumulative, windowed, and circular capture
     * patterns for different real-time processing scenarios.
     *
     * **Capture Modes:**
     * - **TRANSIENT**: Single-cycle capture with automatic expiration (default)
     * - **ACCUMULATE**: Multi-cycle accumulation in persistent container
     * - **TRIGGERED**: Conditional capture based on predicates
     * - **WINDOWED**: Rolling window capture with configurable overlap
     * - **CIRCULAR**: Fixed-size circular buffer with overwrite behavior
     *
     * **Key Features:**
     * - Multiple capture strategies for different use cases
     * - Callback system for data lifecycle events
     * - Metadata and tagging support for organization
     * - Integration with BufferPipeline and BufferOperation
     * - Sample-accurate timing and synchronization
     *
     * **Use Cases:**
     * - Real-time audio analysis and feature extraction
     * - Streaming data collection for machine learning
     * - Circular delay lines and feedback systems
     * - Windowed processing for spectral analysis
     * - Event-driven data capture and recording
     *
     * @see BufferOperation For pipeline integration
     * @see BufferPipeline For execution context
     * @see CaptureBuilder For fluent construction
     */
    class BufferCapture {
    public:
        /**
         * @enum CaptureMode
         * @brief Defines the data capture and retention strategy.
         */
        enum class CaptureMode : u_int8_t {
            TRANSIENT, ///< Single cycle capture (default) - data expires after 1 cycle
            ACCUMULATE, ///< Accumulate over multiple cycles in container
            TRIGGERED, ///< Capture only when condition met
            WINDOWED, ///< Rolling window capture with overlap
            CIRCULAR ///< Circular buffer with overwrite
        };

        /**
         * @enum ProcessingControl
         * @brief Controls how and when data processing occurs.
         */
        enum class ProcessingControl : u_int8_t {
            AUTOMATIC, // Let BufferManager handle processing (default)
            ON_CAPTURE, // Only process when capture reads data
            MANUAL // User controls processing explicitly
        };

        /**
         * @brief Construct a BufferCapture with specified mode and parameters.
         * @param buffer Target AudioBuffer to capture from
         * @param mode Capture strategy to use (default: TRANSIENT)
         * @param cycle_count Number of cycles for multi-cycle modes (default: 1)
         */
        BufferCapture(std::shared_ptr<Buffers::AudioBuffer> buffer,
            CaptureMode mode = CaptureMode::TRANSIENT,
            u_int32_t cycle_count = 1);

        /**
         * @brief Set processing control strategy.
         * @param control ProcessingControl strategy (default: AUTOMATIC)
         * @return Reference to this BufferCapture for chaining
         */
        BufferCapture& with_processing_control(ProcessingControl control);

        /**
         * @brief Set the number of cycles to capture data.
         * @param count Number of processing cycles to capture
         * @return Reference to this BufferCapture for chaining
         */
        BufferCapture& for_cycle(u_int32_t count);

        /**
         * @brief Set a condition that stops capture when met.
         * @param predicate Function returning true when capture should stop
         * @return Reference to this BufferCapture for chaining
         */
        BufferCapture& until_condition(std::function<bool()> predicate);

        /**
         * @brief Configure windowed capture with overlap.
         * @param window_size Size of each capture window
         * @param overlap_ratio Overlap between windows (0.0-1.0, default: 0.0)
         * @return Reference to this BufferCapture for chaining
         */
        BufferCapture& with_window(u_int32_t window_size, float overlap_ratio = 0.0F);

        /**
         * @brief Enable circular buffer mode with fixed size.
         * @param size Fixed size of the circular buffer
         * @return Reference to this BufferCapture for chaining
         */
        BufferCapture& as_circular(u_int32_t size);

        /**
         * @brief Set callback for when captured data is ready.
         * @param callback Function called with captured data and cycle number
         * @return Reference to this BufferCapture for chaining
         */
        BufferCapture& on_data_ready(std::function<void(const Kakshya::DataVariant&, u_int32_t)> callback);

        /**
         * @brief Set callback for when a capture cycle completes.
         * @param callback Function called with cycle number
         * @return Reference to this BufferCapture for chaining
         */
        BufferCapture& on_cycle_complete(std::function<void(u_int32_t)> callback);

        /**
         * @brief Set callback for when captured data expires.
         * @param callback Function called with expired data and cycle number
         * @return Reference to this BufferCapture for chaining
         */
        BufferCapture& on_data_expired(std::function<void(const Kakshya::DataVariant&, u_int32_t)> callback);

        /**
         * @brief Assign a tag for identification and organization.
         * @param tag String identifier for this capture
         * @return Reference to this BufferCapture for chaining
         */
        BufferCapture& with_tag(const std::string& tag);

        /**
         * @brief Add metadata key-value pair.
         * @param key Metadata key
         * @param value Metadata value
         * @return Reference to this BufferCapture for chaining
         */
        BufferCapture& with_metadata(const std::string& key, const std::string& value);

        // Accessors
        inline std::shared_ptr<Buffers::AudioBuffer> get_buffer() const { return m_buffer; }
        inline CaptureMode get_mode() const { return m_mode; }
        inline ProcessingControl get_processing_control() const { return m_processing_control; }
        inline u_int32_t get_cycle_count() const { return m_cycle_count; }
        inline const std::string& get_tag() const { return m_tag; }
        inline u_int32_t get_circular_size() const { return m_circular_size; }
        inline u_int32_t get_window_size() const { return m_window_size; }
        inline float get_overlap_ratio() const { return m_overlap_ratio; }
        inline void set_processing_control(ProcessingControl control) { m_processing_control = control; }

    private:
        std::shared_ptr<Buffers::AudioBuffer> m_buffer;
        CaptureMode m_mode;
        ProcessingControl m_processing_control = ProcessingControl::AUTOMATIC;
        u_int32_t m_cycle_count;
        u_int32_t m_window_size;
        u_int32_t m_circular_size;
        float m_overlap_ratio;

        std::function<bool()> m_stop_condition;
        std::function<void(const Kakshya::DataVariant&, u_int32_t)> m_data_ready_callback;
        std::function<void(u_int32_t)> m_cycle_callback;
        std::function<void(const Kakshya::DataVariant&, u_int32_t)> m_data_expired_callback;

        std::string m_tag;
        std::unordered_map<std::string, std::string> m_metadata;

        friend class BufferOperation;
        friend class BufferPipeline;
    };

    /**
     * @class CaptureBuilder
     * @brief Fluent builder interface for constructing BufferCapture configurations.
     *
     * CaptureBuilder provides a fluent, chainable API for constructing BufferCapture
     * objects with complex configurations. It follows the builder pattern to make
     * capture setup more readable and maintainable, especially for advanced capture
     * scenarios with multiple parameters and callbacks.
     *
     * **Design Philosophy:**
     * The builder pattern is used here to address the complexity of BufferCapture
     * configuration while maintaining type safety and compile-time validation.
     * It provides a more expressive API than constructor overloading for complex
     * capture scenarios.
     *
     * **Integration:**
     * CaptureBuilder seamlessly converts to BufferOperation, allowing it to be
     * used directly in BufferPipeline construction without explicit conversion.
     * This maintains the fluent nature of the Kriya pipeline API.
     *
     * **Example Usage:**
     * ```cpp
     * auto capture_op = CaptureBuilder(audio_buffer)
     *     .for_cycles(10)
     *     .with_window(512, 0.5f)
     *     .on_data_ready([](const auto& data, u_int32_t cycle) {
     *         process_windowed_data(data, cycle);
     *     })
     *     .with_tag("spectral_analysis");
     *
     * pipeline >> capture_op >> route_to_container(output_stream);
     * ```
     */
    class CaptureBuilder {
    public:
        /**
         * @brief Construct builder with target AudioBuffer.
         * @param buffer AudioBuffer to capture from
         */
        explicit CaptureBuilder(std::shared_ptr<Buffers::AudioBuffer> buffer);

        /**
         * @brief Set number of cycles to capture (enables ACCUMULATE mode).
         * @param count Number of processing cycles
         * @return Reference to this builder for chaining
         */
        CaptureBuilder& for_cycles(u_int32_t count);

        /**
         * @brief Set processing control to ON_CAPTURE mode.
         * @return Reference to this builder for chaining
         */
        CaptureBuilder& on_capture_processing();

        /**
         * @brief Set processing control to MANUAL mode.
         * @return Reference to this builder for chaining
         */
        CaptureBuilder& manual_processing();

        /**
         * @brief Set processing control to AUTOMATIC mode.
         * @return Reference to this builder for chaining
         */
        CaptureBuilder& auto_processing();

        /**
         * @brief Set stop condition (enables TRIGGERED mode).
         * @param predicate Function returning true when capture should stop
         * @return Reference to this builder for chaining
         */
        CaptureBuilder& until_condition(std::function<bool()> predicate);

        /**
         * @brief Enable circular buffer mode with specified size.
         * @param buffer_size Fixed size of the circular buffer
         * @return Reference to this builder for chaining
         */
        CaptureBuilder& as_circular(u_int32_t buffer_size);

        /**
         * @brief Configure windowed capture (enables WINDOWED mode).
         * @param window_size Size of each capture window
         * @param overlap_ratio Overlap between windows (0.0-1.0)
         * @return Reference to this builder for chaining
         */
        CaptureBuilder& with_window(u_int32_t window_size, float overlap_ratio = 0.0F);

        /**
         * @brief Set data ready callback.
         * @param callback Function called when data is available
         * @return Reference to this builder for chaining
         */
        CaptureBuilder& on_data_ready(std::function<void(const Kakshya::DataVariant&, u_int32_t)> callback);

        /**
         * @brief Set cycle completion callback.
         * @param callback Function called at end of each cycle
         * @return Reference to this builder for chaining
         */
        CaptureBuilder& on_cycle_complete(std::function<void(u_int32_t)> callback);

        /**
         * @brief Set data expiration callback.
         * @param callback Function called when data expires
         * @return Reference to this builder for chaining
         */
        CaptureBuilder& on_data_expired(std::function<void(const Kakshya::DataVariant&, u_int32_t)> callback);

        /**
         * @brief Assign identification tag.
         * @param tag String identifier for this capture
         * @return Reference to this builder for chaining
         */
        CaptureBuilder& with_tag(const std::string& tag);

        /**
         * @brief Add metadata key-value pair.
         * @param key Metadata key
         * @param value Metadata value
         * @return Reference to this builder for chaining
         */
        CaptureBuilder& with_metadata(const std::string& key, const std::string& value);

        /**
         * @brief Get the assigned tag for this capture.
         * @return Tag string
         */
        inline std::string get_tag() { return m_capture.get_tag(); }

        /**
         * @brief Convert to BufferOperation for pipeline integration.
         * @return BufferOperation containing this capture configuration
         */
        operator BufferOperation();

    private:
        BufferCapture m_capture; ///< Internal BufferCapture being configured
    };

}
}
