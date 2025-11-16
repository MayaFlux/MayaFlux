#pragma once

namespace MayaFlux::Buffers {

class BufferProcessor;
class BufferProcessingChain;

/**
 * @class Buffer
 * @brief Backend-agnostic interface for sequential data storage and transformation
 *
 * Buffer provides a unified interface for all buffer types in the MayaFlux engine,
 * supporting multiple data types and processing backends. Buffers store sequential
 * data samples and provide mechanisms for transforming that data through attached
 * processors. Unlike nodes which operate on individual values, buffers process
 * blocks of data, enabling efficient batch operations and transformations.
 *
 * The Buffer interface is designed to be backend-agnostic and data-type agnostic,
 * allowing for concrete implementations like AudioBuffer, VideoBuffer, TextureBuffer,
 * and other specialized buffer types. This mirrors the processing token architecture
 * used in the node system, where different backends handle different types of data
 * processing.
 *
 * Buffers can:
 * - Store and provide access to sequential data of various types (audio, video, texture, etc.)
 * - Be transformed by one or more BufferProcessor objects
 * - Be arranged in processing networks via BufferProcessingChain
 * - Bridge between continuous (node) and discrete (buffer) computational domains
 * - Support different processing backends through concrete implementations
 *
 * The buffer system complements the node system by providing block-based processing
 * capabilities, which are more efficient for certain operations and essential for
 * interfacing with hardware and external systems that operate on data blocks.
 * Different buffer types can be managed centrally while maintaining type-specific
 * processing capabilities through their concrete implementations.
 */
class MAYAFLUX_API Buffer : public std::enable_shared_from_this<Buffer> {
public:
    /**
     * @brief Virtual destructor for proper resource management
     *
     * Ensures proper cleanup of resources in derived buffer implementations
     * across different data types and processing backends.
     */
    virtual ~Buffer() = default;

    /**
     * @brief Resets all data values in the buffer
     *
     * Initializes all data elements to their default/zero state without
     * changing the buffer capacity. The specific behavior depends on the
     * concrete buffer implementation and data type.
     */
    virtual void clear() = 0;

    /**
     * @brief Applies the default transformation to the buffer's data
     *
     * Executes the default processing algorithm on the buffer's data.
     * The specific transformation depends on the buffer type, data format,
     * and its configured default processor. This enables backend-specific
     * default processing while maintaining a unified interface.
     */
    virtual void process_default() = 0;

    /**
     * @brief Sets the default transformation processor for this buffer
     * @param processor Processor to use as default transformation
     *
     * The default processor is used when process_default() is called.
     * Different buffer types may accept different processor types depending
     * on their data format and processing requirements.
     */
    virtual void set_default_processor(std::shared_ptr<BufferProcessor> processor) = 0;

    /**
     * @brief Gets the current default transformation processor
     * @return Shared pointer to the default processor, or nullptr if none set
     *
     * Returns the processor that will be used for default transformations.
     * The specific processor type depends on the concrete buffer implementation.
     */
    virtual std::shared_ptr<BufferProcessor> get_default_processor() const = 0;

    /**
     * @brief Gets the transformation chain attached to this buffer
     * @return Shared pointer to the buffer processing chain
     *
     * The processing chain contains multiple transformations that are
     * applied in sequence when the buffer is processed. Chain composition
     * may vary based on the buffer type and backend capabilities.
     */
    virtual std::shared_ptr<BufferProcessingChain> get_processing_chain() = 0;

    /**
     * @brief Sets the transformation chain for this buffer
     * @param chain New processing chain for sequential transformations
     * @param force If true, forces the replacement of the current chain even if incompatible
     *
     * Replaces the current processing chain with the provided one if force is true.
     * else merges the new chain only if compatible.
     * The chain should be compatible with the buffer's data type and
     * processing backend.
     */
    virtual void set_processing_chain(std::shared_ptr<BufferProcessingChain> chain, bool force = false) = 0;

    /**
     * @brief Checks if the buffer has data for the current processing cycle
     * @return True if the buffer has data available for processing, false otherwise
     *
     * This method is particularly relevant for dynamic buffer management systems
     * like SignalSourceContainers, where buffers may not always contain valid data.
     * The interpretation and behavior may vary based on the buffer type:
     * - Audio buffers typically return true unless specifically marked otherwise
     * - Video buffers may return false if no frame is available
     * - Texture buffers may return false if textures are not loaded
     */
    virtual bool has_data_for_cycle() const = 0;

    /**
     * @brief Checks if the buffer should be removed from processing chains
     * @return True if the buffer should be removed, false otherwise
     *
     * This method enables dynamic buffer lifecycle management. Different buffer
     * types may have different removal criteria:
     * - Audio buffers typically return false unless explicitly marked for removal
     * - Video buffers may return true when a stream ends
     * - Texture buffers may return true when resources are freed
     */
    virtual bool needs_removal() const = 0;

    /**
     * @brief Marks the buffer's data availability for the current processing cycle
     * @param has_data True if the buffer has valid data to process, false otherwise
     *
     * This method allows external systems to control whether the buffer should
     * be considered for processing in the current cycle. Behavior varies by buffer type:
     * - Audio buffers are typically always marked as having data
     * - Video buffers may be marked based on frame availability
     * - Texture buffers may be marked based on resource loading state
     */
    virtual void mark_for_processing(bool has_data) = 0;

    /**
     * @brief Marks the buffer for removal from processing chains
     *
     * Sets the buffer's removal flag, indicating it should be removed from
     * any processing chains or management systems. The specific removal behavior
     * depends on the buffer type and the systems managing it:
     * - Audio buffers rarely need removal unless explicitly requested
     * - Video buffers may be removed when streams end or switch
     * - Texture buffers may be removed when resources are deallocated
     */
    virtual void mark_for_removal() = 0;

    /**
     * @brief Controls whether the buffer should use default processing
     * @param should_process True if default processing should be applied, false otherwise
     *
     * This method allows fine-grained control over when the buffer's default
     * processor is applied. Different buffer types may have different default
     * processing requirements:
     * - Audio buffers typically always use default processing
     * - Video buffers may skip processing for certain frame types
     * - Texture buffers may skip processing when not actively displayed
     */
    virtual void enforce_default_processing(bool should_process) = 0;

    /**
     * @brief Checks if the buffer should undergo default processing
     * @return True if default processing should be applied, false otherwise
     *
     * Determines whether the buffer's default processor should be executed
     * during the current processing cycle. The decision criteria may vary
     * based on buffer type and current state:
     * - Audio buffers typically always need default processing
     * - Video buffers may skip processing for duplicate frames
     * - Texture buffers may skip processing when not visible
     */
    virtual bool needs_default_processing() = 0;

    /**
     * @brief Attempts to acquire processing rights for the buffer
     * @return True if processing rights were successfully acquired, false otherwise
     *
     * This method is used to control access to the buffer's data during processing.
     * It allows the buffer to manage concurrent access and ensure that only one
     * processing operation occurs at a time. The specific implementation may vary
     * based on the buffer type and its processing backend.
     */
    bool virtual try_acquire_processing() = 0;

    /**
     * @brief Releases processing rights for the buffer
     *
     * This method is called to release the processing rights acquired by
     * try_acquire_processing(). It allows other processing operations to
     * access the buffer's data once the current operation is complete.
     * The specific implementation may vary based on the buffer type and
     * its processing backend.
     */
    virtual void release_processing() = 0;

    /**
     * @brief Checks if the buffer is currently being processed
     * @return True if the buffer is in a processing state, false otherwise
     *
     * This method indicates whether the buffer is currently undergoing
     * a processing operation. It is used to manage concurrent access and
     * ensure that processing operations do not interfere with each other.
     * The specific implementation may vary based on the buffer type and
     * its processing backend.
     */
    virtual bool is_processing() const = 0;

    /**
     * @brief Creates a clone of this buffer for a specific channel or usage enum
     * @param dest_desc Destination channel identifier or usage descriptor for the cloned buffer
     * @return Shared pointer to the cloned buffer
     *
     * This method creates a new instance of the buffer with the same
     * data and properties, but assigned to a different channel. The
     * cloned buffer can be used independently in processing chains.
     *
     * NOTE: The moment of cloning is the divergence point between the original
     * and the cloned. While they both will follow the same processing chain or have the same
     * default procesor, any changes made to one buffer after cloning will not affect the other.
     */
    virtual std::shared_ptr<Buffer> clone_to(uint8_t dest_desc) = 0;

    /**
     * @brief Marks the buffer as internal-only, preventing root aggregation
     * @param internal True to mark as internal-only, false to allow root aggregation
     *
     * Internal-only buffers are excluded from root-level aggregation and
     * processing. This is typically used for buffers that are processed
     * entirely within a specific backend or domain (e.g., GPU-only buffers).
     */
    virtual void mark_internal_only(bool internal) = 0;

    /**
     * @brief Checks if the buffer is marked as internal-only
     * @return True if the buffer is internal-only, false otherwise
     *
     * Indicates whether the buffer is excluded from root-level aggregation
     * and processing. Internal-only buffers are typically processed entirely
     * within a specific backend or domain.
     */
    virtual bool is_internal_only() const = 0;
};

}
