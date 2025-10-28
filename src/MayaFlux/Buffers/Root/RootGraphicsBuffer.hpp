// RootGraphicsBuffer.hpp
#pragma once

#include "MayaFlux/Buffers/BufferProcessor.hpp"
#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "RootBuffer.hpp"

namespace MayaFlux::Buffers {

class BufferProcessingChain;

/**
 * @class RootGraphicsBuffer
 * @brief Root container for GPU buffer lifecycle management and batch processing
 *
 * RootGraphicsBuffer serves as the organizational hub for graphics buffers in a processing
 * domain. Unlike RootAudioBuffer which accumulates and mixes sample data, RootGraphicsBuffer
 * focuses on:
 * - Managing the lifecycle of VKBuffer instances (GPU resources)
 * - Coordinating batch processing across multiple GPU buffers
 * - Tracking active GPU resources for backend queries
 * - Providing cleanup mechanisms for marked buffers
 *
 * Key Differences from RootAudioBuffer:
 * - No data accumulation or mixing (each buffer is independent)
 * - No "final output" concept (buffers are consumed by shaders/render passes)
 * - Focuses on resource management and batch coordination
 * - Processing means executing BufferProcessor chains (uploads, compute, etc.)
 *
 * Token Compatibility:
 * - Primary Token: GRAPHICS_BACKEND (frame-rate, GPU, parallel)
 * - Compatible with GPU_PROCESS tokens for compute operations
 * - Not compatible with AUDIO_BACKEND tokens (different processing model)
 */

class MAYAFLUX_API RootGraphicsBuffer : public RootBuffer<VKBuffer> {
public:
    /**
     * @brief Creates a new root graphics buffer
     *
     * Initializes with GRAPHICS_BACKEND token preference and prepares
     * for managing GPU buffer resources. No Vulkan resources are created
     * until child buffers are registered and initialized by the backend.
     */
    RootGraphicsBuffer();

    /**
     * @brief Virtual destructor ensuring proper cleanup
     *
     * Cleans up all child buffer references and ensures pending removals
     * are processed. Actual GPU resource cleanup is handled by the backend
     * through registered cleanup hooks.
     */
    ~RootGraphicsBuffer() override;

    /**
     * @brief Initializes the root buffer with default processor
     *
     * For graphics, this sets up a GraphicsBatchProcessor as the default
     * processor which handles coordinating child buffer processing.
     */
    void initialize();

    /**
     * @brief Processes all child buffers in batch
     * @param processing_units Number of processing units (typically 1 for graphics frames)
     *
     * Executes the processing chain for each active child buffer:
     * 1. Cleans up buffers marked for removal
     * 2. Processes each buffer's default processor (if enabled)
     * 3. Executes each buffer's processing chain (upload, compute, etc.)
     * 4. Handles any errors that occur during processing
     *
     * This is the core processing loop for GPU buffers, coordinating operations
     * like staging buffer uploads, compute shader dispatches, and resource transitions.
     */
    void process_children(uint32_t processing_units = 1);

    /**
     * @brief Processes this root buffer using default processing
     *
     * For graphics root buffers, this:
     * 1. Processes all child buffers through process_children()
     * 2. Executes final processor if one is set (typically for debug/profiling)
     * 3. Handles pending buffer operations
     */
    void process_default() override;

    /**
     * @brief Gets all child buffers managed by this root
     * @return Constant reference to vector of child buffers
     */
    inline const std::vector<std::shared_ptr<VKBuffer>>& get_child_buffers() const
    {
        return m_child_buffers;
    }

    /**
     * @brief Sets an optional final processor
     * @param processor Processor to execute after all child buffers are processed
     *
     * Unlike audio where final processing is critical (limiting, normalization),
     * graphics rarely needs final processing. This can be used for:
     * - Debug visualization passes
     * - Profiling/timing measurements
     * - Resource synchronization barriers
     */
    void set_final_processor(std::shared_ptr<BufferProcessor> processor);

    /**
     * @brief Gets the current final processor
     * @return Shared pointer to final processor or nullptr
     */
    std::shared_ptr<BufferProcessor> get_final_processor() const;

    /**
     * @brief Gets buffers filtered by usage type
     * @param usage VKBuffer::Usage type to filter by
     * @return Vector of buffers matching the specified usage
     *
     * Useful for backend queries like "get all compute buffers" or
     * "find all staging buffers that need upload".
     */
    std::vector<std::shared_ptr<VKBuffer>> get_buffers_by_usage(VKBuffer::Usage usage) const;

    /**
     * @brief Removes buffers marked for deletion
     *
     * Performs the actual removal of buffers that have been marked with
     * mark_for_removal(). This is called automatically during process_children()
     * but can be called manually for immediate cleanup.
     *
     * The actual GPU resource cleanup is handled by the backend's cleanup hooks.
     */
    void cleanup_marked_buffers();

    /**
     * @brief Gets the number of child buffers
     * @return Number of buffers currently managed
     */
    size_t get_buffer_count() const { return m_child_buffers.size(); }

    /**
     * @brief Checks if a specific buffer is registered
     * @param buffer Buffer to check
     * @return True if buffer is managed by this root, false otherwise
     */
    bool has_buffer(const std::shared_ptr<VKBuffer>& buffer) const;

    /**
     * @brief Activates/deactivates processing for the current token
     * @param active Whether this buffer should process when its token is active
     */
    inline void set_token_active(bool active) override { m_token_active = active; }

    /**
     * @brief Checks if the buffer is active for its assigned token
     * @return True if the buffer will process when its token is processed
     */
    inline bool is_token_active() const override { return m_token_active; }

private:
    /**
     * @brief Processes a single buffer with error handling
     * @param buffer Buffer to process
     * @param processing_units Number of processing units
     *
     * Internal helper that executes:
     * 1. Buffer's default processor
     * 2. Buffer's processing chain
     * With proper error handling and logging.
     */
    void process_buffer(const std::shared_ptr<VKBuffer>& buffer, uint32_t processing_units);

    /**
     * @brief Creates the default graphics batch processor
     * @return Shared pointer to new GraphicsBatchProcessor
     */
    std::shared_ptr<BufferProcessor> create_default_processor();

    /**
     * @brief Optional final processor (rarely used in graphics)
     */
    std::shared_ptr<BufferProcessor> m_final_processor;

    /**
     * @brief Buffers pending removal (cleaned up in next process cycle)
     */
    std::vector<std::shared_ptr<VKBuffer>> m_pending_removal;

    /**
     * @brief Flag indicating if this buffer is active for token processing
     */
    bool m_token_active {};
};

/**
 * @class GraphicsBatchProcessor
 * @brief Default processor for coordinating batch GPU buffer processing
 *
 * GraphicsBatchProcessor manages the execution of processing chains across
 * multiple GPU buffers in a coordinated manner. Unlike ChannelProcessor which
 * accumulates audio data, GraphicsBatchProcessor focuses on:
 * - Coordinating buffer uploads (CPU -> GPU)
 * - Dispatching compute shader operations
 * - Managing resource transitions and barriers
 * - Ensuring proper synchronization between operations
 *
 * Token Compatibility:
 * - Primary Token: GRAPHICS_BACKEND (frame-rate, GPU, parallel processing)
 * - Compatible with GPU_PROCESS tokens for compute operations
 * - Handles parallel batch operations on GPU resources
 */
class MAYAFLUX_API GraphicsBatchProcessor : public BufferProcessor {
public:
    /**
     * @brief Creates a new graphics batch processor
     * @param root_buffer Shared pointer to the root buffer this processor manages
     */
    GraphicsBatchProcessor(std::shared_ptr<Buffer> root_buffer);

    /**
     * @brief Processes a buffer by coordinating child buffer operations
     * @param buffer Buffer to process (should be RootGraphicsBuffer)
     *
     * This executes the batch processing loop:
     * 1. Iterates through all child buffers
     * 2. Executes each buffer's default processor
     * 3. Runs each buffer's processing chain
     * 4. Handles synchronization and error cases
     */
    void processing_function(std::shared_ptr<Buffer> buffer) override;

    /**
     * @brief Called when processor is attached to a buffer
     * @param buffer Buffer being attached to
     *
     * Validates that the buffer is a RootGraphicsBuffer and ensures
     * token compatibility for GPU processing.
     */
    void on_attach(std::shared_ptr<Buffer> buffer) override;

    /**
     * @brief Checks compatibility with a specific buffer type
     * @param buffer Buffer to check compatibility with
     * @return True if compatible (buffer is RootGraphicsBuffer), false otherwise
     */
    [[nodiscard]] bool is_compatible_with(std::shared_ptr<Buffer> buffer) const override;

private:
    /**
     * @brief Shared pointer to the root buffer this processor manages
     */
    std::shared_ptr<RootGraphicsBuffer> m_root_buffer;
};

} // namespace MayaFlux::Buffers
