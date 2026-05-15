#pragma once

#include <vulkan/vulkan.hpp>

namespace MayaFlux::Core {

/**
 * @class VKCommandManager
 * @brief Manages Vulkan command pools and command buffers
 *
 * Handles command buffer allocation, recording, and submission.
 * Creates per-thread command pools for thread safety.
 */
class VKCommandManager {
public:
    VKCommandManager() = default;
    ~VKCommandManager();

    /**
     * @brief Initialize command manager
     * @param device Logical device
     * @param graphics_queue_family Graphics queue family index
     * @param compute_queue_family Compute queue family index (optional, for compute operations)
     * @return True if initialization succeeded
     */
    // bool initialize(vk::Device device, uint32_t graphics_queue_family);
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
     * @brief End and submit single-time command
     * @param command_buffer Command buffer to submit
     * @param queue Queue to submit to
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
     * @brief End and submit single-time command for compute operations
     * @param command_buffer Command buffer to submit
     * @param queue Queue to submit to
     */
    [[nodiscard]] bool has_deferred_commands() const { return m_deferred_pending; }

    /**
     * @brief Record deferred commands for later submission
     * @param recorder Command recording function that takes a command buffer
     */
    void record_deferred_commands(const std::function<void(vk::CommandBuffer)>& recorder);

    /**
     * @brief Get the deferred command buffer (if any)
     * @return Deferred command buffer, or null if none pending
     */
    [[nodiscard]] vk::CommandBuffer get_deferred_cmd() const { return m_deferred_cmd; }

    /**
     * @brief Reset deferred command buffer and clear pending state
     */
    void reset_deferred();

private:
    vk::Device m_device;
    vk::CommandPool m_command_pool;
    vk::CommandPool m_compute_command_pool;
    uint32_t m_graphics_queue_family {};
    uint32_t m_compute_queue_family {};

    vk::CommandBuffer m_deferred_cmd {};
    bool m_deferred_pending {};

    std::vector<vk::CommandBuffer> m_allocated_buffers;
    std::vector<vk::CommandBuffer> m_compute_allocated_buffers;
};

} // namespace MayaFlux::Core
