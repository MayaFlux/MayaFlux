#pragma once

#include "NDimensionalContainer.hpp"

namespace MayaFlux::Kakshya {

class DataProcessor;
class DataProcessingChain;

/**
 * @enum ProcessingState
 * @brief Represents the current processing lifecycle state of a container
 *
 * ProcessingState tracks a container's position in the data processing lifecycle,
 * enabling coordinated processing across components and optimizing resource usage.
 * This state-based approach allows the system to make intelligent decisions about
 * when to process data and how to handle dependencies between components.
 *
 * The state transitions typically follow this sequence:
 * 1. IDLE → READY (when data is loaded/prepared)
 * 2. READY → PROCESSING (when processing begins)
 * 3. PROCESSING → PROCESSED (when processing completes)
 * 4. PROCESSED → READY (when new processing is needed)
 * 5. Any state → NEEDS_REMOVAL (when container should be removed)
 *
 * Components can register for state change notifications to coordinate their
 * activities with the container's lifecycle, enabling efficient resource
 * management and processing optimization.
 */
enum class ProcessingState : u_int8_t {
    /**
     * Container is inactive with no data or not ready for processing.
     * Typically the initial state or when a container is reset.
     */
    IDLE,

    /**
     * Container has data loaded and is ready for processing.
     * Processing can begin when resources are available.
     */
    READY,

    /**
     * Container is actively being processed.
     * Other components should avoid modifying the data during this state.
     */
    PROCESSING,

    /**
     * Container has completed processing and results are available.
     * Data can be consumed by downstream components.
     */
    PROCESSED,

    /**
     * Container is marked for removal from the system.
     * Resources should be released and references cleared.
     */
    NEEDS_REMOVAL,

    /**
     * Container is in an error state and cannot proceed.
     * Typically requires external intervention to resolve.
     */
    ERROR
};

/**
 * @class SignalSourceContainer
 * @brief Data-driven interface for managing arbitrary processable signal sources.
 *
 * SignalSourceContainer provides a flexible, extensible abstraction for handling any data source
 * that can be interpreted and processed as an audio signal or multi-dimensional stream. Unlike
 * AudioBuffer, which is specialized for direct audio sample storage, this container is designed
 * for digital-first workflows and can manage:
 * - Audio files of any format or structure
 * - Network or streaming sources
 * - External buffers from other applications or devices
 * - Algorithmically generated or procedurally synthesized data
 * - Any data source larger than or structurally different from AudioBuffer
 *
 * The container maintains its own processing state and lifecycle, decoupled from the engine's
 * BufferManager, enabling asynchronous, scheduled, or on-demand processing. It acts as a bridge
 * between raw, heterogeneous data sources and the Maya Flux processing system, using DataProcessor
 * objects to transform and organize data into processable, channel-oriented forms.
 *
 * Key features:
 * - Explicit, observable processing state for robust orchestration and resource management
 * - Support for registering state change callbacks for event-driven workflows
 * - Pluggable processing chains and processors for custom or default data transformation
 * - Fine-grained reader/consumer tracking for safe, concurrent, and efficient access
 * - Designed for composability with digital-first nodes, routines, and buffer systems
 * - Enables data-driven, non-analog-centric development and integration of new data modalities
 *
 * This interface is foundational for advanced, data-driven workflows in Maya Flux, supporting
 * real-time streaming, offline analysis, hybrid computation, and seamless integration of
 * unconventional or future-facing signal sources.
 */
class SignalSourceContainer : public NDDataContainer, public std::enable_shared_from_this<SignalSourceContainer> {
public:
    ~SignalSourceContainer() override = default;

    /**
     * @brief Get the current processing state of the container.
     * @return Current ProcessingState (IDLE, READY, PROCESSING, etc.)
     *
     * Enables orchestration and coordination of processing across the system.
     */
    virtual ProcessingState get_processing_state() const = 0;

    /**
     * @brief Update the processing state of the container.
     * @param new_state New ProcessingState to set
     *
     * May trigger registered state change callbacks for event-driven workflows.
     */
    virtual void update_processing_state(ProcessingState new_state) = 0;

    /**
     * @brief Register a callback to be invoked on processing state changes.
     * @param callback Function to call when state changes (receives container and new state)
     *
     * Enables external components to react to lifecycle transitions for orchestration,
     * resource management, or UI updates.
     */
    virtual void register_state_change_callback(
        std::function<void(std::shared_ptr<SignalSourceContainer>, ProcessingState)> callback)
        = 0;

    /**
     * @brief Unregister the state change callback, if any.
     */
    virtual void unregister_state_change_callback() = 0;

    /**
     * @brief Check if the container is ready for processing.
     * @return true if ready to process, false otherwise
     *
     * Used for scheduling and dependency resolution in data-driven pipelines.
     */
    virtual bool is_ready_for_processing() const = 0;

    /**
     * @brief Mark the container as ready or not ready for processing.
     * @param ready true to mark as ready, false otherwise
     */
    virtual void mark_ready_for_processing(bool ready) = 0;

    /**
     * @brief Create and configure a default processor for this container.
     *
     * Instantiates a standard DataProcessor to handle basic processing needs,
     * such as channel organization or format conversion. Called during initialization
     * if no custom processor is provided.
     */
    virtual void create_default_processor() = 0;

    /**
     * @brief Process the container's data using the default processor.
     *
     * Executes the default processing chain, transforming raw data into a
     * processable form. This is a convenience wrapper for standard workflows.
     */
    virtual void process_default() = 0;

    /**
     * @brief Set the default data processor for this container.
     * @param processor Shared pointer to the DataProcessor to use
     */
    virtual void set_default_processor(std::shared_ptr<DataProcessor> processor) = 0;

    /**
     * @brief Get the current default data processor.
     * @return Shared pointer to the current DataProcessor, or nullptr if none
     */
    virtual std::shared_ptr<DataProcessor> get_default_processor() const = 0;

    /**
     * @brief Get the current processing chain for this container.
     * @return Shared pointer to the DataProcessingChain, or nullptr if none
     */
    virtual std::shared_ptr<DataProcessingChain> get_processing_chain() = 0;

    /**
     * @brief Set the processing chain for this container.
     * @param chain Shared pointer to the DataProcessingChain to use
     */
    virtual void set_processing_chain(std::shared_ptr<DataProcessingChain> chain) = 0;

    /**
     * @brief Register a reader for a specific dimension.
     * @param dimension_index Index of the dimension being read
     * @return Reader ID for the registered dimension
     *
     * Used for tracking active readers in multi-threaded or streaming scenarios,
     * enabling safe concurrent access and efficient resource management.
     */
    virtual u_int32_t register_dimension_reader(u_int32_t dimension_index) = 0;

    /**
     * @brief Unregister a reader for a specific dimension.
     * @param dimension_index Index of the dimension no longer being read
     */
    virtual void unregister_dimension_reader(u_int32_t dimension_index) = 0;

    /**
     * @brief Check if any dimensions currently have active readers.
     * @return true if any dimension is being read, false otherwise
     */
    virtual bool has_active_readers() const = 0;

    /**
     * @brief Mark a dimension as consumed for the current processing cycle.
     * @param dimension_index Index of the dimension that was processed
     * @param reader_id Reader ID for the dimension
     */
    virtual void mark_dimension_consumed(u_int32_t dimension_index, u_int32_t reader_id) = 0;

    /**
     * @brief Check if all active dimensions have been consumed in this cycle.
     * @return true if all active dimensions have been processed
     */
    virtual bool all_dimensions_consumed() const = 0;

    // ===== Processed Data Access =====

    /**
     * @brief Get a mutable reference to the processed data buffer.
     * @return Reference to the processed DataVariant vector
     *
     * The structure and type of this data is implementation-specific and may
     * depend on the processing chain or data source.
     */
    virtual std::vector<DataVariant>& get_processed_data() = 0;

    /**
     * @brief Get a const reference to the processed data buffer.
     * @return Const reference to the processed DataVariant
     */
    virtual const std::vector<DataVariant>& get_processed_data() const = 0;

    /** @brief Get a reference to the raw data stored in the container.
     * @return Const reference to the vector of DataVariant representing raw data
     *
     * This provides access to the unprocessed, original data source managed by the container.
     */
    virtual const std::vector<DataVariant>& get_data() = 0;

    // ===== Buffer Integration =====

    /**
     * @brief Mark associated buffers for processing in the next cycle.
     * @param should_process true to enable processing, false to disable
     *
     * Used to coordinate buffer state with the container's processing lifecycle,
     * ensuring that buffers are processed only when needed in data-driven flows.
     */
    virtual void mark_buffers_for_processing(bool should_process) = 0;

    /**
     * @brief Mark associated buffers for removal from the system.
     *
     * Signals that buffers should be released and references cleared, supporting
     * efficient resource management in dynamic, digital-first workflows.
     */
    virtual void mark_buffers_for_removal() = 0;
};

}
