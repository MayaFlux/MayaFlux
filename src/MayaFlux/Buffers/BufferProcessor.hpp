#pragma once

#include "config.h"

namespace MayaFlux::Buffers {

class AudioBuffer;

/**
 * @class BufferProcessor
 * @brief Interface for computational transformations on data buffers
 *
 * BufferProcessor defines the interface for components that transform
 * data stored in buffer objects. This design follows the Strategy
 * pattern, allowing different algorithmic transformations to be applied
 * to buffers without changing the underlying buffer implementation.
 *
 * This abstraction enables a wide range of computational processes:
 * - Mathematical transformations (FFT, convolution, statistical analysis)
 * - Signal processing algorithms (filtering, modulation, synthesis)
 * - Data manipulation (normalization, scaling, interpolation)
 * - Cross-domain mappings and transformations
 *
 * The processor system provides a flexible framework for block-based
 * computational operations, complementing the sample-by-sample processing
 * of the node system and enabling efficient parallel transformations.
 */
class BufferProcessor {
public:
    /**
     * @brief Virtual destructor for proper cleanup of derived classes
     */
    virtual ~BufferProcessor() = default;

    /**
     * @brief Applies a computational transformation to the data in the provided buffer
     * @param buffer Buffer to transform
     *
     * This is the main transformation method that must be implemented by all
     * concrete processor classes. It applies the processor's algorithm
     * to the data in the buffer, potentially modifying it in-place or
     * generating derived information.
     */
    virtual void process(std::shared_ptr<AudioBuffer> buffer) = 0;

    /**
     * @brief Called when this processor is attached to a buffer
     * @param buffer Buffer this processor is being attached to
     *
     * Provides an opportunity for the processor to initialize any
     * buffer-specific state, allocate resources, or perform validation.
     * Default implementation does nothing.
     */
    virtual void on_attach(std::shared_ptr<AudioBuffer> buffer) {};

    /**
     * @brief Called when this processor is detached from a buffer
     * @param buffer Buffer this processor is being detached from
     *
     * Provides an opportunity for the processor to clean up any
     * buffer-specific state or release resources.
     * Default implementation does nothing.
     */
    virtual void on_detach(std::shared_ptr<AudioBuffer> buffer) {};
};

/**
 * @class BufferProcessingChain
 * @brief Manages computational transformation pipelines for data buffers
 *
 * BufferProcessingChain organizes multiple BufferProcessor objects into
 * sequential transformation pipelines for one or more buffers. This allows
 * complex multi-stage computational processes to be applied to data in a
 * controlled, deterministic order.
 *
 * The chain implements a directed acyclic graph (DAG) of transformations,
 * maintaining separate processor sequences for each buffer while allowing
 * processors to be shared across multiple buffers when appropriate.
 *
 * Key features:
 * - Enables construction of complex computational pipelines
 * - Supports both parallel and sequential transformation patterns
 * - Preserves transformation order based on the sequence processors are added
 * - Provides special "final" processors for post-processing operations
 * - Allows dynamic reconfiguration of transformation pipelines
 */
class BufferProcessingChain {
public:
    /**
     * @brief Adds a processor to the transformation pipeline for a specific buffer
     * @param processor Processor to add
     * @param buffer Buffer to associate with this processor
     *
     * The processor is added to the end of the transformation sequence
     * for the specified buffer. If this is the first processor for
     * the buffer, a new sequence is created.
     */
    void add_processor(std::shared_ptr<BufferProcessor> processor, std::shared_ptr<AudioBuffer> buffer);

    /**
     * @brief Removes a processor from the pipeline for a specific buffer
     * @param processor Processor to remove
     * @param buffer Buffer to remove the processor from
     *
     * If the processor is found in the buffer's transformation sequence,
     * it is removed and its on_detach method is called.
     */
    void remove_processor(std::shared_ptr<BufferProcessor> processor, std::shared_ptr<AudioBuffer> buffer);

    /**
     * @brief Applies the transformation pipeline to a buffer
     * @param buffer Buffer to transform
     *
     * Applies each processor in the buffer's sequence in order.
     * This does not include the final processor, which must be
     * applied separately with process_final().
     */
    void process(std::shared_ptr<AudioBuffer> buffer);

    /**
     * @brief Sets a special processor to be applied after the main pipeline
     * @param processor Final processor to add
     * @param buffer Buffer to associate with this final processor
     *
     * The final processor is applied after all regular processors
     * when process_final() is called. This is useful for operations
     * like normalization, boundary enforcement, or validation that
     * should be applied as the last step in a transformation pipeline.
     */
    void add_final_processor(std::shared_ptr<BufferProcessor> processor, std::shared_ptr<AudioBuffer> buffer);

    /**
     * @brief Checks if a buffer has any processors in its pipeline
     * @param buffer Buffer to check
     * @return true if the buffer has processors, false otherwise
     */
    bool has_processors(std::shared_ptr<AudioBuffer> buffer) const;

    /**
     * @brief Gets all processors in a buffer's transformation pipeline
     * @param buffer Buffer to get processors for
     * @return Constant reference to the vector of processors
     *
     * Returns an empty vector if the buffer has no processors.
     */
    const std::vector<std::shared_ptr<BufferProcessor>>& get_processors(std::shared_ptr<AudioBuffer> buffer) const;

    /**
     * @brief Gets the entire transformation pipeline structure
     * @return Map of buffers to their processor sequences
     *
     * This provides access to the internal structure of the pipeline,
     * mapping each buffer to its sequence of transformation processors.
     */
    inline const std::unordered_map<std::shared_ptr<AudioBuffer>, std::vector<std::shared_ptr<BufferProcessor>>> get_chain() const { return m_buffer_processors; }

    /**
     * @brief Combines another processing pipeline into this one
     * @param other Chain to merge into this one
     *
     * Adds all processors from the other chain to this one,
     * preserving their buffer associations and order. This enables
     * the composition of complex transformation pipelines from
     * simpler, reusable components.
     */
    void merge_chain(const std::shared_ptr<BufferProcessingChain> other);

    /**
     * @brief Applies the final processor to a buffer
     * @param buffer Buffer to process
     *
     * If the buffer has a final processor, it is applied.
     * This is typically called after process() to apply
     * final-stage transformations like normalization or
     * boundary enforcement.
     */
    void process_final(std::shared_ptr<AudioBuffer> buffer);

private:
    /**
     * @brief Map of buffers to their processor sequences
     *
     * Each buffer has its own vector of processors that are
     * applied in order when the buffer is processed.
     */
    std::unordered_map<std::shared_ptr<AudioBuffer>, std::vector<std::shared_ptr<BufferProcessor>>> m_buffer_processors;

    /**
     * @brief Map of buffers to their final processors
     *
     * Each buffer can have one final processor that is
     * applied after the main processing sequence.
     */
    std::unordered_map<std::shared_ptr<AudioBuffer>, std::shared_ptr<BufferProcessor>> m_final_processors;
};

}
