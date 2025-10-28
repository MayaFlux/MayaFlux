#pragma once

#include <vulkan/vulkan.hpp>

namespace MayaFlux::Buffers {
class Buffer;
class VKBuffer;

using BufferRegistrationCallback = std::function<void(std::shared_ptr<VKBuffer>)>;

/**
 * @class VKProcessingContext
 * @brief Minimal GPU command interface for processors
 *
 * Provides command buffer recording without exposing full backend.
 * Passed to processors during processing.
 */
class MAYAFLUX_API VKProcessingContext {
public:
    using CommandRecorder = std::function<void(vk::CommandBuffer)>;

    VKProcessingContext() = default;
    ~VKProcessingContext() = default;

    /**
     * @brief Execute GPU commands immediately
     * @param recorder Lambda that records commands into command buffer
     *
     * Usage:
     *   context.execute_immediate([&](vk::CommandBuffer cmd) {
     *       cmd.copyBuffer(src, dst, copy_region);
     *   });
     *
     * Handles:
     * - Command buffer allocation
     * - Begin/end recording
     * - Queue submission
     * - Fence wait
     */
    void execute_immediate(CommandRecorder recorder);

    /**
     * @brief Record commands for deferred execution
     * @param recorder Lambda that records commands
     *
     * Commands are batched and submitted later by backend.
     * Use for optimal performance when order doesn't matter.
     */
    void record_deferred(CommandRecorder recorder);

    /**
     * @brief Flush host-visible buffer memory
     */
    void flush_buffer(vk::DeviceMemory memory, size_t offset, size_t size);

    /**
     * @brief Invalidate host-visible buffer memory
     */
    void invalidate_buffer(vk::DeviceMemory memory, size_t offset, size_t size);

    // Backend sets these during initialization
    void set_execute_immediate_callback(std::function<void(CommandRecorder)> callback);
    void set_record_deferred_callback(std::function<void(CommandRecorder)> callback);
    void set_flush_callback(std::function<void(vk::DeviceMemory, size_t, size_t)> callback);
    void set_invalidate_callback(std::function<void(vk::DeviceMemory, size_t, size_t)> callback);

    /**
     * @brief Initializes a buffer using the registered initializer callback
     * @param buffer Buffer to initialize
     */
    static void initialize_buffer(const std::shared_ptr<VKBuffer>& buffer);

    /**
     * @brief Cleans up a buffer using the registered cleaner callback
     * @param buffer Buffer to clean up
     */
    static void cleanup_buffer(const std::shared_ptr<VKBuffer>& buffer);

    /**
     * @brief Sets the global buffer initializer and cleaner callbacks
     * @param initializer Callback to initialize buffers
     */
    static void set_initializer(BufferRegistrationCallback initializer);

    /**
     * @brief Sets the global buffer cleaner callback
     * @param cleaner Callback to clean up buffers
     */
    static void set_cleaner(BufferRegistrationCallback cleaner);

private:
    std::function<void(CommandRecorder)> m_execute_immediate;
    std::function<void(CommandRecorder)> m_record_deferred;
    std::function<void(vk::DeviceMemory, size_t, size_t)> m_flush;
    std::function<void(vk::DeviceMemory, size_t, size_t)> m_invalidate;

    static BufferRegistrationCallback s_buffer_initializer;
    static BufferRegistrationCallback s_buffer_cleaner;
};

} // namespace MayaFlux::Buffers
