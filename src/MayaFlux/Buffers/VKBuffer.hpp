#pragma once

#include "Buffer.hpp"

#include "MayaFlux/Buffers/BufferProcessor.hpp"
#include "MayaFlux/Core/ProcessingTokens.hpp"

#include "MayaFlux/Portal/Graphics/ShaderUtils.hpp"

namespace MayaFlux::Registry::Service {
struct BufferService;
struct ComputeService;
}

namespace MayaFlux::Core {
class Window;
}

namespace MayaFlux::Buffers {

struct VKBufferResources {
    vk::Buffer buffer;
    vk::DeviceMemory memory;
    void* mapped_ptr;
};

using RenderPipelineID = uint64_t;
using CommandBufferID = uint64_t;

/**
 * @class VKBuffer
 * @brief Vulkan-backed buffer wrapper used in processing chains
 *
 * VKBuffer is a lightweight, high-level representation of a GPU buffer used by
 * the MayaFlux processing pipeline. It carries semantic metadata (Kakshya
 * modalities and dimensions), integrates with the Buffer processing chain and
 * BufferManager, and exposes Vulkan handles once the backend registers the
 * buffer. Prior to registration the object contains no GPU resources and can
 * be manipulated cheaply (like AudioBuffer).
 *
 * Responsibilities:
 * - Store buffer size, usage intent and semantic modality
 * - Provide inferred data dimensions for processors and pipeline inspection
 * - Hold Vulkan handles (VkBuffer, VkDeviceMemory) assigned by graphics backend
 * - Provide convenience helpers for common Vulkan creation flags and memory props
 * - Integrate with BufferProcessor / BufferProcessingChain for default processing
 *
 * Note: Actual allocation, mapping and command-based transfers are performed by
 * the graphics backend / BufferManager. VKBuffer only stores handles and
 * metadata and provides helpers for processors to operate on it.
 */
class MAYAFLUX_API VKBuffer : public Buffer {
public:
    enum class Usage : uint8_t {
        STAGING, ///< Host-visible staging buffer (CPU-writable)
        DEVICE, ///< Device-local GPU-only buffer
        COMPUTE, ///< Storage buffer for compute shaders
        VERTEX, ///< Vertex buffer
        INDEX, ///< Index buffer
        UNIFORM ///< Uniform buffer (host-visible when requested)
    };

    /**
     * @brief Context shared with BufferProcessors during pipeline execution
     *
     * Processors can use this struct to share data (e.g., push constant staging)
     * and metadata during processing of this buffer in a chain.
     */
    struct PipelineContext {
        std::vector<uint8_t> push_constant_staging;

        std::vector<Portal::Graphics::DescriptorBindingInfo> descriptor_buffer_bindings;

        std::unordered_map<std::string, std::any> metadata;
    };

    /**
     * @brief Construct an unregistered VKBuffer
     *
     * Creates a VKBuffer object with the requested capacity, usage intent and
     * semantic modality. No Vulkan resources are created by this constructor —
     * registration with the BufferManager / backend is required to allocate
     * VkBuffer and VkDeviceMemory.
     *
     * @param size_bytes Buffer capacity in bytes.
     * @param usage Intended usage pattern (affects Vulkan flags and memory).
     * @param modality Semantic interpretation of the buffer contents.
     */
    VKBuffer(
        size_t size_bytes,
        Usage usage,
        Kakshya::DataModality modality);

    VKBuffer() = default;

    /**
     * @brief Virtual destructor
     *
     * VKBuffer does not own Vulkan resources directly; cleanup is handled by
     * the backend during unregistration. The destructor ensures derived class
     * cleanup and safe destruction semantics.
     */
    ~VKBuffer() override;

    /**
     * @brief Clear buffer contents
     *
     * If the buffer is host-visible and mapped the memory is zeroed. For
     * device-local buffers the backend must provide a ClearBufferProcessor
     * that performs the appropriate GPU-side clear.
     */
    void clear() override;

    /**
     * @brief Read buffer contents as Kakshya DataVariant
     *
     * For host-visible buffers this returns a single DataVariant containing the
     * raw bytes. For device-local buffers this warns and returns empty — a
     * BufferDownloadProcessor should be used to read GPU-only memory.
     *
     * @return Vector of DataVariant objects representing buffer contents.
     */
    std::vector<Kakshya::DataVariant> get_data();

    /**
     * @brief Write data into the buffer
     *
     * If the buffer is host-visible and mapped the provided data is copied into
     * the mapped memory. For device-local buffers, a BufferUploadProcessor must
     * be present in the processing chain to perform the staging/upload.
     *
     * @param data Vector of Kakshya::DataVariant containing the payload to copy.
     */
    void set_data(const std::vector<Kakshya::DataVariant>& data);

    /**
     * @brief Resize buffer and recreate GPU resources if needed
     * @param new_size New size in bytes
     * @param preserve_data If true, copy existing data to new buffer
     *
     * If buffer is already initialized (has GPU resources), this will:
     * 1. Create new GPU buffer with new size
     * 2. Optionally copy old data
     * 3. Destroy old GPU buffer
     * 4. Update buffer resources
     *
     * If buffer is not initialized, just updates logical size.
     */
    void resize(size_t new_size, bool preserve_data = false);

    /**
     * @brief Get current logical size in bytes
     * @return Buffer size in bytes.
     */
    size_t get_size() const { return m_size_bytes; }

    /**
     * @brief Run the buffer's default processor (if set and enabled)
     *
     * Invokes the attached default BufferProcessor. Processors should be
     * prepared to handle mapped/unmapped memory according to buffer usage.
     */
    void process_default() override;

    /**
     * @brief Set the buffer's default processor
     *
     * Attaches a processor that will be invoked by process_default(). The
     * previous default processor (if any) is detached first.
     *
     * @param processor Shared pointer to a BufferProcessor or nullptr to clear.
     */
    void set_default_processor(std::shared_ptr<Buffers::BufferProcessor> processor) override;

    /**
     * @brief Get the currently attached default processor
     * @return Shared pointer to the default BufferProcessor or nullptr.
     */
    std::shared_ptr<Buffers::BufferProcessor> get_default_processor() const override;

    /**
     * @brief Access the buffer's processing chain
     * @return Shared pointer to the BufferProcessingChain used for this buffer.
     */
    std::shared_ptr<Buffers::BufferProcessingChain> get_processing_chain() override;

    /**
     * @brief Replace the buffer's processing chain
     * @param chain New processing chain to assign.
     * @param force If true, replaces existing chain even if one is set.
     */
    void set_processing_chain(std::shared_ptr<Buffers::BufferProcessingChain> chain, bool force = false) override;

    bool has_data_for_cycle() const override { return m_has_data; }
    bool needs_removal() const override { return m_needs_removal; }
    void mark_for_processing(bool has_data) override { m_has_data = has_data; }
    void mark_for_removal() override { m_needs_removal = true; }
    void enforce_default_processing(bool should_process) override { m_process_default = should_process; }
    bool needs_default_processing() override { return m_process_default; }

    /**
     * @brief Try to acquire processing lock for this buffer
     * @return True if lock acquired, false if already processing.
     *
     * Uses an atomic flag to guard concurrent processing attempts.
     */
    inline bool try_acquire_processing() override
    {
        bool expected = false;
        return m_is_processing.compare_exchange_strong(expected, true,
            std::memory_order_acquire, std::memory_order_relaxed);
    }

    /**
     * @brief Release previously acquired processing lock
     */
    inline void release_processing() override
    {
        m_is_processing.store(false, std::memory_order_release);
    }

    /**
     * @brief Query whether the buffer is currently being processed
     * @return True if a processing operation holds the lock.
     */
    inline bool is_processing() const override
    {
        return m_is_processing.load(std::memory_order_acquire);
    }

    /** Get VkBuffer handle (VK_NULL_HANDLE if not registered) */
    vk::Buffer& get_buffer() { return m_resources.buffer; }

    /* Get logical buffer size as VkDeviceSize */
    vk::DeviceSize get_size_bytes() const { return m_size_bytes; }

    /**
     * @brief Convert modality to a recommended VkFormat
     * @return VkFormat corresponding to the buffer's modality, or VK_FORMAT_UNDEFINED.
     */
    vk::Format get_format() const;

    /** Check whether Vulkan handles are present (buffer registered) */
    bool is_initialized() const { return m_resources.buffer != VK_NULL_HANDLE; }

    /**
     * @brief Setup processors with a processing token
     * @param token ProcessingToken to assign.
     *
     * For VKBuffer this is a no-op as processors get the token.
     * This is meant for derived classes that need to setup default processors
     */
    virtual void setup_processors(ProcessingToken token) { }

    /** Get the buffer's semantic modality */
    Kakshya::DataModality get_modality() const { return m_modality; }

    /** Get the inferred data dimensions for the buffer contents */
    const std::vector<Kakshya::DataDimension>& get_dimensions() const { return m_dimensions; }

    /**
     * @brief Update the semantic modality and re-infer dimensions
     * @param modality New Kakshya::DataModality to apply.
     */
    void set_modality(Kakshya::DataModality modality);

    /** Retrieve the declared usage intent */
    Usage get_usage() const { return m_usage; }

    /** Set VkBuffer handle after backend allocation */
    void set_buffer(vk::Buffer buffer) { m_resources.buffer = buffer; }

    /** Set device memory handle after backend allocation */
    void set_memory(vk::DeviceMemory memory) { m_resources.memory = memory; }

    /** Set mapped host pointer (for host-visible allocations) */
    void set_mapped_ptr(void* ptr) { m_resources.mapped_ptr = ptr; }

    /** Set all buffer resources at once */
    inline void set_buffer_resources(const VKBufferResources& resources)
    {
        m_resources = resources;
    }

    /** Get all buffer resources at once */
    inline const VKBufferResources& get_buffer_resources() { return m_resources; }

    /**
     * @brief Whether this VKBuffer should be host-visible
     * @return True for staging or uniform buffers, false for device-local types.
     */
    bool is_host_visible() const
    {
        return m_usage == Usage::STAGING || m_usage == Usage::UNIFORM;
    }

    /**
     * @brief Get appropriate VkBufferUsageFlags for creation based on Usage
     * @return VkBufferUsageFlags to be used when creating VkBuffer.
     */
    vk::BufferUsageFlags get_usage_flags() const;

    /**
     * @brief Get appropriate VkMemoryPropertyFlags for allocation based on Usage
     * @return VkMemoryPropertyFlags to request during memory allocation.
     */
    vk::MemoryPropertyFlags get_memory_properties() const;

    /** Get mapped host pointer (nullptr if not host-visible or unmapped) */
    void* get_mapped_ptr() const { return m_resources.mapped_ptr; }

    /** Get device memory handle */
    void mark_dirty_range(size_t offset, size_t size);

    /** Mark a range as invalid (needs download) */
    void mark_invalid_range(size_t offset, size_t size);

    /** Retrieve and clear all dirty ranges */
    std::vector<std::pair<size_t, size_t>> get_and_clear_dirty_ranges();

    /** Retrieve and clear all invalid ranges */
    std::vector<std::pair<size_t, size_t>> get_and_clear_invalid_ranges();

    /**
     * @brief Associate this buffer with a window for rendering
     * @param window Target window for rendering this buffer's content
     *
     * When this buffer is processed, its content will be rendered to the associated window.
     * Currently supports one window per buffer (will be extended to multiple windows).
     */
    void set_pipeline_window(RenderPipelineID id, const std::shared_ptr<Core::Window>& window)
    {
        m_window_pipelines[id] = window;
    }

    /**
     * @brief Get the window associated with this buffer
     * @return Target window, or nullptr if not set
     */
    std::shared_ptr<Core::Window> get_pipeline_window(RenderPipelineID id) const
    {
        auto it = m_window_pipelines.find(id);
        if (it != m_window_pipelines.end()) {
            return it->second;
        }
        return nullptr;
    }

    /**
     * @brief Check if this buffer has a rendering pipeline configured
     */
    bool has_render_pipeline() const
    {
        return !m_window_pipelines.empty();
    }

    /**
     * @brief Get all render pipelines associated with this buffer
     * @return Map of RenderPipelineID to associated windows
     */
    std::unordered_map<RenderPipelineID, std::shared_ptr<Core::Window>> get_render_pipelines() const
    {
        return m_window_pipelines;
    }

    /**
     * @brief Store recorded command buffer for a pipeline
     */
    void set_pipeline_command(RenderPipelineID pipeline_id,
        CommandBufferID cmd_id)
    {
        m_pipeline_commands[pipeline_id] = cmd_id;
    }

    /**
     * @brief Get recorded command buffer for a pipeline
     */
    CommandBufferID get_pipeline_command(RenderPipelineID pipeline_id) const
    {
        auto it = m_pipeline_commands.find(pipeline_id);
        return it != m_pipeline_commands.end() ? it->second : 0;
    }

    /**
     * @brief Clear all recorded commands (called after presentation)
     */
    void clear_pipeline_commands()
    {
        m_pipeline_commands.clear();
    }

    /**
     * @brief Set vertex layout for this buffer
     *
     * Required before using buffer with graphics rendering.
     * Describes how to interpret buffer data as vertices.
     *
     * @param layout VertexLayout describing vertex structure
     */
    void set_vertex_layout(const Kakshya::VertexLayout& layout);

    /**
     * @brief Get vertex layout if set
     * @return Optional containing layout, or empty if not set
     */
    std::optional<Kakshya::VertexLayout> get_vertex_layout() const { return m_vertex_layout; }

    /**
     * @brief Check if this buffer has vertex layout configured
     */
    bool has_vertex_layout() const { return m_vertex_layout.has_value(); }

    /**
     * @brief Clear vertex layout
     */
    void clear_vertex_layout()
    {
        m_vertex_layout.reset();
    }

    std::shared_ptr<Buffer> clone_to(uint8_t dest_desc) override;

    /**
     * @brief Create a clone of this buffer with the same data and properties
     * @param usage Usage enum for the cloned buffer
     * @return Shared pointer to the cloned VKBuffer
     *
     * The cloned buffer will have the same size, modality, dimensions,
     * processing chain and default processor as the original. Changes to
     * one buffer after cloning do not affect the other.
     */
    std::shared_ptr<VKBuffer> clone_to(Usage usage);

    /** Set whether this buffer is for internal engine usage */
    void force_internal_usage(bool internal) override { m_internal_usage = internal; }

    /** Check whether this buffer is for internal engine usage */
    bool is_internal_only() const override { return m_internal_usage; }

    /** Access the pipeline context for custom metadata (non-const) */
    PipelineContext& get_pipeline_context() { return m_pipeline_context; }

    /** Access the pipeline context for custom metadata (const) */
    const PipelineContext& get_pipeline_context() const { return m_pipeline_context; }

private:
    VKBufferResources m_resources;

    // Buffer parameters
    size_t m_size_bytes {};
    Usage m_usage {};

    // Semantic metadata
    Kakshya::DataModality m_modality;
    std::vector<Kakshya::DataDimension> m_dimensions;

    std::optional<Kakshya::VertexLayout> m_vertex_layout;

    // Buffer interface state
    bool m_has_data { true };
    bool m_needs_removal {};
    bool m_process_default { true };
    bool m_internal_usage {};
    std::atomic<bool> m_is_processing;
    std::shared_ptr<Buffers::BufferProcessor> m_default_processor;
    std::shared_ptr<Buffers::BufferProcessingChain> m_processing_chain;
    ProcessingToken m_processing_token;
    PipelineContext m_pipeline_context;

    std::unordered_map<RenderPipelineID, std::shared_ptr<Core::Window>> m_window_pipelines;
    std::unordered_map<RenderPipelineID, CommandBufferID> m_pipeline_commands;

    std::vector<std::pair<size_t, size_t>> m_dirty_ranges;
    std::vector<std::pair<size_t, size_t>> m_invalid_ranges;

    /**
     * @brief Infer Kakshya::DataDimension entries from a given byte count
     *
     * Uses the current modality and provided byte count to populate
     * m_dimensions so processors and UI code can reason about the buffer's layout.
     *
     * @param byte_count Number of bytes of data to infer dimensions from.
     */
    void infer_dimensions_from_data(size_t byte_count);
};

class VKBufferProcessor : public BufferProcessor {
protected:
    Registry::Service::BufferService* m_buffer_service = nullptr;
    Registry::Service::ComputeService* m_compute_service = nullptr;

    void initialize_buffer_service();
    void initialize_compute_service();
};

} // namespace MayaFlux::Buffers
