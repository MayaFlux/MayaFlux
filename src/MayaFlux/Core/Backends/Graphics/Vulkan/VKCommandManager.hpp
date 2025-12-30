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
     * @return True if initialization succeeded
     */
    bool initialize(vk::Device device, uint32_t graphics_queue_family);

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

private:
    vk::Device m_device;
    vk::CommandPool m_command_pool;
    uint32_t m_graphics_queue_family = 0;

    std::vector<vk::CommandBuffer> m_allocated_buffers;
};

} // namespace MayaFlux::Core
