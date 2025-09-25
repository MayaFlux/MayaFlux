#pragma once

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Buffers/BufferProcessor.hpp"
#include "MayaFlux/Kakshya/StreamContainer.hpp"

namespace MayaFlux::Buffers {

/**
 * @class ContainerToBufferAdapter
 * @brief Adapter for bridging N-dimensional containers and AudioBuffer interface.
 *
 * ContainerToBufferAdapter enables seamless integration between N-dimensional
 * data containers (such as Kakshya::StreamContainer or SoundFileContainer) and the AudioBuffer
 * processing system. It extracts audio data from containers and presents it as a standard
 * AudioBuffer for use in block-based DSP, node networks, and hardware output.
 *
 * Key responsibilities:
 * - Maps N-dimensional container data (time/channel/other axes) to linear audio buffer format.
 * - Handles dimension selection, channel extraction, and position tracking.
 * - Synchronizes processing state and lifecycle between container and buffer.
 * - Supports automatic or manual advancement of read position for streaming or block-based workflows.
 * - Enables zero-copy operation when possible, falling back to cached extraction as needed.
 *
 * This adapter is foundational for digital-first workflows where data may originate from
 * files, streams, or procedural sources, and must be routed into the buffer system for
 * further processing or output. While currently focused on audio, the design can be
 * extended to support other data container types as more reader processors are implemented.
 *
 * @see ContainerBuffer, StreamContainer, SoundFileContainer, ContiguousAccessProcessor
 */
class ContainerToBufferAdapter : public BufferProcessor {
public:
    explicit ContainerToBufferAdapter(std::shared_ptr<Kakshya::StreamContainer> container);

    /**
     * @brief Extracts and processes data from the container into the target AudioBuffer.
     * Handles dimension mapping, position tracking, and state synchronization.
     * @param buffer The AudioBuffer to fill.
     */
    void processing_function(std::shared_ptr<Buffer> buffer) override;

    /**
     * @brief Attach the adapter to an AudioBuffer.
     * Registers for container state changes and prepares for processing.
     * @param buffer The AudioBuffer to attach to.
     */
    void on_attach(std::shared_ptr<Buffer> buffer) override;

    /**
     * @brief Detach the adapter from its AudioBuffer.
     * Cleans up state and unregisters callbacks.
     * @param buffer The AudioBuffer to detach from.
     */
    void on_detach(std::shared_ptr<Buffer> buffer) override;

    /**
     * @brief Set which channel dimension to extract from the container.
     * @param channel_index Index in the channel dimension (default: 0)
     */
    void set_source_channel(u_int32_t channel_index);
    u_int32_t get_source_channel() const { return m_source_channel; }

    /**
     * @brief Set the container to adapt.
     * @param container The StreamContainer to extract data from.
     */
    void set_container(std::shared_ptr<Kakshya::StreamContainer> container);
    std::shared_ptr<Kakshya::StreamContainer> get_container() const { return m_container; }

    /**
     * @brief Enable or disable automatic advancement of the container's read position.
     * When enabled, the adapter will advance the container after each process call.
     * @param enable True to enable auto-advance, false for manual control.
     */
    void set_auto_advance(bool enable) { m_auto_advance = enable; }
    bool get_auto_advance() const { return m_auto_advance; }

    /**
     * @brief Enable or disable buffer state flag updates.
     * When enabled, the adapter will update buffer processing/removal flags based on container state.
     * @param update True to enable state updates.
     */
    void set_update_flags(bool update) { m_update_flags = update; }
    bool get_update_flags() const { return m_update_flags; }

private:
    std::shared_ptr<Kakshya::StreamContainer> m_container;
    u_int32_t m_source_channel {};
    bool m_auto_advance = true;
    bool m_update_flags = true;

    u_int32_t m_time_reader_id = UINT32_MAX;
    u_int32_t m_channel_reader_id = UINT32_MAX;

    u_int32_t m_num_channels { 1 };
    u_int32_t m_reader_id {};

    // Cache for efficiency
    mutable std::vector<double> m_temp_buffer;

    /**
     * @brief Analyze the container's dimensions and update mapping info.
     */
    void analyze_container_dimensions();

    /**
     * @brief Extract channel data from the container into the output buffer.
     * @param output Output span to fill.
     */
    void extract_channel_data(std::span<double> output);

    /**
     * @brief Respond to container state changes (e.g., READY, PROCESSED, NEEDS_REMOVAL).
     * Updates buffer state flags as needed.
     * @param container The container whose state changed.
     * @param state The new processing state.
     */
    void on_container_state_change(std::shared_ptr<Kakshya::SignalSourceContainer> container,
        Kakshya::ProcessingState state);
};

/**
 * @class ContainerBuffer
 * @brief AudioBuffer implementation backed by a StreamContainer.
 *
 * ContainerBuffer provides a bridge between the digital-first container system and
 * the traditional AudioBuffer interface. It enables zero-copy or efficient extraction
 * of audio data from StreamContainers (such as SoundFileContainer) for use in
 * block-based DSP, node networks, and hardware output.
 *
 * Key responsibilities:
 * - Maintains a reference to the backing StreamContainer and source channel.
 * - Supports zero-copy operation when container memory layout matches buffer needs.
 * - Falls back to cached extraction when zero-copy is not possible.
 * - Integrates with ContainerToBufferAdapter for data extraction and state management.
 * - Can be initialized and reconfigured at runtime for flexible routing.
 *
 * While currently focused on audio, this pattern can be extended to other data types
 * as more container reader processors are implemented.
 *
 * @see ContainerToBufferAdapter, StreamContainer, SoundFileContainer
 */
class ContainerBuffer : public AudioBuffer {
public:
    /**
     * @brief Construct a ContainerBuffer for a specific channel and container.
     * @param channel_id Buffer channel index.
     * @param num_samples Number of samples in the buffer.
     * @param container Backing StreamContainer.
     * @param source_channel Channel index in the container (default: 0).
     */
    ContainerBuffer(u_int32_t channel_id,
        u_int32_t num_samples,
        std::shared_ptr<Kakshya::StreamContainer> container,
        u_int32_t source_channel = 0);

    /**
     * @brief Initialize the buffer after construction.
     * Must be called after the buffer is owned by a shared_ptr.
     */
    void initialize();

    /**
     * @brief Get the backing StreamContainer.
     */
    std::shared_ptr<Kakshya::StreamContainer> get_container() const { return m_container; }

    /**
     * @brief Get the source channel in the container.
     */
    u_int32_t get_source_channel() const { return m_source_channel; }

    /**
     * @brief Update the container reference.
     * @param container New StreamContainer to use.
     */
    void set_container(std::shared_ptr<Kakshya::StreamContainer> container);

    /**
     * @brief Check if buffer data is directly mapped to container (zero-copy).
     * @return True if zero-copy mode is active.
     */
    bool is_zero_copy() const { return m_zero_copy_mode; }

protected:
    /**
     * @brief Create the default processor (ContainerToBufferAdapter) for this buffer.
     * @return Shared pointer to the created processor.
     */
    std::shared_ptr<BufferProcessor> create_default_processor() override;

private:
    std::shared_ptr<Kakshya::StreamContainer> m_container;
    u_int32_t m_source_channel;
    std::shared_ptr<BufferProcessor> m_pending_adapter;
    bool m_zero_copy_mode = false;

    /**
     * @brief Attempt to enable zero-copy operation if container layout allows.
     */
    void setup_zero_copy_if_possible();
};

} // namespace MayaFlux::Buffers
