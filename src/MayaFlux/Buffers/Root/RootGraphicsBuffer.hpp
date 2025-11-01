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
     * @brief Information about a buffer that's ready to render
     */
    struct RenderableBufferInfo {
        std::shared_ptr<VKBuffer> buffer;
        std::shared_ptr<Core::Window> target_window;
        RenderPipelineID pipeline_id;
        CommandBufferID command_buffer_id;
    };

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

    /**
     * @brief Get list of buffers ready for rendering
     * @return Vector of renderable buffers with their associated windows and pipelines
     *
     * Populated by GraphicsBatchProcessor during batch processing.
     * Consumed by PresentProcessor to perform actual rendering.
     */
    const std::vector<RenderableBufferInfo>& get_renderable_buffers() const
    {
        return m_renderable_buffers;
    }

    /**
     * @brief Clear the renderable buffers list
     *
     * Called after rendering completes to prepare for next frame.
     */
    void clear_renderable_buffers()
    {
        m_renderable_buffers.clear();
    }

private:
    friend class GraphicsBatchProcessor;

    /**
     * @brief Creates the default graphics batch processor
     * @return Shared pointer to new GraphicsBatchProcessor
     */
    std::shared_ptr<BufferProcessor> create_default_processor();

    /**
     * @brief Add a buffer to the renderable list
     *
     * Called by GraphicsBatchProcessor during batch processing.
     */
    void add_renderable_buffer(const RenderableBufferInfo& info)
    {
        m_renderable_buffers.push_back(info);
    }

    std::vector<RenderableBufferInfo> m_renderable_buffers;

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

/**
 * @class PresentProcessor
 * @brief Final processor that executes render operations after all buffer processing
 *
 * PresentProcessor is designed to be set as the final processor of RootGraphicsBuffer.
 * It's invoked after all child buffer processing chains have completed, making it
 * the ideal point to:
 * - Record render commands using processed GPU buffers
 * - Coordinate rendering operations across multiple buffers
 * - Submit command buffers to GPU queues
 * - Present frames to the swapchain
 *
 * Design Philosophy:
 * - Callback-based for maximum flexibility
 * - Receives RootGraphicsBuffer with all processed child buffers
 * - No assumptions about rendering strategy (forward, deferred, etc.)
 * - Can query buffers by usage type for organized rendering
 *
 * Usage Pattern:
 * ```cpp
 * auto render_proc = std::make_shared<PresentProcessor>(
 *     [this](RootGraphicsBuffer* root) {
 *         // All buffers processed, ready for rendering
 *         auto vertex_bufs = root->get_buffers_by_usage(VKBuffer::Usage::VERTEX);
 *         auto uniform_bufs = root->get_buffers_by_usage(VKBuffer::Usage::UNIFORM);
 *
 *         // Record render commands
 *         record_render_pass(vertex_bufs, uniform_bufs);
 *
 *         // Submit and present
 *         submit_graphics_queue();
 *         present_frame();
 *     }
 * );
 *
 * root_graphics_buffer->set_final_processor(render_proc);
 * ```
 *
 * Token Compatibility:
 * - Primary Token: GRAPHICS_BACKEND
 * - Executes at frame rate (after all GPU buffer processing)
 * - Should NOT perform heavy CPU computations (rendering coordination only)
 */
class MAYAFLUX_API PresentProcessor : public BufferProcessor {
public:
    /**
     * @brief Callback signature for render operations
     * @param root RootGraphicsBuffer with all processed child buffers
     *
     * The callback receives the root buffer after all child processing is complete.
     * Child buffers are accessible via:
     * - root->get_child_buffers() - all buffers
     * - root->get_buffers_by_usage(usage) - filtered by usage type
     */
    using RenderCallback = std::function<void(const std::shared_ptr<RootGraphicsBuffer>& root)>;

    /**
     * @brief Creates a render processor with a callback function
     * @param callback Function to execute for rendering operations
     *
     * The callback will be invoked during processing_function() with access
     * to the RootGraphicsBuffer and all its processed child buffers.
     */
    PresentProcessor(RenderCallback callback);

    /**
     * @brief Default constructor (no callback set)
     *
     * Callback can be set later via set_callback().
     * Processing will be a no-op until callback is configured.
     */
    PresentProcessor();

    ~PresentProcessor() override = default;

    /**
     * @brief Executes the render callback
     * @param buffer Buffer to process (must be RootGraphicsBuffer)
     *
     * Validates buffer type and invokes the render callback.
     * This is the core rendering coordination point - all child buffers
     * have been processed by the time this executes.
     */
    void processing_function(std::shared_ptr<Buffer> buffer) override;

    /**
     * @brief Called when processor is attached to a buffer
     * @param buffer Buffer being attached to
     *
     * Validates that the buffer is a RootGraphicsBuffer and ensures
     * token compatibility for graphics rendering.
     */
    void on_attach(std::shared_ptr<Buffer> buffer) override;

    /**
     * @brief Called when processor is detached from a buffer
     * @param buffer Buffer being detached from
     */
    void on_detach(std::shared_ptr<Buffer> buffer) override;

    /**
     * @brief Checks compatibility with a specific buffer type
     * @param buffer Buffer to check compatibility with
     * @return True if buffer is RootGraphicsBuffer, false otherwise
     */
    [[nodiscard]] bool is_compatible_with(std::shared_ptr<Buffer> buffer) const override;

    /**
     * @brief Sets or updates the render callback
     * @param callback New callback function
     *
     * Allows runtime reconfiguration of rendering strategy.
     * Useful for switching between different rendering modes or techniques.
     */
    void set_callback(RenderCallback callback);

    /**
     * @brief Checks if a callback is configured
     * @return True if callback is set, false otherwise
     */
    [[nodiscard]] bool has_callback() const { return static_cast<bool>(m_callback); }

    /**
     * @brief Clears the current callback
     *
     * After clearing, processing will be a no-op until a new callback is set.
     */
    void clear_callback() { m_callback = nullptr; }

private:
    /**
     * @brief User-provided render callback
     */
    RenderCallback m_callback;

    /**
     * @brief Reference to root buffer (for validation and callbacks)
     *
     * We store a raw pointer to avoid circular references, as the root
     * buffer owns the shared_ptr to this processor.
     */
    std::shared_ptr<RootGraphicsBuffer> m_root_buffer;

    void fallback_renderer(const std::shared_ptr<RootGraphicsBuffer>& root);
};

} // namespace MayaFlux::Buffers
