#pragma once

namespace MayaFlux::Kakshya {

class SignalSourceContainer;

/**
 * @class DataProcessor
 * @brief Interface for processing data within SignalSourceContainer objects
 *
 * DataProcessor defines the interface for components that transform data stored in
 * SignalSourceContainer objects. While conceptually similar to BufferProcessor in the
 * audio buffer domain, DataProcessor operates with greater independence and flexibility,
 * designed specifically for on-demand processing of arbitrary data sources.
 *
 * Key differences from BufferProcessor:
 * - Independence from engine cycle: DataProcessors can be invoked on demand rather than
 *   being tied to the audio engine's processing cycle
 * - Broader data scope: Operates on arbitrary data sources beyond audio buffers
 * - Lifecycle management: Includes explicit attach/detach methods for resource management
 * - State tracking: Maintains processing state for asynchronous operations
 * - Self-contained: Can operate without dependency on BufferManager or other engine components
 *
 * DataProcessors enable flexible data transformation workflows that can operate independently
 * of the real-time audio processing path, making them ideal for tasks like:
 * - File loading and format conversion
 * - Offline processing of large datasets
 * - Background analysis and feature extraction
 * - Scheduled or event-driven processing
 * - Integration with external data sources and sinks
 */
class MAYAFLUX_API DataProcessor {
public:
    /**
     * @brief Virtual destructor for proper cleanup
     */
    virtual ~DataProcessor() = default;

    /**
     * @brief Called when this processor is attached to a container
     * @param container Container this processor is being attached to
     *
     * This method provides an opportunity for the processor to:
     * - Initialize container-specific state
     * - Allocate resources needed for processing
     * - Validate the container's data structure
     * - Configure processing parameters based on container properties
     * - Store references or metadata needed for processing
     *
     * Unlike BufferProcessor which may be attached implicitly through chains,
     * DataProcessor attachment is typically an explicit operation that triggers
     * immediate initialization.
     */
    virtual void on_attach(std::shared_ptr<SignalSourceContainer> container) = 0;

    /**
     * @brief Called when this processor is detached from a container
     * @param container Container this processor is being detached from
     *
     * This method allows the processor to:
     * - Release container-specific resources
     * - Finalize any pending operations
     * - Clean up state information
     * - Perform any necessary cleanup before the processor is removed
     *
     * Explicit detachment provides cleaner resource management compared to
     * BufferProcessor's implicit detachment through chain removal.
     */
    virtual void on_detach(std::shared_ptr<SignalSourceContainer> container) = 0;

    /**
     * @brief Processes the data in the container
     * @param container Container whose data should be processed
     *
     * This is the main processing method that transforms the container's data.
     * Unlike BufferProcessor which is typically invoked automatically during
     * the engine's processing cycle, DataProcessor's process method is called
     * explicitly when processing is needed, enabling on-demand operation.
     *
     * This method may:
     * - Transform data in place within the container
     * - Generate new data based on the container's content
     * - Analyze the container's data and store results
     * - Update the container's metadata or state
     */
    virtual void process(std::shared_ptr<SignalSourceContainer> container) = 0;

    /**
     * @brief Checks if the processor is currently performing processing
     * @return true if processing is in progress, false otherwise
     *
     * This state tracking enables asynchronous processing models where
     * a processor might operate in a background thread or over multiple
     * invocations. It allows clients to check if processing has completed
     * without blocking.
     *
     * BufferProcessors lack this state tracking as they operate synchronously
     * within the engine's processing cycle.
     */
    virtual bool is_processing() const = 0;
};

}
