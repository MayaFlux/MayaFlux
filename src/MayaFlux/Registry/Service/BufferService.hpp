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
     * @brief Copy a contiguous byte range from one device buffer to another.
     * @param src    Source buffer handle (opaque to caller).
     * @param dst    Destination buffer handle (opaque to caller).
     * @param size   Byte count to copy.
     * @param src_offset Byte offset into source.
     * @param dst_offset Byte offset into destination.
     *
     * Recorded into a single-use command buffer and submitted synchronously.
     * Caller must ensure both buffers are device-accessible.
     */
    std::function<void(void*, void*, size_t, size_t, size_t)> copy_buffer;

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
     * @brief Query the device address of a buffer
     * @param buffer Buffer handle: must have been initialized with a BDA-capable usage
     * @return Device address, or 0 if the buffer does not support device addressing
     */
    std::function<uint64_t(const std::shared_ptr<void>&)> get_buffer_device_address;

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

    /**
     * @brief Submit commands with a fence. Non-blocking.
     * @param recorder Lambda that records commands into command buffer.
     * @return Opaque handle tracking the submission.
     *
     * Allocates a command buffer and fence, runs the recorder, submits to
     * the graphics queue with the fence attached, and returns immediately.
     * The returned handle owns the cmd buffer + fence lifetime.
     *
     * Use wait_fenced() to block until the submission completes, then
     * release_fenced() to free the underlying resources.
     *
     * Unlike execute_immediate, does not drain the graphics queue. Safe
     * to invoke from any thread; blocks only the thread that calls
     * wait_fenced().
     */
    std::function<std::shared_ptr<void>(std::function<void(void*)>)> execute_fenced;

    /**
     * @brief Wait for a fenced submission to complete.
     * @param handle Handle returned by execute_fenced.
     *
     * Blocks the calling thread until the fence signals. Safe to call
     * from any thread. No-op if handle is null or already waited.
     */
    std::function<void(const std::shared_ptr<void>&)> wait_fenced;

    /**
     * @brief Release resources associated with a fenced submission.
     * @param handle Handle returned by execute_fenced.
     *
     * Destroys the fence and frees the command buffer. Caller must have
     * already observed completion via wait_fenced() (or equivalent) before
     * calling this. No-op if handle is null.
     */
    std::function<void(const std::shared_ptr<void>&)> release_fenced;

    /**
     * @brief Copy a byte range between two device buffers without blocking the graphics queue.
     * @param src        Source buffer handle (opaque VkBuffer).
     * @param dst        Destination buffer handle (opaque VkBuffer).
     * @param size       Byte count to copy.
     * @param src_offset Byte offset into source.
     * @param dst_offset Byte offset into destination.
     * @return           Opaque fence handle. Pass to wait_fenced(), then release_fenced().
     */
    std::function<std::shared_ptr<void>(void*, void*, size_t, size_t, size_t)> copy_buffer_fenced;
};

} // namespace MayaFlux::Registry::Services
