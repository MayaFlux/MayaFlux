#pragma once

#include <vulkan/vulkan.hpp>

namespace MayaFlux::Core {
class VulkanBackend;
class VKShaderModule;
class VKComputePipeline;
class VKDescriptorManager;
}

namespace MayaFlux::Portal::Graphics {

class ComputePress;
class RenderFlow;

using ShaderID = uint64_t;
using DescriptorSetID = uint64_t;
using CommandBufferID = uint64_t;
using FenceID = uint64_t;
using SemaphoreID = uint64_t;

constexpr ShaderID INVALID_SHADER = 0;
constexpr DescriptorSetID INVALID_DESCRIPTOR_SET = 0;
constexpr CommandBufferID INVALID_COMMAND_BUFFER = 0;
constexpr FenceID INVALID_FENCE = 0;
constexpr SemaphoreID INVALID_SEMAPHORE = 0;

/**
 * @enum ShaderStage
 * @brief User-friendly shader stage enum
 *
 * Abstracts vk::ShaderStageFlagBits for Portal API convenience.
 * Maps directly to Vulkan stages internally.
 */
enum class ShaderStage : uint8_t {
    COMPUTE,
    VERTEX,
    FRAGMENT,
    GEOMETRY,
    TESS_CONTROL,
    TESS_EVALUATION
};

/**
 * @struct ShaderCompilerConfig
 * @brief Configuration for shader compilation
 */
struct ShaderCompilerConfig {
    bool enable_optimization = true;
    bool enable_debug_info = false; ///< Include debug symbols (line numbers, variable names)
    bool enable_reflection = true; ///< Extract descriptor bindings and metadata
    bool enable_validation = true; ///< Validate SPIR-V after compilation
    std::vector<std::string> include_directories; ///< Paths for #include resolution
    std::unordered_map<std::string, std::string> defines; ///< Preprocessor macros

    ShaderCompilerConfig() = default;
};

/**
 * @struct ShaderSource
 * @brief Shader source descriptor for compilation
 */
struct ShaderSource {
    std::string content; ///< Shader source code or SPIR-V path
    ShaderStage stage;
    std::string entry_point = "main";

    enum class SourceType : uint8_t {
        GLSL_STRING, ///< In-memory GLSL source
        GLSL_FILE, ///< Path to .comp/.vert/.frag/etc
        SPIRV_FILE ///< Path to .spv file
    } type
        = SourceType::GLSL_FILE;

    ShaderSource() = default;
    ShaderSource(std::string content_, ShaderStage stage_, SourceType type_)
        : content(std::move(content_))
        , stage(stage_)
        , type(type_)
    {
    }
};

/**
 * @struct DescriptorBindingInfo
 * @brief Extracted descriptor binding information from shader reflection
 */
struct DescriptorBindingInfo {
    uint32_t set;
    uint32_t binding;
    vk::DescriptorType type;
    std::string name;
};

/**
 * @struct DescriptorBindingConfig
 * @brief Portal-level descriptor binding configuration
 */
struct DescriptorBindingConfig {
    uint32_t set = 0;
    uint32_t binding = 0;
    vk::DescriptorType type = vk::DescriptorType::eStorageBuffer;

    DescriptorBindingConfig() = default;
    DescriptorBindingConfig(uint32_t s, uint32_t b, vk::DescriptorType t = vk::DescriptorType::eStorageBuffer)
        : set(s)
        , binding(b)
        , type(t)
    {
    }
};

/**
 * @struct PushConstantRangeInfo
 * @brief Extracted push constant range from shader reflection
 */
struct PushConstantRangeInfo {
    uint32_t offset;
    uint32_t size;
};

/**
 * @struct ShaderReflectionInfo
 * @brief Extracted reflection information from compiled shader
 */
struct ShaderReflectionInfo {
    ShaderStage stage;
    std::string entry_point;
    std::optional<std::array<uint32_t, 3>> workgroup_size;
    std::vector<DescriptorBindingInfo> descriptor_bindings;
    std::vector<PushConstantRangeInfo> push_constant_ranges;
};

/**
 * @class ShaderFoundry
 * @brief Portal-level shader compilation and caching
 *
 * ShaderFoundry is a thin glue layer that:
 * - Wraps Core::VKShaderModule for convenient shader creation
 * - Provides caching to avoid redundant compilation
 * - Supports hot-reload workflows (watch files, recompile)
 * - Returns VKShaderModule directly for use in pipelines
 *
 * Design Philosophy:
 * - Manages compilation, NOT execution (that's Pipeline/Compute)
 * - Returns VKShaderModule directly (no wrapping)
 * - Simple, focused API aligned with VKShaderModule capabilities
 * - Integrates with existing Core shader infrastructure
 *
 * Consumers:
 * - VKBufferProcessor subclasses (compute shaders)
 * - Future Yantra::ComputePipeline (compute shaders)
 * - Future Yantra::RenderPipeline (graphics shaders)
 * - Future DataProcessors (image processing shaders)
 *
 * Usage:
 *   auto& compiler = Portal::Graphics::ShaderFoundry::instance();
 *
 *   // Compile from file
 *   auto shader = compiler.compile_from_file("shaders/my_kernel.comp");
 *
 *   // Compile from string
 *   auto shader = compiler.compile_from_source(glsl_code, ShaderStage::COMPUTE);
 *
 *   // Use in pipeline
 *   my_buffer_processor->set_shader(shader);
 *   my_compute_pipeline->set_shader(shader);
 */
class MAYAFLUX_API ShaderFoundry {
public:
    enum class CommandBufferType : uint8_t {
        GRAPHICS,
        COMPUTE,
        TRANSFER
    };

private:
    struct DescriptorSetState {
        vk::DescriptorSet descriptor_set;
    };

    struct CommandBufferState {
        vk::CommandBuffer cmd;
        CommandBufferType type;
        bool is_active;
        vk::QueryPool timestamp_pool;
        std::unordered_map<std::string, uint32_t> timestamp_queries;
    };

    struct FenceState {
        vk::Fence fence;
        bool signaled;
    };

    struct SemaphoreState {
        vk::Semaphore semaphore;
    };

    struct ShaderState {
        std::shared_ptr<Core::VKShaderModule> module;
        std::string filepath;
        ShaderStage stage;
        std::string entry_point;
    };

public:
    static ShaderFoundry& instance()
    {
        static ShaderFoundry compiler;
        return compiler;
    }

    ShaderFoundry(const ShaderFoundry&) = delete;
    ShaderFoundry& operator=(const ShaderFoundry&) = delete;
    ShaderFoundry(ShaderFoundry&&) noexcept = delete;
    ShaderFoundry& operator=(ShaderFoundry&&) noexcept = delete;

    /**
     * @brief Initialize shader compiler
     * @param backend Shared pointer to VulkanBackend
     * @param config Compiler configuration
     * @return True if initialization succeeded
     *
     * Must be called before compiling any shaders.
     */
    bool initialize(
        const std::shared_ptr<Core::VulkanBackend>& backend,
        const ShaderCompilerConfig& config = {});

    /**
     * @brief Shutdown and cleanup
     *
     * Destroys all cached shader modules.
     * Safe to call multiple times.
     */
    void shutdown();

    /**
     * @brief Check if compiler is initialized
     */
    [[nodiscard]] bool is_initialized() const { return m_backend != nullptr; }

    //==========================================================================
    // Shader Compilation - Primary API
    //==========================================================================

    /**
     * @brief Compile shader from file path
     * @param filepath Path to shader file (.comp, .vert, .frag, .spv, etc.)
     * @param stage Optional stage override (auto-detected from extension if omitted)
     * @param entry_point Entry point function name (default: "main")
     * @return ID of compiled shader module, or INVALID_SHADER on failure
     *
     * Supports:
     * - GLSL files: .comp, .vert, .frag, .geom, .tesc, .tese
     * - SPIR-V files: .spv
     *
     * Stage auto-detection:
     *   .comp → COMPUTE
     *   .vert → VERTEX
     *   .frag → FRAGMENT
     *   .geom → GEOMETRY
     *   .tesc → TESS_CONTROL
     *   .tese → TESS_EVALUATION
     *
     * Caching: If the same file is compiled twice, returns cached version.
     * Use invalidate_cache() or hot_reload() to force recompilation.
     */
    ShaderID load_shader(
        const std::string& filepath,
        std::optional<ShaderStage> stage = std::nullopt,
        const std::string& entry_point = "main");

    /**
     * @brief Compile shader from GLSL source string
     * @param source GLSL source code
     * @param stage Shader stage (COMPUTE, VERTEX, FRAGMENT, etc.)
     * @param entry_point Entry point function name (default: "main")
     * @return ID of compiled shader module, or INVALID_SHADER on failure
     *
     * Does NOT cache by default (source strings are transient).
     * Use compile_from_source_cached() if you want caching.
     */
    ShaderID load_shader_from_source(
        const std::string& source,
        ShaderStage stage,
        const std::string& entry_point = "main");

    /**
     * @brief Compile shader from GLSL source with caching
     * @param source GLSL source code
     * @param stage Shader stage
     * @param cache_key Unique key for caching (e.g., "my_kernel_v1")
     * @param entry_point Entry point function name
     * @return ID of compiled shader module, or INVALID_SHADER on failure
     *
     * Caches compiled shader under cache_key.
     * Useful for generated shaders or frequently reused inline shaders.
     */
    ShaderID load_shader_from_source_cached(
        const std::string& source,
        ShaderStage stage,
        const std::string& cache_key,
        const std::string& entry_point = "main");

    /**
     * @brief Compile shader from SPIR-V file
     * @param spirv_path Path to .spv file
     * @param stage Shader stage (must specify, cannot auto-detect from SPIR-V)
     * @param entry_point Entry point function name (default: "main")
     * @return ID of compiled shader module, or INVALID_SHADER on failure
     *
     * For pre-compiled SPIR-V binaries.
     * Caches by file path like compile_from_file().
     */
    ShaderID load_shader_from_spirv(
        const std::string& spirv_path,
        ShaderStage stage,
        const std::string& entry_point = "main");

    /**
     * @brief Hot-reload shader (returns new ID)
     */
    ShaderID reload_shader(const std::string& filepath);

    /**
     * @brief Destroy shader (cleanup internal state)
     */
    void destroy_shader(ShaderID shader_id);

    /**
     * @brief Compile shader from ShaderSource descriptor
     * @param shader_source Shader descriptor (path or source + type + stage)
     * @return Compiled shader module, or nullptr on failure
     *
     * Unified interface that dispatches to appropriate compile method.
     * Useful for abstracted shader loading pipelines.
     */
    std::shared_ptr<Core::VKShaderModule> compile(const ShaderSource& shader_source);

    //==========================================================================
    // Shader Introspection
    //==========================================================================

    /**
     * @brief Get reflection info for compiled shader
     * @param shader_id ID of compiled shader
     * @return Reflection information
     *
     * Extracted during compilation if enabled in config.
     * Includes descriptor bindings, push constant ranges, workgroup size, etc.
     */
    ShaderReflectionInfo get_shader_reflection(ShaderID shader_id);

    /**
     * @brief Get shader stage for compiled shader
     * @param shader_id ID of compiled shader
     * @return Shader stage (COMPUTE, VERTEX, FRAGMENT, etc.)
     */
    ShaderStage get_shader_stage(ShaderID shader_id);

    /**
     * @brief Get entry point name for compiled shader
     * @param shader_id ID of compiled shader
     * @return Entry point function name
     */
    std::string get_shader_entry_point(ShaderID shader_id);

    //==========================================================================
    // Hot-Reload Support
    //==========================================================================

    /**
     * @brief Invalidate cache for specific shader
     * @param cache_key File path or cache key
     *
     * Forces next compilation to recompile from source.
     * Useful for hot-reload workflows.
     */
    void invalidate_cache(const std::string& cache_key);

    /**
     * @brief Invalidate entire shader cache
     *
     * Forces all subsequent compilations to recompile.
     * Does NOT destroy existing shader modules (they remain valid).
     */
    void clear_cache();

    /**
     * @brief Hot-reload a shader from file
     * @param filepath Path to shader file
     * @return Recompiled shader module, or nullptr on failure
     *
     * Convenience method: invalidate_cache() + compile_from_file().
     * Returns new shader module; consumers must update references.
     */
    std::shared_ptr<Core::VKShaderModule> hot_reload(const std::string& filepath);

    //==========================================================================
    // Configuration
    //==========================================================================

    /**
     * @brief Update compiler configuration
     * @param config New configuration
     *
     * Affects future compilations.
     * Does NOT recompile existing shaders.
     */
    void set_config(const ShaderCompilerConfig& config);

    /**
     * @brief Get current compiler configuration
     */
    [[nodiscard]] const ShaderCompilerConfig& get_config() const { return m_config; }

    /**
     * @brief Add include directory for shader compilation
     * @param directory Path to directory containing shader includes
     *
     * Used for #include resolution in GLSL files.
     */
    void add_include_directory(const std::string& directory);

    /**
     * @brief Add preprocessor define for shader compilation
     * @param name Macro name
     * @param value Macro value (optional)
     *
     * Example: define("DEBUG", "1") → #define DEBUG 1
     */
    void add_define(const std::string& name, const std::string& value = "");

    //==========================================================================
    // Introspection
    //==========================================================================

    /**
     * @brief Check if shader is cached
     * @param cache_key File path or cache key
     */
    [[nodiscard]] bool is_cached(const std::string& cache_key) const;

    /**
     * @brief Get all cached shader keys
     */
    [[nodiscard]] std::vector<std::string> get_cached_keys() const;

    /**
     * @brief Get number of cached shaders
     */
    [[nodiscard]] size_t get_cache_size() const { return m_shader_cache.size(); }

    //==========================================================================
    // Descriptor Set Management - ShaderFoundry allocates and tracks
    //==========================================================================

    /**
     * @brief Allocate descriptor set for a pipeline
     * @param pipeline_id Which pipeline this is for
     * @param set_index Which descriptor set (0, 1, 2...)
     * @return Descriptor set ID
     */
    DescriptorSetID allocate_descriptor_set(vk::DescriptorSetLayout layout);

    /**
     * @brief Update descriptor set with buffer binding
     * @param descriptor_set_id ID of descriptor set to update
     * @param binding Binding index within the descriptor set
     * @param type Descriptor type (e.g., eStorageBuffer, eUniformBuffer)
     * @param buffer Vulkan buffer to bind
     * @param offset Offset within the buffer
     * @param size Size of the buffer region
     */
    void update_descriptor_buffer(
        DescriptorSetID descriptor_set_id,
        uint32_t binding,
        vk::DescriptorType type,
        vk::Buffer buffer,
        size_t offset,
        size_t size);

    /**
     * @brief Update descriptor set with image binding
     * @param descriptor_set_id ID of descriptor set to update
     * @param binding Binding index within the descriptor set
     * @param image_view Vulkan image view to bind
     * @param sampler Vulkan sampler to bind
     * @param layout Image layout (default: eShaderReadOnlyOptimal)
     */
    void update_descriptor_image(
        DescriptorSetID descriptor_set_id,
        uint32_t binding,
        vk::ImageView image_view,
        vk::Sampler sampler,
        vk::ImageLayout layout = vk::ImageLayout::eShaderReadOnlyOptimal);

    /**
     * @brief Update descriptor set with storage image binding
     * @param descriptor_set_id ID of descriptor set to update
     * @param binding Binding index within the descriptor set
     * @param image_view Vulkan image view to bind
     * @param layout Image layout (default: eGeneral)
     */
    void update_descriptor_storage_image(
        DescriptorSetID descriptor_set_id,
        uint32_t binding,
        vk::ImageView image_view,
        vk::ImageLayout layout = vk::ImageLayout::eGeneral);

    /**
     * @brief Get Vulkan descriptor set handle from DescriptorSetID
     * @param descriptor_set_id Descriptor set ID
     * @return Vulkan descriptor set handle
     */
    vk::DescriptorSet get_descriptor_set(DescriptorSetID descriptor_set_id);

    //==========================================================================
    // Command Recording - ShaderFoundry manages command buffers
    //==========================================================================

    /**
     * @brief Begin recording command buffer
     * @param type Command buffer type (GRAPHICS, COMPUTE, TRANSFER)
     * @return Command buffer ID
     */
    CommandBufferID begin_commands(CommandBufferType type);

    /**
     * @brief Get Vulkan command buffer handle from CommandBufferID
     * @param cmd_id Command buffer ID
     */
    vk::CommandBuffer get_command_buffer(CommandBufferID cmd_id);

    //==========================================================================
    // Memory Barriers and Synchronization
    //==========================================================================

    /**
     * @brief Submit command buffer and wait for completion
     * @param cmd_id Command buffer ID to submit
     */
    void submit_and_wait(CommandBufferID cmd_id);

    /**
     * @brief Submit command buffer asynchronously, returning a fence
     * @param cmd_id Command buffer ID to submit
     * @return Fence ID to wait on later
     */
    FenceID submit_async(CommandBufferID cmd_id);

    /**
     * @brief Submit command buffer asynchronously, returning a semaphore
     * @param cmd_id Command buffer ID to submit
     * @return Semaphore ID to wait on later
     */
    SemaphoreID submit_with_signal(CommandBufferID cmd_id);

    /**
     * @brief Wait for fence to be signaled
     * @param fence_id Fence ID to wait on
     */
    void wait_for_fence(FenceID fence_id);

    /**
     * @brief Wait for multiple fences to be signaled
     * @param fence_ids Vector of fence IDs to wait on
     */
    void wait_for_fences(const std::vector<FenceID>& fence_ids);

    /**
     * @brief Check if fence is signaled
     * @param fence_id Fence ID to check
     * @return True if fence is signaled, false otherwise
     */
    bool is_fence_signaled(FenceID fence_id);

    /**
     * @brief Begin command buffer that waits on a semaphore
     * @param type Command buffer type (GRAPHICS, COMPUTE, TRANSFER)
     * @param wait_semaphore Semaphore ID to wait on
     * @param wait_stage Pipeline stage to wait at
     * @return Command buffer ID
     */
    CommandBufferID begin_commands_with_wait(
        CommandBufferType type,
        SemaphoreID wait_semaphore,
        vk::PipelineStageFlags wait_stage);

    /**
     * @brief Get Vulkan fence handle from FenceID
     * @param fence_id Fence ID
     */
    vk::Semaphore get_semaphore_handle(SemaphoreID semaphore_id);

    /**
     * @brief Insert buffer memory barrier
     */
    void buffer_barrier(
        CommandBufferID cmd_id,
        vk::Buffer buffer,
        vk::AccessFlags src_access,
        vk::AccessFlags dst_access,
        vk::PipelineStageFlags src_stage,
        vk::PipelineStageFlags dst_stage);

    /**
     * @brief Insert image memory barrier
     */
    void image_barrier(
        CommandBufferID cmd_id,
        vk::Image image,
        vk::ImageLayout old_layout,
        vk::ImageLayout new_layout,
        vk::AccessFlags src_access,
        vk::AccessFlags dst_access,
        vk::PipelineStageFlags src_stage,
        vk::PipelineStageFlags dst_stage);

    //==========================================================================
    // Queue Management
    //==========================================================================

    /**
     * @brief Set Vulkan queues for command submission
     */
    void set_graphics_queue(vk::Queue queue);

    /**
     * @brief Set Vulkan queues for command submission
     */
    void set_compute_queue(vk::Queue queue);

    /**
     * @brief Set Vulkan queues for command submission
     */
    void set_transfer_queue(vk::Queue queue);

    /**
     * @brief Get Vulkan graphics queue
     */
    [[nodiscard]] vk::Queue get_graphics_queue() const;

    /**
     * @brief Get Vulkan compute queue
     */
    [[nodiscard]] vk::Queue get_compute_queue() const;

    /**
     * @brief Get Vulkan transfer queue
     */
    [[nodiscard]] vk::Queue get_transfer_queue() const;

    //==========================================================================
    // Profiling
    //==========================================================================

    void begin_timestamp(CommandBufferID cmd_id, const std::string& label = "");
    void end_timestamp(CommandBufferID cmd_id, const std::string& label = "");

    struct TimestampResult {
        std::string label;
        uint64_t duration_ns;
        bool valid;
    };

    TimestampResult get_timestamp_result(CommandBufferID cmd_id, const std::string& label);

    //==========================================================================
    // Utilities
    //==========================================================================

    /**
     * @brief Convert Portal ShaderStage to Vulkan ShaderStageFlagBits
     */
    static vk::ShaderStageFlagBits to_vulkan_stage(ShaderStage stage);

    /**
     * @brief Auto-detect shader stage from file extension
     * @param filepath Path to shader file
     * @return Detected stage, or nullopt if unknown
     *
     * Delegates to VKShaderModule::detect_stage_from_extension().
     */
    static std::optional<ShaderStage> detect_stage_from_extension(const std::string& filepath);

private:
    ShaderFoundry() = default;
    ~ShaderFoundry() { shutdown(); }

    std::shared_ptr<Core::VulkanBackend> m_backend;
    ShaderCompilerConfig m_config;

    std::unordered_map<std::string, std::shared_ptr<Core::VKShaderModule>> m_shader_cache;
    std::unordered_map<ShaderID, ShaderState> m_shaders;
    std::unordered_map<std::string, ShaderID> m_shader_filepath_cache;

    std::shared_ptr<Core::VKDescriptorManager> m_global_descriptor_manager;
    std::unordered_map<DescriptorSetID, DescriptorSetState> m_descriptor_sets;

    std::unordered_map<CommandBufferID, CommandBufferState> m_command_buffers;
    std::unordered_map<FenceID, FenceState> m_fences;
    std::unordered_map<SemaphoreID, SemaphoreState> m_semaphores;

    vk::Queue m_graphics_queue;
    vk::Queue m_compute_queue;
    vk::Queue m_transfer_queue;

    std::atomic<uint64_t> m_next_shader_id { 1 };
    std::atomic<uint64_t> m_next_descriptor_set_id { 1 };
    std::atomic<uint64_t> m_next_command_id { 1 };
    std::atomic<uint64_t> m_next_fence_id { 1 };
    std::atomic<uint64_t> m_next_semaphore_id { 1 };

    std::shared_ptr<Core::VKShaderModule> create_shader_module();
    vk::Device get_device() const;

    //==========================================================================
    // INTERNAL Shader Compilation Methods
    //==========================================================================

    std::shared_ptr<Core::VKShaderModule> compile_from_file(
        const std::string& filepath,
        std::optional<ShaderStage> stage = std::nullopt,
        const std::string& entry_point = "main");

    std::shared_ptr<Core::VKShaderModule> compile_from_source(
        const std::string& source,
        ShaderStage stage,
        const std::string& entry_point = "main");

    std::shared_ptr<Core::VKShaderModule> compile_from_source_cached(
        const std::string& source,
        ShaderStage stage,
        const std::string& cache_key,
        const std::string& entry_point = "main");

    std::shared_ptr<Core::VKShaderModule> compile_from_spirv(
        const std::string& spirv_path,
        ShaderStage stage,
        const std::string& entry_point = "main");

    std::shared_ptr<Core::VKShaderModule> get_vk_shader_module(ShaderID shader_id);

    friend class ComputePress;
    friend class RenderFlow;
};

/**
 * @brief Get the global shader compiler instance
 * @return Reference to singleton shader compiler
 *
 * Must call initialize() before first use.
 * Thread-safe after initialization.
 */
inline MAYAFLUX_API ShaderFoundry& get_shader_foundry()
{
    return ShaderFoundry::instance();
}

} // namespace MayaFlux::Portal::Graphics
