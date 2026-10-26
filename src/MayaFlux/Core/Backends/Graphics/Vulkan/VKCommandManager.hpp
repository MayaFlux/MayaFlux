#pragma once

#include <vulkan/vulkan.hpp>

namespace MayaFlux::Core {

/**
 * @class VKCommandManager
 * @brief Owns command pools, queue submission and command buffer retirement
 *
 * Every queue submission goes through submit(), which signals one timeline
 * semaphore with a strictly increasing value. That value is the completion
 * token for the submission: command buffers are retired against it and callers
 * wait on it instead of draining a queue.
 *
 * Submission is externally synchronized, as vkQueueSubmit is. Two threads must
 * not call submit(), retire() or collect() concurrently.
 */
class MAYAFLUX_API VKCommandManager {
public:
    VKCommandManager() = default;
    ~VKCommandManager();

    /**
     * @brief Initialize command manager
     * @param device Logical device with timelineSemaphore enabled
     * @param graphics_queue_family Graphics queue family index
     * @param compute_queue_family Compute queue family index (optional, for compute operations)
     * @return True if initialization succeeded
     */
    bool initialize(vk::Device device, uint32_t graphics_queue_family, uint32_t compute_queue_family);

    /**
     * @brief Cleanup all command pools and buffers
     */
    void cleanup();

    /**
     * @brief Allocate a command buffer with specified level
     * @param level Primary or secondary
     * @return Allocated command buffer
     */
    vk::CommandBuffer allocate_command_buffer(
        vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary);

    /**
     * @brief Free a command buffer back to the pool
     */
    void free_command_buffer(vk::CommandBuffer command_buffer);

    /**
     * @brief Begin single-time command (for transfers, etc.)
     * @return Command buffer ready for recording
     */
    vk::CommandBuffer begin_single_time_commands();

    /**
     * @brief End, submit and wait for a single-time command
     * @param command_buffer Command buffer to submit
     * @param queue Queue to submit to
     *
     * Waits for this submission only and frees the command buffer.
     */
    void end_single_time_commands(vk::CommandBuffer command_buffer, vk::Queue queue);

    /**
     * @brief Reset command pool (invalidates all allocated buffers)
     */
    void reset_pool();

    /**
     * @brief Get the command pool
     */
    [[nodiscard]] vk::CommandPool get_pool() const { return m_command_pool; }

    /**
     * @brief Begin single-time command for compute operations
     * @return Command buffer ready for recording
     */
    [[nodiscard]] vk::CommandBuffer begin_single_time_commands_compute();

    /**
     * @brief Check whether deferred commands are waiting for flush_deferred()
     */
    [[nodiscard]] bool has_deferred_commands() const { return m_deferred_pending; }

    /**
     * @brief Record deferred commands for later submission
     * @param recorder Command recording function that takes a command buffer
     */
    void record_deferred_commands(const std::function<void(vk::CommandBuffer)>& recorder);

    /**
     * @brief Submit the pending deferred commands and retire their buffer
     * @param queue Queue to submit to
     * @return Completion value of the submission, or 0 when nothing was pending or the submit failed
     */
    uint64_t flush_deferred(vk::Queue queue);

    /**
     * @brief Submit command buffers and signal the timeline
     * @param queue Queue to submit to
     * @param commands Command buffers, executed in order
     * @param waits Semaphores to wait on; timeline waits carry their value
     * @param signals Additional semaphores to signal; binary ones carry value 0
     * @param fence Optional fence signaled with the submission
     * @return Completion value of the submission, or 0 if the submit failed
     *
     * The returned value is signaled on get_timeline() when the submission
     * completes. It orders strictly with every other submission through this
     * manager.
     */
    uint64_t submit(
        vk::Queue queue,
        std::span<const vk::CommandBuffer> commands,
        std::span<const vk::SemaphoreSubmitInfo> waits = {},
        std::span<const vk::SemaphoreSubmitInfo> signals = {},
        vk::Fence fence = {});

    /**
     * @brief Free a command buffer once its submission has completed
     * @param command_buffer Command buffer to free
     * @param value Completion value returned by submit()
     */
    void retire(vk::CommandBuffer command_buffer, uint64_t value);

    /**
     * @brief Free every retired command buffer whose submission has completed
     */
    void collect();

    /**
     * @brief Highest completion value reached on the timeline
     */
    [[nodiscard]] uint64_t completed_value() const;

    /**
     * @brief Highest completion value handed out by submit()
     */
    [[nodiscard]] uint64_t submitted_value() const { return m_submitted; }

    /**
     * @brief Check whether a submission has completed
     */
    [[nodiscard]] bool is_complete(uint64_t value) const { return value <= completed_value(); }

    /**
     * @brief Block until a submission has completed
     * @param value Completion value returned by submit(); 0 returns immediately
     * @param timeout_ns Timeout in nanoseconds
     */
    void wait(uint64_t value, uint64_t timeout_ns = UINT64_MAX) const;

    /**
     * @brief The timeline semaphore signaled by every submission
     */
    [[nodiscard]] vk::Semaphore get_timeline() const { return m_timeline; }

private:
    struct RetiredCommand {
        vk::CommandBuffer command_buffer;
        uint64_t value {};
    };

    vk::Device m_device;
    vk::CommandPool m_command_pool;
    vk::CommandPool m_compute_command_pool;
    uint32_t m_graphics_queue_family {};
    uint32_t m_compute_queue_family {};

    vk::Semaphore m_timeline {};
    uint64_t m_submitted {};

    vk::CommandBuffer m_deferred_cmd {};
    bool m_deferred_pending {};

    std::vector<vk::CommandBuffer> m_allocated_buffers;
    std::vector<vk::CommandBuffer> m_compute_allocated_buffers;
    std::vector<RetiredCommand> m_retired;
};

} // namespace MayaFlux::Core
