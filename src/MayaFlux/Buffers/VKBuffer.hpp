#pragma once

#include "Buffer.hpp"
#include "MayaFlux/Core/ProcessingTokens.hpp"
#include "MayaFlux/Kakshya/NDData/NDData.hpp"
#include "vulkan/vulkan.hpp"

namespace MayaFlux::Buffers {

struct VKBufferResources {
    vk::Buffer buffer;
    vk::DeviceMemory memory;
    void* mapped_ptr;
};

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
     * @brief Request a new capacity for the buffer
     *
     * Updates the logical size and inferred dimensions. If the buffer is
     * already registered the actual Vulkan re-creation is deferred to the
     * backend (BufferManager) and the Vulkan handles are invalidated locally.
     *
     * @param new_size New buffer size in bytes.
     */
    void resize(size_t new_size);

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
     */
    void set_processing_chain(std::shared_ptr<Buffers::BufferProcessingChain> chain) override;

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
    vk::Buffer& get_vk_buffer() { return m_resources.buffer; }

    /* Get logical buffer size as VkDeviceSize */
    vk::DeviceSize get_size_bytes() const { return m_size_bytes; }

    /**
     * @brief Convert modality to a recommended VkFormat
     * @return VkFormat corresponding to the buffer's modality, or VK_FORMAT_UNDEFINED.
     */
    vk::Format get_format() const;

    /** Check whether Vulkan handles are present (buffer registered) */
    bool is_initialized() const { return m_resources.buffer != VK_NULL_HANDLE; }

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

    // void flush(size_t offset = 0, size_t size = VK_WHOLE_SIZE);
    // void invalidate(size_t offset, size_t size);

private:
    VKBufferResources m_resources;

    // Buffer parameters
    size_t m_size_bytes {};
    Usage m_usage {};

    // Semantic metadata
    Kakshya::DataModality m_modality;
    std::vector<Kakshya::DataDimension> m_dimensions;

    // Buffer interface state
    bool m_has_data { true };
    bool m_needs_removal {};
    bool m_process_default { true };
    std::atomic<bool> m_is_processing;
    std::shared_ptr<Buffers::BufferProcessor> m_default_processor;
    std::shared_ptr<Buffers::BufferProcessingChain> m_processing_chain;
    ProcessingToken m_processing_token;

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

} // namespace MayaFlux::Buffers
