#pragma once

namespace MayaFlux::Registry::Service {

/**
 * @brief Backend buffer management service interface
 *
 * Defines GPU/backend buffer operations that any graphics backend must provide.
 * Implementations are backend-specific (Vulkan, OpenGL, Metal, DirectX, etc.).
 * All handles are opaque to maintain backend independence while providing type safety.
 */
struct MAYAFLUX_API BufferService {
    /**
     * @brief Initialize a buffer object
     * @param buffer Buffer handle to initialize
     */
    std::function<void(const std::shared_ptr<void>&)> initialize_buffer;

    /**
     * @brief Destroy a buffer and free its associated memory
     * @param buffer Buffer handle to destroy
     *
     * Automatically handles cleanup of both buffer and memory resources.
     * Safe to call with invalid/null handles (no-op).
     */
    std::function<void(const std::shared_ptr<void>&)> destroy_buffer;

    /**
     * @brief Map buffer memory to host-visible pointer
     * @param memory Memory handle to map
     * @param offset Offset in bytes from memory start
     * @param size Size in bytes to map (or 0 for entire range)
     * @return Host-visible pointer to mapped memory
     *
     * Only valid for host-visible memory. Pointer remains valid until
     * unmap_buffer() is called. Multiple maps of the same memory are
     * backend-specific behavior.
     */
    std::function<void*(void*, size_t, size_t)> map_buffer;

    /**
     * @brief Unmap previously mapped buffer memory
     * @param memory Memory handle to unmap
     *
     * Invalidates the pointer returned by map_buffer(). Host writes may
     * not be visible to device until flush_range() is called.
     */
    std::function<void(void*)> unmap_buffer;

    /**
     * @brief Flush mapped memory range (make host writes visible to device)
     * @param memory Memory handle
     * @param offset Offset in bytes
     * @param size Size in bytes to flush (or 0 for entire mapped range)
     *
     * Required for non-coherent host-visible memory after CPU writes.
     * For coherent memory, this is typically a no-op but may still
     * provide ordering guarantees.
     */
    std::function<void(void*, size_t, size_t)> flush_range;

    /**
     * @brief Invalidate mapped memory range (make device writes visible to host)
     * @param memory Memory handle
     * @param offset Offset in bytes
     * @param size Size in bytes to invalidate (or 0 for entire mapped range)
     *
     * Required for non-coherent host-visible memory before CPU reads.
     * Ensures device writes are visible to the host.
     */
    std::function<void(void*, size_t, size_t)> invalidate_range;

    /**
     * @brief Execute commands immediately with synchronization
     * @param recorder Lambda that records commands into command buffer
     *
     * The backend handles:
     * - Command buffer allocation
     * - Begin/end recording
     * - Queue submission
     * - Fence wait for completion
     *
     * Blocks until GPU operations complete. Thread-safe.
     *
     * Example:
     *   service->execute_immediate([&](CommandBuffer cmd) {
     *       // Record copy, clear, or other operations
     *   });
     */
    std::function<void(std::function<void(void*)>)> execute_immediate;

    /**
     * @brief Record commands for deferred execution
     * @param recorder Lambda that records commands into command buffer
     *
     * Commands are batched and submitted later by backend for optimal
     * performance. Does not block. Thread-safe.
     *
     * Use for operations where immediate completion is not required.
     * Backend determines when to flush batched commands.
     */
    std::function<void(std::function<void(void*)>)> record_deferred;
};

} // namespace MayaFlux::Registry::Services
