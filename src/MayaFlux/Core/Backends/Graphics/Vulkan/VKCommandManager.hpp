#pragma once

#include "VKTimeline.hpp"

#include "MayaFlux/Transitive/Memory/SweepList.hpp"

#include <vulkan/vulkan.hpp>

namespace MayaFlux::Core {

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
 * not call submit() concurrently. retire(), retire_resource() and collect() may
 * be called from any thread: the retire lists are lock-free Memory::SweepList
 * instances.
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
     * into the manager. They run on the thread that collects, which is not
     * necessarily the one that retired them. They are dropped without running at cleanup().
     */
    void retire_resource(std::function<void()> release, const TimelineSnapshot& completion);

    /**
     * @brief Free every retired command buffer and run every release action whose submission has completed
     *
     * Entries that have not completed stay for the next call.
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
     * @brief Record a submission whose output later work may consume
     * @param point Point returned by submit(); an empty point is ignored
     *
     * Keeps the highest value per timeline. Safe from any thread. Never cleared:
     * consumers skip values that have completed.
     */
    void note_produced(TimelinePoint point);

    /**
     * @brief Everything noted by note_produced() so far
     */
    [[nodiscard]] TimelineSnapshot produced() const;

    /**
     * @brief Waits for produced work that has not completed yet
     * @param stage Pipeline stages of the consuming submission that read the output
     * @return One timeline wait per timeline with incomplete produced work; empty when everything is done
     */
    [[nodiscard]] std::vector<vk::SemaphoreSubmitInfo> produced_waits(vk::PipelineStageFlags2 stage) const;

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
    std::array<std::atomic<uint64_t>, k_max_timelines> m_produced {};

    vk::CommandBuffer m_deferred_cmd {};
    bool m_deferred_pending {};

    std::vector<vk::CommandBuffer> m_allocated_buffers;
    std::vector<vk::CommandBuffer> m_compute_allocated_buffers;
    Memory::SweepList<RetiredCommand> m_retired;
    Memory::SweepList<RetiredResource> m_retired_resources;
};

} // namespace MayaFlux::Core
