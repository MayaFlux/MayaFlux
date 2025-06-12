#pragma once

#include "MayaFlux/Kakshya/DataProcessor.hpp"
#include "MayaFlux/Kakshya/KakshyaUtils.hpp"

/**
 * @class ContiguousAccessProcessor
 * @brief Data Processor for efficient, sequential access to N-dimensional data containers.
 *
 * ContiguousAccessProcessor is the default processor for streaming, reading, and processing
 * N-dimensional data in a linear, memory-efficient manner. It is designed for digital-first,
 * data-driven workflows and supports:
 * - Efficient sequential access to multi-dimensional data (audio, images, tensors, etc.)
 * - Both row-major and column-major memory layouts
 * - Automatic or manual advancement of read position for streaming or block-based processing
 * - Looping and region-based access for playback, streaming, or repeated analysis
 * - Flexible output buffer sizing and dimension selection for custom workflows
 *
 * This processor is foundational for scenarios such as:
 * - Real-time audio or signal streaming
 * - Batch or block-based data processing
 * - Efficient extraction of contiguous regions for machine learning or DSP
 * - Integration with digital-first nodes, routines, and buffer systems
 *
 * Unlike analog-inspired processors, ContiguousAccessProcessor is unconstrained by
 * legacy metaphors and is optimized for modern, data-centric applications.
 */
namespace MayaFlux::Kakshya {

class ContiguousAccessProcessor : public DataProcessor {
public:
    ContiguousAccessProcessor() = default;
    ~ContiguousAccessProcessor() = default;

    /**
     * @brief Attach the processor to a signal source container.
     * Initializes dimension metadata, memory layout, and prepares for processing.
     * @param container The SignalSourceContainer to attach to.
     */
    void on_attach(std::shared_ptr<SignalSourceContainer> container) override;

    /**
     * @brief Detach the processor from its container.
     * Cleans up internal state and metadata.
     * @param container The SignalSourceContainer to detach from.
     */
    void on_detach(std::shared_ptr<SignalSourceContainer> container) override;

    /**
     * @brief Process the current region or block of data.
     * Advances the read position if auto-advance is enabled.
     * @param container The SignalSourceContainer to process.
     */
    void process(std::shared_ptr<SignalSourceContainer> container) override;

    /**
     * @brief Query if the processor is currently performing processing.
     * @return true if processing is in progress, false otherwise.
     */
    bool is_processing() const override { return m_is_processing.load(); }

    /**
     * @brief Set the output buffer size (shape) for each processing call.
     * @param shape Vector specifying the size in each dimension.
     */
    void set_output_size(const std::vector<u_int64_t>& shape) { m_output_shape = shape; }

    /**
     * @brief Set which dimensions to process (by index).
     * Allows for partial or selective processing of multi-dimensional data.
     * @param dims Indices of active dimensions.
     */
    void set_active_dimensions(const std::vector<u_int32_t>& dims) { m_active_dimensions = dims; }

    /**
     * @brief Enable or disable automatic advancement of the read position after each process call.
     * @param enable true to auto-advance, false for manual control.
     */
    void set_auto_advance(bool enable) { m_auto_advance = enable; }

    /**
     * @brief Set the current read position (N-dimensional coordinates).
     * @param new_position New position vector.
     */
    inline void set_current_position(const std::vector<u_int64_t> new_position)
    {
        m_current_position = new_position;
    }

private:
    // Processing state
    std::atomic<bool> m_is_processing { false };
    bool m_prepared = false;
    bool m_auto_advance = true;

    // Container reference
    std::weak_ptr<SignalSourceContainer> m_source_container_weak;

    // Dimension information
    std::vector<DataDimension> m_dimensions;
    std::vector<u_int32_t> m_active_dimensions;
    MemoryLayout m_memory_layout;

    // Position tracking
    std::vector<u_int64_t> m_current_position;
    std::vector<u_int64_t> m_output_shape;

    // Loop configuration
    bool m_looping_enabled = false;
    Region m_loop_region;

    // Metadata
    u_int64_t m_total_elements = 0;
    std::chrono::steady_clock::time_point m_last_process_time;

    // ===== Helper methods =====

    /**
     * @brief Store dimension and layout metadata from the container.
     * @param container The SignalSourceContainer to query.
     */
    void store_metadata(std::shared_ptr<SignalSourceContainer> container);

    /**
     * @brief Validate the container's structure and output configuration.
     * Throws if configuration is invalid.
     * @param container The SignalSourceContainer to validate.
     */
    void validate_container(std::shared_ptr<SignalSourceContainer> container);

    /**
     * @brief Process a specific region and write to the output buffer.
     * @param container The SignalSourceContainer to read from.
     * @param region The region to process.
     * @param output Output DataVariant to fill.
     */
    void process_region(std::shared_ptr<SignalSourceContainer> container,
        const Region& region,
        DataVariant& output);

    /**
     * @brief Advance the read position by the output shape.
     * Handles looping if enabled.
     * @param position Current position vector (modified in place).
     * @param shape Output shape vector.
     */
    void advance_read_position(std::vector<u_int64_t>& position, const std::vector<u_int64_t>& shape);

    /**
     * @brief Calculate the output region based on current position and shape.
     * @param current_pos Current position vector.
     * @param output_shape Output shape vector.
     * @return Region representing the region to process.
     */
    Region calculate_output_region(const std::vector<u_int64_t>& current_pos,
        const std::vector<u_int64_t>& output_shape) const;

    /**
     * @brief Handle looping logic for the current position.
     * @param position Position vector to update.
     */
    void handle_looping(std::vector<u_int64_t>& position);
};

} // namespace MayaFlux::Kakshya
