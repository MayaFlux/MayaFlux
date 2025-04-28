#pragma once

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Buffers/BufferProcessor.hpp"

namespace MayaFlux::Containers {
class SignalSourceContainer;
enum class ProcessingState;
}

namespace MayaFlux::Buffers {
/**
 * @brief Adapter to connect SignalSourceContainer to AudioBuffer system
 *
 * This processor bridges the gap between containers and buffers by
 * handling the transfer of data and state between the two systems.
 *
 * ContainerToBufferAdapter serves as the primary integration point between the
 * container-based data management system and the buffer-based audio processing system.
 * It translates between these two paradigms, allowing containers to participate in
 * the real-time audio processing workflow while maintaining their independence.
 *
 * Key responsibilities:
 * - Transferring data from containers to buffers at processing time
 * - Synchronizing processing states between systems
 * - Managing lifecycle events for both container and buffer
 * - Optimizing processing by tracking state changes
 * - Providing channel mapping between container and buffer domains
 *
 * This adapter implements an intelligent processing strategy that only transfers
 * data when the container has been updated, avoiding redundant operations and
 * maintaining processing efficiency in the real-time audio path.
 */
class ContainerToBufferAdapter : public Buffers::BufferProcessor {
public:
    /**
     * @brief Creates a new adapter connected to the specified container
     * @param container The SignalSourceContainer to adapt to the buffer system
     *
     * Initializes the adapter with default settings:
     * - Source channel: 0 (first channel)
     * - Auto advance: false (container position not automatically updated)
     * - Update flags: true (buffer processing flags updated after transfer)
     */
    ContainerToBufferAdapter(std::shared_ptr<Containers::SignalSourceContainer> container);

    /**
     * @brief Transfers data from container to buffer during processing
     * @param buffer The destination AudioBuffer to receive container data
     *
     * This method is called during the buffer processing cycle and performs
     * the actual data transfer from container to buffer. It checks the container's
     * processing state to avoid redundant transfers and only updates the buffer
     * when the container has new data available.
     */
    void process(std::shared_ptr<Buffers::AudioBuffer> buffer) override;

    /**
     * @brief Initializes the connection when attached to a buffer
     * @param buffer The AudioBuffer this adapter is being attached to
     *
     * When attached to a buffer, this method:
     * - Validates the container's readiness
     * - Performs initial data transfer
     * - Sets up state tracking
     * - Configures buffer processing flags
     *
     * This ensures the buffer immediately has valid data from the container
     * and is properly configured for subsequent processing cycles.
     */
    void on_attach(std::shared_ptr<Buffers::AudioBuffer> buffer) override;

    /**
     * @brief Cleans up the connection when detached from a buffer
     * @param buffer The AudioBuffer this adapter is being detached from
     *
     * Performs cleanup operations when the adapter is removed from a buffer,
     * ensuring proper resource management and state cleanup.
     */
    void on_detach(std::shared_ptr<Buffers::AudioBuffer> buffer) override;

    /**
     * @brief Sets which channel from the container to use as source
     * @param channel Channel index in the container
     *
     * For multi-channel containers, this specifies which channel's data
     * should be transferred to the buffer. Defaults to channel 0.
     */
    void set_source_channel(u_int32_t channel);

    /**
     * @brief Gets the currently selected source channel
     * @return Current source channel index
     */
    u_int32_t get_source_channel() const;

    /**
     * @brief Changes the container this adapter is connected to
     * @param container New container to use as data source
     *
     * Allows dynamically switching the data source without recreating
     * the adapter or disrupting the buffer processing chain.
     */
    void set_container(std::shared_ptr<Containers::SignalSourceContainer> container);

    /**
     * @brief Gets the currently connected container
     * @return Current container used as data source
     */
    std::shared_ptr<Containers::SignalSourceContainer> get_container() const;

    /**
     * @brief Controls whether the container's position advances automatically
     * @param enable True to enable auto-advancement, false to disable
     *
     * When enabled, the container's read position is automatically advanced
     * after each data transfer, simulating continuous playback. When disabled,
     * the position must be managed externally.
     */
    void set_auto_advance(bool enable);

    /**
     * @brief Checks if auto-advancement is enabled
     * @return True if enabled, false if disabled
     */
    bool get_auto_advance() const;

    /**
     * @brief Controls whether buffer processing flags are updated after transfer
     * @param update True to update flags, false to leave unchanged
     *
     * When enabled, the buffer's processing flags are updated after data transfer
     * to indicate new data is available. This ensures downstream processors are
     * triggered appropriately.
     */
    void set_update_flags(bool update);

    /**
     * @brief Checks if flag updating is enabled
     * @return True if enabled, false if disabled
     */
    bool get_update_flags() const;

private:
    /**
     * @brief Container providing the source data
     */
    std::shared_ptr<Containers::SignalSourceContainer> m_container;

    /**
     * @brief Channel index to read from the container
     */
    u_int32_t m_source_channel;

    /**
     * @brief Flag controlling automatic position advancement
     */
    bool m_auto_advance;

    /**
     * @brief Flag controlling buffer processing flag updates
     */
    bool m_update_flags;

    /**
     * @brief Handles container state changes
     * @param container Container whose state has changed
     * @param state New processing state
     *
     * This callback is triggered when the container's processing state changes,
     * allowing the adapter to respond appropriately (e.g., marking the buffer
     * for processing when new data is available).
     */
    void on_container_state_change(std::shared_ptr<Containers::SignalSourceContainer> container,
        Containers::ProcessingState state);
};

/**
 * @class ContainerBuffer
 * @brief Specialized AudioBuffer that directly integrates with a SignalSourceContainer
 *
 * ContainerBuffer provides a convenient wrapper that combines a standard audio buffer
 * with a ContainerToBufferAdapter, creating a seamless integration point between
 * the container and buffer processing systems.
 *
 * This buffer type serves as the default and recommended method for bringing container
 * data into the buffer processing workflow, offering a simplified interface that
 * handles the complexities of adapter configuration and lifecycle management.
 *
 * Unlike standard buffers that generate or process data directly, ContainerBuffer
 * delegates data sourcing to its connected container, acting as a view or window
 * into the container's data that participates in the buffer processing system.
 */
class ContainerBuffer : public StandardAudioBuffer {
public:
    /**
     * @brief Creates a new buffer connected to a container
     * @param channel_id Channel ID in the buffer system
     * @param num_samples Number of samples this buffer can hold
     * @param container Container to use as data source
     * @param source_channel Channel index to read from the container
     *
     * Initializes a buffer that automatically pulls data from the specified
     * container during processing cycles. The buffer's default processor is
     * configured as a ContainerToBufferAdapter connected to the container.
     */
    ContainerBuffer(u_int32_t channel_id, u_int32_t num_samples,
        std::shared_ptr<Containers::SignalSourceContainer> container,
        u_int32_t source_channel = 0);

    /**
     * @brief Gets the container this buffer is connected to
     * @return Container used as data source
     */
    inline std::shared_ptr<Containers::SignalSourceContainer> get_container() const
    {
        return m_container;
    }

    /**
     * @brief Gets the channel index being read from the container
     * @return Source channel index
     */
    inline u_int32_t get_source_channel() const
    {
        return m_source_channel;
    }

    /**
     * @brief Initilaize default processor
     * Workaround to let shared_from_this() evaluate after constructor
     */
    void initialize();

protected:
    /**
     * @brief Container providing the source data
     */
    std::shared_ptr<Containers::SignalSourceContainer> m_container;

    /**
     * @brief Channel index to read from the container
     */
    u_int32_t m_source_channel;

    /**
     * @brief Default processor (ContainerToBufferAdapter) for this buffer.
     * Marked pending to allow post constructor initialization
     */
    std::shared_ptr<BufferProcessor> m_pending_adapter;
};

}
