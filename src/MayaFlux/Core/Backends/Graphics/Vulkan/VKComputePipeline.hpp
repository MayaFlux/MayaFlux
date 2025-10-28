#pragma once

#include "VKDescriptorManager.hpp"
#include "VKShaderModule.hpp"

namespace MayaFlux::Core {

/**
 * @struct PushConstantRange
 * @brief Defines a push constant range for pipeline creation
 *
 * Push constants are small amounts of data (<128 bytes typically) that can be
 * updated very efficiently without descriptor sets. Useful for per-dispatch
 * parameters like iteration counts, scaling factors, etc.
 */
struct PushConstantRange {
    vk::ShaderStageFlags stage_flags; ///< Which shader stages access this
    uint32_t offset; ///< Offset in push constant block (bytes)
    uint32_t size; ///< Size of push constant data (bytes)

    PushConstantRange(vk::ShaderStageFlags stages, uint32_t offset_, uint32_t size_)
        : stage_flags(stages)
        , offset(offset_)
        , size(size_)
    {
    }
};

/**
 * @struct ComputePipelineConfig
 * @brief Configuration for creating a compute pipeline
 *
 * Defines all parameters needed to create a compute pipeline:
 * - Shader module
 * - Descriptor set layouts (resource bindings)
 * - Push constants (small uniform data)
 * - Specialization constants (compile-time shader parameters)
 */
struct ComputePipelineConfig {
    VKShaderModule* shader = nullptr; ///< Compute shader
    std::vector<vk::DescriptorSetLayout> set_layouts; ///< Descriptor layouts
    std::vector<PushConstantRange> push_constants; ///< Push constant ranges

    vk::PipelineCache cache = nullptr;

    void add_descriptor_set_layout(vk::DescriptorSetLayout layout)
    {
        set_layouts.push_back(layout);
    }

    void add_push_constant(vk::ShaderStageFlags stages, uint32_t size, uint32_t offset = 0)
    {
        push_constants.emplace_back(stages, offset, size);
    }
};

/**
 * @class VKComputePipeline
 * @brief Wrapper for Vulkan compute pipeline with simplified interface
 *
 * Responsibilities:
 * - Create compute pipeline from shader and configuration
 * - Manage pipeline layout (descriptor sets + push constants)
 * - Bind pipeline to command buffer
 * - Bind descriptor sets
 * - Update push constants
 * - Dispatch compute workgroups
 * - Handle pipeline recreation (for hot-reload)
 *
 * Does NOT handle:
 * - Shader compilation (that's VKShaderModule)
 * - Descriptor allocation (that's VKDescriptorManager)
 * - Command buffer management (that's VKCommandManager)
 * - Synchronization (that's the caller's responsibility)
 *
 * Design:
 * - Immutable after creation (recreation required for changes)
 * - Lightweight wrapper around vk::Pipeline
 * - Can be used directly or via VKComputeProcessor
 *
 * Thread Safety:
 * - NOT thread-safe - caller must synchronize access
 * - Safe to use same pipeline on different command buffers (sequentially)
 */
class VKComputePipeline {
public:
    VKComputePipeline() = default;
    ~VKComputePipeline();

    VKComputePipeline(const VKComputePipeline&) = delete;
    VKComputePipeline& operator=(const VKComputePipeline&) = delete;
    VKComputePipeline(VKComputePipeline&&) noexcept;
    VKComputePipeline& operator=(VKComputePipeline&&) noexcept;

    /**
     * @brief Create compute pipeline from configuration
     * @param device Logical device
     * @param config Pipeline configuration (shader, layouts, push constants)
     * @return true if creation succeeded
     *
     * Creates:
     * 1. Pipeline layout (from descriptor set layouts + push constants)
     * 2. Compute pipeline (from shader + layout)
     *
     * If config.cache is provided, pipeline creation will be faster on
     * subsequent runs (cache can be saved/loaded between sessions).
     *
     * Example:
     *   ComputePipelineConfig config;
     *   config.shader = &my_shader;
     *   config.add_descriptor_set_layout(layout);
     *   config.add_push_constant(vk::ShaderStageFlagBits::eCompute, 16);
     *
     *   VKComputePipeline pipeline;
     *   pipeline.create(device, config);
     */
    bool create(vk::Device device, const ComputePipelineConfig& config);

    /**
     * @brief Cleanup pipeline resources
     * @param device Logical device (must match creation device)
     *
     * Destroys pipeline and pipeline layout. Safe to call multiple times.
     */
    void cleanup(vk::Device device);

    /**
     * @brief Check if pipeline is valid
     * @return true if pipeline was successfully created
     */
    [[nodiscard]] bool is_valid() const { return m_pipeline != nullptr; }

    /**
     * @brief Get raw Vulkan pipeline handle
     * @return vk::Pipeline handle
     */
    [[nodiscard]] vk::Pipeline get() const { return m_pipeline; }

    /**
     * @brief Get pipeline layout handle
     * @return vk::PipelineLayout handle
     */
    [[nodiscard]] vk::PipelineLayout get_layout() const { return m_layout; }

    /**
     * @brief Bind pipeline to command buffer
     * @param cmd Command buffer
     *
     * Makes this pipeline the active compute pipeline for subsequent
     * dispatch commands. Must be called before bind_descriptor_sets()
     * or dispatch().
     *
     * Example:
     *   pipeline.bind(cmd);
     *   pipeline.bind_descriptor_sets(cmd, {desc_set});
     *   pipeline.dispatch(cmd, 256, 1, 1);
     */
    void bind(vk::CommandBuffer cmd) const;

    /**
     * @brief Bind descriptor sets to pipeline
     * @param cmd Command buffer
     * @param descriptor_sets Vector of descriptor sets to bind
     * @param first_set First set index (default 0 for set=0)
     * @param dynamic_offsets Optional dynamic offsets for dynamic buffers
     *
     * Binds descriptor sets for use by the shader. Set indices must match
     * the layout(set=X) declarations in the shader.
     *
     * Example:
     *   // Shader has: layout(set=0, binding=0) buffer Data { ... };
     *   pipeline.bind_descriptor_sets(cmd, {desc_set_0});
     *
     *   // Multiple sets: layout(set=0, ...) and layout(set=1, ...)
     *   pipeline.bind_descriptor_sets(cmd, {desc_set_0, desc_set_1});
     */
    void bind_descriptor_sets(
        vk::CommandBuffer cmd,
        const std::vector<vk::DescriptorSet>& descriptor_sets,
        uint32_t first_set = 0,
        const std::vector<uint32_t>& dynamic_offsets = {}) const;

    /**
     * @brief Update push constants
     * @param cmd Command buffer
     * @param stage_flags Shader stages that will access this data
     * @param offset Offset in push constant block (bytes)
     * @param size Size of data (bytes)
     * @param data Pointer to data to copy
     *
     * Updates push constant data that will be visible to the shader.
     * More efficient than descriptor sets for small, frequently-changing data.
     *
     * Example:
     *   struct Params { float scale; uint32_t iterations; };
     *   Params params = {2.0f, 100};
     *   pipeline.push_constants(cmd, vk::ShaderStageFlagBits::eCompute,
     *                          0, sizeof(params), &params);
     */
    void push_constants(
        vk::CommandBuffer cmd,
        vk::ShaderStageFlags stage_flags,
        uint32_t offset,
        uint32_t size,
        const void* data) const;

    /**
     * @brief Dispatch compute workgroups
     * @param cmd Command buffer
     * @param group_count_x Number of workgroups in X dimension
     * @param group_count_y Number of workgroups in Y dimension
     * @param group_count_z Number of workgroups in Z dimension
     *
     * Executes the compute shader with the specified number of workgroups.
     * Total invocations = group_count * local_size (from shader).
     *
     * Example:
     *   // Shader: layout(local_size_x = 256) in;
     *   // Process 1M elements: 1,000,000 / 256 = 3,907 workgroups
     *   pipeline.dispatch(cmd, 3907, 1, 1);
     *
     * Must be called after bind() and bind_descriptor_sets().
     */
    void dispatch(
        vk::CommandBuffer cmd,
        uint32_t group_count_x,
        uint32_t group_count_y,
        uint32_t group_count_z) const;

    /**
     * @brief Dispatch compute workgroups with automatic calculation
     * @param cmd Command buffer
     * @param element_count Total number of elements to process
     * @param local_size_x Workgroup size (from shader local_size_x)
     *
     * Convenience function that calculates workgroup count automatically.
     * Rounds up to ensure all elements are processed.
     *
     * Example:
     *   // Shader: layout(local_size_x = 256) in;
     *   pipeline.dispatch_1d(cmd, 1'000'000, 256);
     *   // Internally: dispatch(cmd, ceil(1M / 256), 1, 1)
     */
    void dispatch_1d(
        vk::CommandBuffer cmd,
        uint32_t element_count,
        uint32_t local_size_x) const;

    /**
     * @brief Dispatch compute workgroups in 2D with automatic calculation
     * @param cmd Command buffer
     * @param width_elements Width in elements
     * @param height_elements Height in elements
     * @param local_size_x Workgroup width (shader local_size_x)
     * @param local_size_y Workgroup height (shader local_size_y)
     *
     * Convenience for 2D operations (image processing, etc.)
     *
     * Example:
     *   // Process 1920x1080 image with 16x16 workgroups
     *   pipeline.dispatch_2d(cmd, 1920, 1080, 16, 16);
     */
    void dispatch_2d(
        vk::CommandBuffer cmd,
        uint32_t width_elements,
        uint32_t height_elements,
        uint32_t local_size_x,
        uint32_t local_size_y) const;

    /**
     * @brief Dispatch compute workgroups in 3D with automatic calculation
     * @param cmd Command buffer
     * @param width_elements Width in elements
     * @param height_elements Height in elements
     * @param depth_elements Depth in elements
     * @param local_size_x Workgroup width
     * @param local_size_y Workgroup height
     * @param local_size_z Workgroup depth
     */
    void dispatch_3d(
        vk::CommandBuffer cmd,
        uint32_t width_elements,
        uint32_t height_elements,
        uint32_t depth_elements,
        uint32_t local_size_x,
        uint32_t local_size_y,
        uint32_t local_size_z) const;

    /**
     * @brief Get shader workgroup size from reflection
     * @return Optional array of [x, y, z] workgroup size
     *
     * Extracts local_size_x/y/z from shader if reflection was enabled.
     * Useful for automatic dispatch calculation.
     *
     * Example:
     *   auto workgroup = pipeline.get_workgroup_size();
     *   if (workgroup) {
     *       pipeline.dispatch_1d(cmd, element_count, (*workgroup)[0]);
     *   }
     */
    [[nodiscard]] std::optional<std::array<uint32_t, 3>> get_workgroup_size() const;

private:
    vk::Pipeline m_pipeline = nullptr;
    vk::PipelineLayout m_layout = nullptr;

    // Cache shader reflection for workgroup size queries
    std::optional<std::array<uint32_t, 3>> m_workgroup_size;

    /**
     * @brief Create pipeline layout from configuration
     * @param device Logical device
     * @param config Pipeline configuration
     * @return Pipeline layout handle, or null on failure
     */
    vk::PipelineLayout create_pipeline_layout(
        vk::Device device,
        const ComputePipelineConfig& config);

    /**
     * @brief Calculate number of workgroups needed
     * @param element_count Total elements to process
     * @param workgroup_size Elements per workgroup
     * @return Number of workgroups (rounded up)
     */
    static uint32_t calculate_workgroups(uint32_t element_count, uint32_t workgroup_size);
};

} // namespace MayaFlux::Core
