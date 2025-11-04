#pragma once

namespace MayaFlux::Registry::Service {

/**
 * @brief Backend compute shader and pipeline service interface
 *
 * Defines GPU compute operations for data-parallel processing.
 * Supports shader compilation, descriptor management, and compute dispatch.
 *
 * Typical Workflow:
 * 1. create_shader_module() -> ShaderHandle
 * 2. create_descriptor_manager() -> DescriptorManagerHandle
 * 3. create_descriptor_layout() -> DescriptorLayoutHandle
 * 4. create_compute_pipeline() -> PipelineHandle
 * 5. dispatch_compute() -> execute shader
 * 6. cleanup_resource() -> cleanup when done
 */
struct MAYAFLUX_API ComputeService {
    /**
     * @brief Create a shader module from compiled shader code
     * @param spirv_path Path to shader file (SPIR-V or backend-specific format)
     * @param stage Shader stage flags (compute, vertex, fragment, etc.)
     * @return Opaque shader module handle
     *
     * For Vulkan: SPIR-V bytecode (.spv files)
     * For OpenGL: GLSL source code
     * For Metal: .metallib or source
     * For DirectX: DXIL or HLSL
     */
    std::function<std::shared_ptr<void>(const std::string&, uint32_t)> create_shader_module;

    /**
     * @brief Create a descriptor set manager/pool
     * @param pool_size Maximum number of descriptor sets to allocate
     * @return Opaque descriptor manager handle
     *
     * Manages allocation of descriptor sets. Pool size determines
     * maximum concurrent allocations before reset/reallocation needed.
     */
    std::function<std::shared_ptr<void>(uint32_t)> create_descriptor_manager;

    /**
     * @brief Create a descriptor set layout
     * @param manager Descriptor manager handle
     * @param bindings Vector of (binding index, descriptor type) pairs
     * @return Opaque descriptor layout handle
     *
     * Defines the structure of shader resource bindings.
     * Descriptor type values are backend-specific:
     * - Vulkan: VK_DESCRIPTOR_TYPE_* enum values
     * - OpenGL: GL_UNIFORM_BUFFER, GL_SHADER_STORAGE_BUFFER, etc.
     *
     * Example bindings:
     *   {0, UNIFORM_BUFFER},    // binding 0: uniform buffer
     *   {1, STORAGE_BUFFER},    // binding 1: storage buffer
     *   {2, COMBINED_IMAGE_SAMPLER}  // binding 2: texture
     */
    std::function<void*(const std::shared_ptr<void>&, const std::vector<std::pair<uint32_t, uint32_t>>&)> create_descriptor_layout;

    /**
     * @brief Create a compute pipeline
     * @param shader Compute shader module handle
     * @param layouts Vector of descriptor set layout handles
     * @param push_constant_size Size of push constants in bytes (0 if unused)
     * @return Opaque compute pipeline handle
     *
     * Combines shader code with resource layout to create executable pipeline.
     * Multiple descriptor set layouts enable logical grouping of resources.
     * Push constants provide fast, small data updates without descriptor sets.
     */
    std::function<std::shared_ptr<void>(const std::shared_ptr<void>&, const std::vector<void*>&, uint32_t)> create_compute_pipeline;

    /**
     * @brief Dispatch a compute shader execution
     * @param pipeline Compute pipeline to execute
     * @param group_count_x Number of workgroups in X dimension
     * @param group_count_y Number of workgroups in Y dimension
     * @param group_count_z Number of workgroups in Z dimension
     *
     * Executes compute shader with specified workgroup counts.
     * Total invocations = (group_count_x * local_size_x) *
     *                    (group_count_y * local_size_y) *
     *                    (group_count_z * local_size_z)
     *
     * Must be called within a command recording context
     * (execute_immediate or record_deferred from IBufferService).
     */
    std::function<void(const std::shared_ptr<void>&, uint32_t, uint32_t, uint32_t)> dispatch_compute;

    /**
     * @brief Cleanup a compute resource
     * @param resource Opaque resource handle (shader, pipeline, layout, etc.)
     *
     * Safe to call with any compute resource type. Implementation determines
     * actual resource type and performs appropriate cleanup. No-op for
     * invalid/null handles.
     */
    std::function<void(const std::shared_ptr<void>&)> cleanup_resource;
};

} // namespace MayaFlux::Registry::Services
