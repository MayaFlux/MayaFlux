#pragma once

#include <vulkan/vulkan.hpp>

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <span>
#include <vector>

namespace MayaFlux::Core {

/// Upper bound on distinct queues: graphics, compute and transfer.
inline constexpr uint32_t k_max_timelines = 3;

/**
 * @struct TimelinePoint
 * @brief Completion token of one submission: a value on one queue's timeline
 *
 * A value of 0 means no submission (a failed submit or nothing pending).
 */
struct TimelinePoint {
    uint32_t timeline {};
    uint64_t value {};

    [[nodiscard]] explicit operator bool() const { return value != 0; }
};

/**
 * @struct TimelineSnapshot
 * @brief Values on every timeline that must complete, indexed by timeline
 *
 * A zero entry means no dependency on that timeline. A single point converts
 * implicitly, so APIs that take a snapshot also accept one point.
 */
struct TimelineSnapshot {
    std::array<uint64_t, k_max_timelines> values {};

    TimelineSnapshot() = default;

    TimelineSnapshot(TimelinePoint point)
    {
        if (point.timeline < k_max_timelines) {
            values[point.timeline] = point.value;
        }
    }
};

/**
 * @class VKCommandManager
 * @brief Owns command pools, queue submission and command buffer retirement
 *
 * Each distinct queue has its own timeline semaphore. Queues that alias the
 * same handle (compute or transfer on the graphics queue) share one timeline,
 * so a device with a single queue has a single timeline. Every submission
 * through submit() signals its queue's timeline with a strictly increasing
 * value, and that point is the completion token: command buffers are retired
 * against it and callers wait on it instead of draining a queue.
 *
 * Values on different timelines are unordered. Completion of work spread over
 * several queues is expressed as a TimelineSnapshot.
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
     * @param graphics_queue Graphics queue
     * @param compute_queue Compute queue, may alias the graphics queue
     * @param transfer_queue Transfer queue, may alias another queue
     * @return True if initialization succeeded
     *
     * One timeline is created per distinct queue handle.
     */
    bool initialize(
        vk::Device device,
        uint32_t graphics_queue_family,
        uint32_t compute_queue_family,
        vk::Queue graphics_queue,
        vk::Queue compute_queue,
        vk::Queue transfer_queue);

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
     * @return Point of the submission, or an empty point when nothing was pending or the submit failed
     */
    TimelinePoint flush_deferred(vk::Queue queue);

    /**
     * @brief Submit command buffers and signal the queue's timeline
     * @param queue Queue to submit to; must be one passed to initialize()
     * @param commands Command buffers, executed in order
     * @param waits Semaphores to wait on; timeline waits carry their value
     * @param signals Additional semaphores to signal; binary ones carry value 0
     * @param fence Optional fence signaled with the submission
     * @return Point signaled when the submission completes, or an empty point if the submit failed
     *
     * The point orders strictly with every other submission to the same queue
     * and is unordered against submissions to other queues.
     */
    TimelinePoint submit(
        vk::Queue queue,
        std::span<const vk::CommandBuffer> commands,
        std::span<const vk::SemaphoreSubmitInfo> waits = {},
        std::span<const vk::SemaphoreSubmitInfo> signals = {},
        vk::Fence fence = {});

    /**
     * @brief Free a command buffer once its submission has completed
     * @param command_buffer Command buffer to free
     * @param completion Point returned by submit(), or a snapshot of several
     */
    void retire(vk::CommandBuffer command_buffer, const TimelineSnapshot& completion);

    /**
     * @brief Run a release action once a submission has completed
     * @param release Action that frees whatever the submission may still read; runs immediately when nothing is pending
     * @param completion Point returned by submit(), or a snapshot of several
     *
     * Actions run from collect(), outside the retire lists, so they may call back
     * into the manager. They are dropped without running at cleanup().
     */
    void retire_resource(std::function<void()> release, const TimelineSnapshot& completion);

    /**
     * @brief Free every retired command buffer and run every release action whose submission has completed
     */
    void collect();

    /**
     * @brief Number of timelines, one per distinct queue
     */
    [[nodiscard]] uint32_t timeline_count() const { return static_cast<uint32_t>(m_timelines.size()); }

    /**
     * @brief Index of the timeline signaled by a queue
     * @return Timeline index, or timeline_count() if the queue was not passed to initialize()
     */
    [[nodiscard]] uint32_t timeline_index(vk::Queue queue) const;

    /**
     * @brief Highest completion value reached on a timeline
     */
    [[nodiscard]] uint64_t completed_value(uint32_t timeline) const;

    /**
     * @brief Highest completion value handed out by submit() on a timeline
     */
    [[nodiscard]] uint64_t submitted_value(uint32_t timeline) const;

    /**
     * @brief Every submission made so far, on every timeline
     */
    [[nodiscard]] TimelineSnapshot submitted() const;

    /**
     * @brief Check whether everything in a snapshot has completed
     */
    [[nodiscard]] bool is_complete(const TimelineSnapshot& completion) const;

    /**
     * @brief Block until everything in a snapshot has completed
     * @param completion Point returned by submit(), or a snapshot of several; empty returns immediately
     * @param timeout_ns Timeout in nanoseconds
     *
     * Waits on all involved timelines in one call.
     */
    void wait(const TimelineSnapshot& completion, uint64_t timeout_ns = UINT64_MAX) const;

    /**
     * @brief The timeline semaphore signaled by a queue's submissions
     */
    [[nodiscard]] vk::Semaphore get_timeline(vk::Queue queue) const;

private:
    struct QueueTimeline {
        vk::Queue queue;
        vk::Semaphore semaphore;
        std::atomic<uint64_t> submitted { 0 };
    };

    struct RetiredCommand {
        vk::CommandBuffer command_buffer;
        TimelineSnapshot completion;
    };

    struct RetiredResource {
        std::function<void()> release;
        TimelineSnapshot completion;
    };

    bool create_timeline(vk::Queue queue);

    vk::Device m_device;
    vk::CommandPool m_command_pool;
    vk::CommandPool m_compute_command_pool;
    uint32_t m_graphics_queue_family {};
    uint32_t m_compute_queue_family {};

    std::vector<std::unique_ptr<QueueTimeline>> m_timelines;

    vk::CommandBuffer m_deferred_cmd {};
    bool m_deferred_pending {};

    std::vector<vk::CommandBuffer> m_allocated_buffers;
    std::vector<vk::CommandBuffer> m_compute_allocated_buffers;
    std::vector<RetiredCommand> m_retired;
    std::vector<RetiredResource> m_retired_resources;
};

} // namespace MayaFlux::Core
